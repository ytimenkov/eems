#include "server.h"

#include "fs.h"
#include "http_messages.h"
#include "ranges.h"
#include "upnp.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <fmt/ostream.h>
#include <range/v3/algorithm/find_if_not.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/iterator.hpp>
#include <spdlog/spdlog.h>

namespace eems
{

auto parse_target(std::string_view target) -> std::pair<fs::path, std::string_view>
{

    if (auto const query_pos = target.find('?'); query_pos != target.npos)
    {
        auto const path = target.substr(0, query_pos);
        target.remove_prefix(query_pos);
        return {{path, fs::path::format::generic_format}, target};
    }
    return {{target, fs::path::format::generic_format}, {}};
}

auto server::handle_connections(net::ip::tcp::socket socket) -> net::awaitable<void>
{
    try
    {
        auto stream = tcp_stream{std::move(socket)};

        auto buffer = beast::flat_buffer{};
        for (;;)
        {
            auto req = http_request{};
            stream.expires_after(std::chrono::seconds(30));
            co_await http::async_read(stream, buffer, req);

            spdlog::debug("Got request: {}", req.target());

            auto [path, query] = parse_target(req.target());

            auto begin = ranges::find_if_not(path, &fs::path::has_root_directory);

            if (begin == ranges::end(path))
            {
                // Serve index.
                co_await http::async_write(stream, *make_error_response(http::status::ok, "<html><body>Welcome!</body></html>", req));
                continue;
            }

            spdlog::debug("Shall dispatch: {}", *begin);
            auto sub_path = fs::path{};
            for (auto it = ranges::next(begin); it != ranges::end(path); ++it)
            {
                sub_path /= *it;
            }
            // If we need to send an error back because of exception, store it separately,
            // because coroutine can't be suspended in catch() caluse (due to the active exception).
            error_response_ptr exceptional_error;
            try
            {
                if (begin->native() == "upnp")
                {
                    co_await upnp_service_.handle_upnp_request(stream, std::move(req), std::move(sub_path));
                    continue;
                }
                else if (begin->native() == "content")
                {
                    co_await content_service_.handle_request(stream, std::move(req), std::move(sub_path));
                    continue;
                }
                else
                {
                    spdlog::debug("Not found: {}", *begin);
                    exceptional_error = make_error_response(http::status::not_found, "Not found", req);
                }
            }
            catch (http_error const& e)
            {
                spdlog::warn("Request {} failed: {}, {}", req.target(), e.status, e.what());
                exceptional_error = make_error_response(e.status, e.what(), req);
            }

            if (exceptional_error)
            {
                co_await http::async_write(stream, *exceptional_error);
            }
        }
    }
    catch (boost::system::system_error& e)
    {
        // It's ok when client closes connection.
        if (e.code() != http::error::end_of_stream)
        {
            spdlog::warn("Exception during handling request: {}", e.what());
        }
    }
    catch (std::exception& e)
    {
        spdlog::warn("Exception during handling request: {}", e.what());
    }
}

auto server::run_server() -> net::awaitable<void>
{
    auto executor = co_await net::this_coro::executor;
    auto acceptor = net::ip::tcp::acceptor{executor, {net::ip::tcp::v4(), 8000}};

    spdlog::info("Server listening on http://{}\n", acceptor.local_endpoint());

    for (;;)
    {
        auto socket = co_await acceptor.async_accept(net::use_awaitable);
        net::co_spawn(executor, handle_connections(std::move(socket)), net::detached);
    }
}
}
