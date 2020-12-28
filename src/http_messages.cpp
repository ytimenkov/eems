#include "http_messages.h"

#include <boost/beast/version.hpp>

namespace eems
{
auto make_error_response(http::status code, std::string_view reason, http_request const& req)
    -> error_response_ptr
{
    auto res = std::make_unique<error_response_ptr::element_type>(code, req.version());
    res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res->set(http::field::content_type, "text/html");
    res->keep_alive(req.keep_alive());
    res->body().commit(net::buffer_copy(
        res->body().prepare(reason.size()),
        net::const_buffer{reason.data(), reason.size()}));
    res->prepare_payload();
    return res;
}
}
