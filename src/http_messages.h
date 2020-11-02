#ifndef EEMS_HTTP_MESSAGES_H
#define EEMS_HTTP_MESSAGES_H

#include "net.h"

#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/basic_dynamic_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

namespace eems
{
using http_request = http::request<http::basic_dynamic_body<beast::flat_buffer>>;
using tcp_stream = net::use_awaitable_t<>::as_default_on_t<beast::tcp_stream>;

class http_error : public std::runtime_error
{
public:
    http_error(http::status status, char const* message)
        : runtime_error{message},
          status{status}
    {
    }

    http::status status;
};

auto make_error_response(http::status, std::string_view reason, http_request const& req)
    -> http::response<http::string_body>;

}
#endif
