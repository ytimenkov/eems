#include "upnp.h"

#include "xml_serialization.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

namespace eems
{
auto handle_upnp_request(net::use_awaitable_t<>::as_default_on_t<beast::tcp_stream>& stream,
                         http::header<true, http::fields> const& req,
                         fs::path::iterator begin, fs::path::iterator end)
    -> net::awaitable<void>
{
    auto doc = root_device_description("http://localhost:8000");

    http::response<http::buffer_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/xml");

    auto const  buf = doc.data();
    res.body().data = buf.data();
    res.body().size = buf.size();
    res.body().more = false;
    res.prepare_payload();

    co_await http::async_write(stream, res);
}
} // namespace eems
