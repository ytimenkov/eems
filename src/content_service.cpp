#include "content_service.h"

#include "spirit.h"

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
    int64_t resource_id;
    if (!parse(std::basic_string_view{sub_path.c_str()}, x3::int64, resource_id))
    {
        co_await http::async_write(stream, *make_error_response(http::status::not_found, sub_path.c_str(), req));
        co_return;
    }

    auto [resource, res_buf] = store_service_.get_resource(resource_id);
    if (!resource || !resource->location())
    {
        co_await http::async_write(stream, *make_error_response(http::status::not_found, sub_path.c_str(), req));
        co_return;
    }
    auto location = read_path(*resource->location());

    spdlog::debug("Serving {} (from {})", sub_path, location);

    auto res = http::response<http::file_body>{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    if (auto mime_type = resource->mime_type(); mime_type)
    {
        res.set(http::field::content_type, get_string_view(*mime_type));
    }
    res.keep_alive(req.keep_alive());
    beast::error_code ec;
    res.body().open(location, beast::file_mode::scan, ec);

    // TODO: Or use exception?..
    if (ec == beast::errc::no_such_file_or_directory)
    {
        co_await http::async_write(stream, *make_error_response(http::status::not_found, sub_path.c_str(), req));
        co_return;
    }
    else if (ec)
    {
        co_await http::async_write(stream, *make_error_response(http::status::internal_server_error, ec.message(), req));
        co_return;
    }

    res.prepare_payload();

    co_await http::async_write(stream, res);
}

}
