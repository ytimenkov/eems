#include "discovery_service.h"

#include "http_serialize.h"

#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

namespace eems
{
using udp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::udp::socket>;
// TODO: ipv6
// [FF02::C] (IPv6 link-local)
// [FF05::C] (IPv6 site-local)

auto discovery_service::run_service()
    -> net::awaitable<void>
{
    auto executor = co_await net::this_coro::executor;
    udp_socket mc_socket{executor, net::ip::udp::v4()};

    mc_socket.set_option(net::ip::udp::socket::reuse_address{true});
    mc_socket.bind({net::ip::udp::v4(), 1900});

    mc_socket.set_option(
        net::ip::multicast::join_group(
            net::ip::make_address_v4(net::ip::address_v4::bytes_type{239, 255, 255, 250})));
    mc_socket.set_option(net::ip::multicast::hops{4});

    net::ip::udp::endpoint sender_endpoint{};
    beast::flat_buffer buffer{};

    for (;;)
    {
        buffer.consume(buffer.size());
        auto received_bytes = co_await mc_socket.async_receive_from(buffer.prepare(1500), sender_endpoint);
        buffer.commit(received_bytes);
        spdlog::info("Received discovery request: {}", beast::make_printable(buffer.data()));
        if (auto response = handle_request(deserialize_request<http::empty_body>(buffer)); response)
        {
            buffer.consume(buffer.size());
            serialize(buffer, *response);
            spdlog::info("Sending discovery response: {}", beast::make_printable(buffer.data()));
            co_await mc_socket.async_send_to(buffer.data(), sender_endpoint);
        }
        else
        {
            spdlog::info("Ignored request");
        }
    }
}

auto discovery_service::handle_request(http::request<http::empty_body>&& req)
    -> std::optional<http::response<http::empty_body>>
{
    if (req.method() != http::verb::msearch)
    {
        spdlog::info("Method {} is not search", req.method_string());
        return std::nullopt;
    }
    if (req.target() != "*")
    {
        spdlog::info("Target {} is not *", req.target());
        return std::nullopt;
    }
    if (req[http::field::man] != "\"ssdp:discover\"")
    {
        spdlog::info("man {} is not ssdp:discover", req[http::field::man]);
        return std::nullopt;
    }
    // TODO: Check host
    auto st = req["st"];
    if (st != "upnp:rootdevice" && st != "urn:schemas-upnp-org:device:MediaServer:1")
    {
        spdlog::info("st {} is not supported", st);
        return std::nullopt;
    }

    http::response<http::empty_body> response(http::status::ok, req.version());
    response.set(http::field::cache_control, fmt::format("max-age={}", 1800)); // TODO: derive from notify interval
    response.set(http::field::location, "http://localhost:8000/device");       // TODO: UPNP device request.
    response.set("st", st);
    response.set("usn", fmt::format("uuid:{}:{}", to_string(config_.uuid), st));

    return response;
}

}
