#include "upnp.h"

#include "soap.h"
#include "spirit.h"
#include "store/fb_converters.h"
#include "xml_serialization.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <spdlog/spdlog.h>

namespace eems
{
enum class browse_flag
{
    metadata,
    direct_children,
};

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

auto upnp_service::handle_cds_browse(tcp_stream& stream, http_request&& req, soap_action_info const& soap_req)
    -> net::awaitable<void>
{
    // TODO: There are filter and search criteria and start index / count
    // TODO: Also returns dumb NumberReturned value and TotalMatches and UpdateID

    int64_t parent_id;
    if (auto const id = soap_req.params.child_value("ObjectID"); !parse(std::string_view{id}, x3::int64, parent_id))
    {
        // TODO: find out proper exception type.
        throw std::runtime_error(fmt::format("Invalid id: {}", id));
    }

#if 0
    auto const browse_flag = [flag = std::string_view{soap_req.params.child_value("BrowseFlag")}]() {
        if (flag == "BrowseDirectChildren")
            return browse_flag::direct_children;
        else if (flag == "BrowseMetadata")
            return browse_flag::metadata;
        else
            // TODO: Here we can send SOAP error instead. Fault or so...
            throw http_error{http::status::bad_request, "Invalid BrowseFlag"};
    }();
#endif

    auto content_base = "http://localhost:8000/content/";

    auto [didl_doc, didl_root] = generate_preamble("DIDL-Lite", "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
    didl_root.append_attribute("xmlns:upnp").set_value("urn:schemas-upnp-org:metadata-1-0/upnp/");
    didl_root.append_attribute("xmlns:dc").set_value("http://purl.org/dc/elements/1.1/");

    auto contents = store_service_.list(ContainerKey{parent_id});
    auto count = ranges::accumulate(contents, std::size_t{0}, [&didl_root, content_base](std::size_t count, MediaItem const& item) {
        auto node = didl_root.append_child("item");
        serialize_common_fields(node, item);

        if (auto resources = item.resources(); resources)
        {
            ranges::for_each(*resources, [&node, &item, content_base](ResourceRef const* r) {
                if (auto res_key = r->ref_nested_root()->key_as_ResourceKey(); res_key)
                {
                    auto res = node.append_child("res");
                    res.append_attribute("protocolInfo").set_value(as_cstring<char>(*r->protocol_info()));
                    res.text().set(fmt::format("{}{}", content_base, res_key->id()).c_str());
                }
                else
                {
                    spdlog::error("Resource ref has no valid key: {}", item.id()->id());
                }
            });
        }
        return ++count;
    });

    co_await respond_with_buffer(stream, req, browse_response(didl_doc, count), "text/xml");
}

auto upnp_service::handle_upnp_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
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
