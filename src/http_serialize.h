#ifndef EEMS_HTTP_SERIALIZE_H
#define EEMS_HTTP_SERIALIZE_H

#include "net.h"

#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/serializer.hpp>

namespace eems
{

template <bool isRequest, typename Body, typename Fields, typename DynamicBuffer>
void serialize(DynamicBuffer& buffer, http::message<isRequest, Body, Fields> const& m)
{
    beast::error_code ec;
    http::serializer<isRequest, Body, Fields> sr{m};
    do
    {
        sr.next(ec, [&sr, &buffer](beast::error_code& ec, auto const& read_buf) {
            ec = {};
            auto write_buf = buffer.prepare(beast::buffer_bytes(read_buf));
            auto const copied = net::buffer_copy(write_buf, read_buf);
            sr.consume(copied);
            buffer.commit(copied);
        });
    } while (!ec && !sr.is_done());
    if (ec)
    {
        throw beast::system_error{ec};
    }
}

template <typename Body, typename DynamicBuffer>
auto deserialize_request(DynamicBuffer& buffer)
{
    http::request_parser<Body> parser{};
    parser.eager(true);
    beast::error_code ec;

    do
    {
        auto const bytes_used = parser.put(buffer.data(), ec);
        if (ec)
        {
            throw beast::system_error(ec, "Invalid HTTP Request");
        }
        buffer.consume(bytes_used);
    } while (!parser.is_done());
    return parser.release();
}

}
#endif
