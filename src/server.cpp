#include "server.h"

#include "fs.h"
#include "upnp.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <fmt/ostream.h>
#include <range/v3/algorithm/find_if_not.hpp>
#include <range/v3/begin_end.hpp>
#include <skyr/url.hpp> // TODO: Just run spirit on the query where needed
#include <spdlog/spdlog.h>

namespace ranges = ::ranges;

namespace eems
{

template <typename Request>
auto bad_request(std::string_view reason, Request const& req)
{
    http::response<http::string_body> res{http::status::bad_request, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = reason;
    res.prepare_payload();
    return res;
}

auto parse_target(std::string_view target) -> std::pair<fs::path, skyr::url_search_parameters>
{

    if (auto const query_pos = target.find('?'); query_pos != target.npos)
    {
        auto const path = target.substr(0, query_pos);
        target.remove_prefix(query_pos);
        return {{path, fs::path::format::generic_format}, skyr::url_search_parameters{target}};
    }
    return {{target, fs::path::format::generic_format}, {}};
}

auto process_request()
{
}

auto handle_connections(net::ip::tcp::socket socket) -> net::awaitable<void>
{
    try
    {
        auto stream{net::use_awaitable_t<>::as_default_on_t<beast::tcp_stream>{std::move(socket)}};

        beast::flat_buffer buffer;
        for (;;)
        {
            http::request<http::string_body> req;
            stream.expires_after(std::chrono::seconds(30));
            co_await http::async_read(stream, buffer, req);

            spdlog::debug("Got request: {}", req.target());

            auto [path, query] = parse_target(req.target());

            if (auto begin = ranges::find_if_not(path, &fs::path::has_root_directory);
                begin != ranges::end(path))
            {
                spdlog::debug("Shall dispatch: {}", *begin);
                if (begin->native() == "upnp")
                {
                    co_await handle_upnp_request(stream, req, ++begin, ranges::end(path));
                    continue;
                }
            }

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = std::string("<html><body>Welcome!</body></html>");
            res.prepare_payload();

            co_await http::async_write(stream, res);
        }
    }
    catch (std::exception& e)
    {
        spdlog::warn("Exception during handling request: {}", e.what());
    }
}

auto run_server() -> net::awaitable<void>
{
    auto executor = co_await net::this_coro::executor;
    net::ip::tcp::acceptor acceptor{executor, {net::ip::tcp::v4(), 8000}};

    spdlog::info("Server listening on http://{}\n", acceptor.local_endpoint());

    for (;;)
    {
        auto socket = co_await acceptor.async_accept(net::use_awaitable);
        net::co_spawn(executor, handle_connections(std::move(socket)), net::detached);
    }
}
} // namespace eems
