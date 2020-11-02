#include "content_service.h"

// TODO: Maybe this dependency is not needed...
#include "directory_service.h"

#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

namespace eems
{
auto content_service::handle_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
    -> net::awaitable<void>
{
    auto const file_path = fs::path{"/"} / sub_path;
    spdlog::debug("Serving {} (from {})", sub_path, file_path);

    auto res = http::response<http::file_body>{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, *get_mime_type(file_path));
    res.keep_alive(req.keep_alive());
    beast::error_code ec;
    res.body().open(file_path.c_str(), beast::file_mode::scan, ec);

    if (ec == beast::errc::no_such_file_or_directory)
    {
        co_await http::async_write(stream, make_error_response(http::status::not_found, sub_path.c_str(), req));
        co_return;
    }
    else if (ec)
    {
        co_await http::async_write(stream, make_error_response(http::status::internal_server_error, ec.message(), req));
        co_return;
    }

    res.prepare_payload();

    co_await http::async_write(stream, res);
}

}
