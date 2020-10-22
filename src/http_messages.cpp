#include "http_messages.h"

#include <boost/beast/version.hpp>

namespace eems
{
auto make_error_response(http::status code, std::string_view reason, http_request const& req)
    -> http::response<http::string_body>
{
    auto res = http::response<http::string_body>{code, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = reason;
    res.prepare_payload();
    return res;
}
}
