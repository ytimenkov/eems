#include "upnp.h"

#include "cds.xml"
#include "cm.xml"
#include "soap.h"
#include "spirit.h"
#include "store/fb_converters.h"
#include "xml_serialization.h"

#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <spdlog/spdlog.h>

namespace eems
{
enum class browse_flag
{
    metadata,
    direct_children,
};

auto create_buffer_response(http_request const& req,
                            boost::asio::const_buffer buffer, std::string_view mime_type)
    -> http::response<http::buffer_body>
{
    auto response = http::response<http::buffer_body>{
        std::piecewise_construct,
        std::make_tuple(const_cast<void*>(buffer.data()), buffer.size(), false), // TODO: It's a pity that bufer_body is always non-const.
        std::make_tuple(http::status::ok, req.version())};
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, mime_type);

    response.content_length(buffer.size());
    response.keep_alive(req.keep_alive());

    return response;
}

inline auto make_string_buffer(char const* str)
    -> boost::asio::const_buffer
{
    return {str, std::strlen(str)};
}

auto upnp_service::handle_cds_browse(tcp_stream& stream, http_request&& req, soap_action_info const& soap_req)
    -> net::awaitable<void>
{
    // TODO: There are filter and search criteria and start index / count
    // TODO: Also returns dumb NumberReturned value and TotalMatches and UpdateID

    int64_t object_id;
    if (auto const id = soap_req.params.child_value("ObjectID"); !parse(std::string_view{id}, x3::int64, object_id))
    {
        // TODO: find out proper exception type.
        throw std::runtime_error(fmt::format("Invalid id: {}", id));
    }

    auto contents = [flag = std::string_view{soap_req.params.child_value("BrowseFlag")},
                     key = ObjectKey{object_id},
                     &store_service = store_service_]() -> store_service::list_result_view {
        if (flag == "BrowseDirectChildren")
            return store_service.list(key);
        else if (flag == "BrowseMetadata")
            return store_service.get(key);
        else
            // TODO: Here we can send SOAP error instead. Fault or so...
            throw http_error{http::status::bad_request, "Invalid BrowseFlag"};
    }();

    co_await http::async_write(
        stream, create_buffer_response(
                    req, browse_response(std::move(contents), server_config_.base_url).cdata(), "text/xml"));
}

auto upnp_service::handle_upnp_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
    -> net::awaitable<void>
{
    if (sub_path.native() == "device")
    {
        co_await http::async_write(stream, create_buffer_response(req, root_device_description(server_config_).cdata(), "text/xml"));
        co_return;
    }
    else if (sub_path.native() == "cds.xml")
    {
        co_await http::async_write(stream, create_buffer_response(req, make_string_buffer(cds_xml), "text/xml"));
        co_return;
    }
    else if (sub_path.native() == "cm.xml")
    {
        co_await http::async_write(stream, create_buffer_response(req, make_string_buffer(cm_xml), "text/xml"));
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
