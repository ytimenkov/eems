#include "upnp.h"

#include "directory_service.h"
#include "soap.h"
#include "xml_serialization.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <spdlog/spdlog.h>

namespace eems
{
auto respond_with_buffer(tcp_stream& stream, http_request const& req,
                         beast::flat_buffer buffer, std::string_view mime_type)
    -> net::awaitable<void>
{
    auto const buf = buffer.data();
    auto res = http::response<http::buffer_body>{
        std::piecewise_construct,
        std::make_tuple(buf.data(), buf.size(), false),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type);

    res.content_length(buf.size());

    co_await http::async_write(stream, res);
}

auto handle_cds_browse(tcp_stream& stream, http_request&& req, soap_action_info const& soap_req)
    -> net::awaitable<void>
{
    auto object_id = soap_req.params.child_value("ObjectID");
    auto const browse_flag = [flag = std::string_view{soap_req.params.child_value("BrowseFlag")}]() {
        if (flag == "BrowseDirectChildren")
            return browse_flag::direct_children;
        else if (flag == "BrowseMetadata")
            return browse_flag::metadata;
        else
            // TODO: Here we can send SOAP error instead. Fault or so...
            throw http_error{http::status::bad_request, "Invalid BrowseFlag"};
    }();
    auto directory = directory_service{};
    auto results = directory.browse(object_id, browse_flag);

    co_await respond_with_buffer(stream, req, list_response(results), "text/xml");
}

auto handle_upnp_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
    -> net::awaitable<void>
{
    if (sub_path.native() == "device")
    {
        co_await respond_with_buffer(stream, req, root_device_description("http://localhost:8000"), "text/xml");
        co_return;
    }
    auto soap_info = parse_soap_request(req);

    if (sub_path.native() == "cds")
    {
        if (soap_info.action != "Browse")
        {
            // TODO: Here we can send SOAP error instead. Fault or so...
            throw http_error{http::status::bad_request, "Invalid action"};
        }
        co_await handle_cds_browse(stream, std::move(req), soap_info);
        co_return;
    }
    throw http_error{http::status::not_found, "Not found"};
}

}
