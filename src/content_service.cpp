#include "content_service.h"

#include "as_result.h"
#include "spirit.h"
#include "store/fb_converters.h"

#include <boost/asio/experimental/as_single.hpp>
#include <boost/beast/core/file.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/include/std_tuple.hpp>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

namespace boost::beast::http
{
template <bool isRequest, class Body, class Fields>
serializer(message<isRequest, Body, Fields> const&) -> serializer<isRequest, Body, Fields>;
}

namespace eems
{

auto content_service::create_response(fs::path const& sub_path, http_request const& req)
    -> std::tuple<http::response<http::buffer_body>, beast::file, std::uintmax_t>
{
    int64_t resource_id;
    if (!parse(std::basic_string_view{sub_path.c_str()}, x3::int64, resource_id))
    {
        throw http_error{http::status::not_found, sub_path.c_str()};
    }

    std::tuple<std::optional<std::uintmax_t>, std::optional<std::uintmax_t>> request_range;

    auto range_not_satisfiable = []() {
        // TODO: A server generating a 416 (Range Not Satisfiable) response to a
        //       byte-range request SHOULD send a Content-Range header field with an
        //       unsatisfied-range value, as in the following example:
        //         Content-Range: bytes */1234
        throw http_error{http::status::range_not_satisfiable, ""};
    };

    if (auto range_header = req[http::field::range]; !range_header.empty())
    {
        constexpr x3::uint_parser<std::uintmax_t> uint_max;
        if (!parse(range_header, ("bytes=" >> -uint_max >> '-' >> -uint_max), request_range))
        {
            range_not_satisfiable();
        }
    }

    auto [resource, res_buf] = store_service_.get_resource(resource_id);
    if (!resource || !resource->location())
    {
        throw http_error{http::status::not_found, sub_path.c_str()};
    }

    auto location = as_cstring<fs::path::value_type>(*resource->location());
    spdlog::debug("Serving {} (from {})", sub_path, location);

    std::tuple<http::response<http::buffer_body>, beast::file, std::uintmax_t> result{};

    auto& [resp, file, size] = result;
    beast::error_code ec;

    auto internal_server_error = [&ec]() {
        throw http_error{http::status::internal_server_error, ec.message().c_str()};
    };

    file.open(location, beast::file_mode::scan, ec);
    if (ec == beast::errc::no_such_file_or_directory)
    {
        throw http_error{http::status::not_found, sub_path.c_str()};
    }
    else if (ec)
    {
        internal_server_error();
    }

    resp.version(req.version());
    resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    resp.set(http::field::accept_ranges, "bytes");
    resp.keep_alive(req.keep_alive());

    size = file.size(ec);
    if (ec)
    {
        internal_server_error();
    }
    if (get<0>(request_range) || get<1>(request_range))
    {
        resp.result(http::status::partial_content);

        auto& [first, last] = request_range;
        if (first && last && *first > *last)
        {
            range_not_satisfiable();
        }

        if (first && *first >= size)
            range_not_satisfiable();

        if (!last)
        {
            // Request for last bytes
            last = size - 1;
        }
        if (!first)
        {
            // Request till the end
            *last = std::min(size, *last);
            first = size - *last;
        }
        resp.set(http::field::content_range, fmt::format("bytes {}-{}/{}", *first, *last, size));
        size = *last - *first + 1;
        file.seek(*first, ec);
        if (ec)
        {
            internal_server_error();
        }
    }
    resp.content_length(size);

    return result;
}

auto content_service::handle_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
    -> net::awaitable<bool>
{
    auto [response, file, size] = create_response(sub_path, req);

    {
        http::serializer sr{response};

        co_await http::async_write_header(stream, sr);
        // Don't use serializer because it throws need buffer exception (in co_await).
    }
    if (req.method() == http::verb::head)
    {
        // No need to send body for HEAD request.
        co_return true;
    }
    // Reuse request's buffer.
    auto& buffer = req.body();
    buffer.consume(buffer.size());

    // TODO: Look at llfio's and its reading...
    beast::error_code ec;
    do
    {
        auto produce_buffer = buffer.prepare(std::min<std::uintmax_t>(size, 4096u));
        if (!produce_buffer.size())
        {
            break;
        }
        auto const read_bytes = file.read(produce_buffer.data(), produce_buffer.size(), ec);
        if (ec)
        {
            spdlog::error("Read failed: {}", ec);
            stream.close();
            co_return false;
        }
        buffer.commit(read_bytes);

        auto const written_bytes = co_await net::async_write(stream, buffer.data(),
                                                   as_result(net::use_awaitable)); // TODO: use_async_result
        if (!written_bytes)
        {
            co_return false;
        }
        buffer.consume(written_bytes.value());
        size -= written_bytes.value();
    } while (true);
    co_return true;
}

}
