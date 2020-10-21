#include "logging.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using net::ip::tcp;

namespace eems
{
net::awaitable<void> echo(tcp::socket socket)
{
    try
    {
        beast::tcp_stream stream{std::move(socket)};
        stream.expires_after(std::chrono::seconds(30));
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        for (;;)
        {
            auto const bytes_transferred = co_await http::async_read(stream, buffer, req, net::use_awaitable);
            fmt::print("Read {}, target: {}\n", bytes_transferred, req.target());

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = std::string("<html><body>Welcome!</body></html>");
            res.prepare_payload();
            auto const bytes_written = co_await http::async_write(stream, res, net::use_awaitable);
            fmt::print("Sent {} bytes\n", bytes_written);
        }
    }
    catch (std::exception& e)
    {
        std::printf("echo Exception: %s\n", e.what());
    }
}

net::awaitable<void> listener(logger_sp logger)
{
    auto executor = co_await net::this_coro::executor;
    tcp::acceptor acceptor{executor, {tcp::v4(), 8080}};

    logger->info("Server listening on http://{}\n", acceptor.local_endpoint());

    for (;;)
    {
        tcp::socket socket = co_await acceptor.async_accept(net::use_awaitable);
        net::co_spawn(executor, echo(std::move(socket)), net::detached);
    }
}
} // namespace eems

int main()
{
    // TODO: Configure log file name
    auto logger = eems::intialize_logging("eems.log");
    net::io_context io_context{1};

    net::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    co_spawn(io_context, eems::listener(logger), net::detached);

    io_context.run();

    return 0;
}
