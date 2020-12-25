#include "discovery_service.h"

#include "http_serialize.h"

#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fmt/ostream.h>

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
        if (auto response = handle_request(deserialize_request<http::empty_body>(buffer)); response)
        {
            buffer.consume(buffer.size());
            serialize(buffer, *response);
            co_await mc_socket.async_send_to(buffer.data(), sender_endpoint);
        }
    }
}

auto discovery_service::handle_request(http::request<http::empty_body>&& req)
    -> std::optional<http::response<http::empty_body>>
{
    if (req.method() != http::verb::msearch)
    {
        return std::nullopt;
    }
    if (req.target() != "*")
    {
        return std::nullopt;
    }
    if (req[http::field::man] != "\"ssdp:discover\"")
    {
        return std::nullopt;
    }
    // TODO: Check host
    auto st = req["st"];
    if (st != "upnp:rootdevice" && st != "urn:schemas-upnp-org:device:MediaServer:1")
    {
        return std::nullopt;
    }

    http::response<http::empty_body> response(http::status::ok, req.version());
    response.set(http::field::cache_control, fmt::format("max-age={}", 1800)); // TODO: derive from notify interval
    response.set(http::field::location, fmt::format("{}/upnp/device", config_.base_url));
    response.set("st", st);
    response.set("usn", fmt::format("uuid:{}::{}", to_string(config_.uuid), st));
    response.set("ext", std::string_view{});

    return response;
}

}
