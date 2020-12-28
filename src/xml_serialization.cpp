#include "xml_serialization.h"

#include "net.h"
#include "ranges.h"
#include "store/fb_converters.h"
#include "store/fb_vector_view.h"

#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <date/date.h>
#include <fmt/core.h>
#include <pugixml.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <spdlog/spdlog.h>

namespace eems
{

class buffer_writer : public pugi::xml_writer
{
public:
    explicit buffer_writer(beast::flat_buffer& buffer)
        : buffer{buffer}
    {
    }
    buffer_writer& operator=(buffer_writer&&) = delete;

    virtual void write(const void* data, size_t size)
    {
        auto buf = buffer.prepare(size);
        buffer.commit(net::buffer_copy(buf, net::const_buffer{data, size}));
    }

private:
    beast::flat_buffer& buffer;
};

inline auto append_null(beast::flat_buffer& buffer)
{
    auto buf = buffer.prepare(1);
    constexpr auto null_char = '\0';
    std::memcpy(buf.data(), &null_char, 1);
    buffer.commit(1);
}

auto generate_preamble(char const* root_element, char const* root_ns)
    -> std::tuple<pugi::xml_document, pugi::xml_node>
{
    auto result = std::tuple<pugi::xml_document, pugi::xml_node>{};
    auto& [doc, root] = result;

    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    root = doc.append_child(root_element);
    root.append_attribute("xmlns").set_value(root_ns);

    return result;
}

auto root_device_description(server_config const& server_config) -> beast::flat_buffer
{
    auto [doc, root] = generate_preamble("root", "urn:schemas-upnp-org:device-1-0");
    auto specVersion = root.append_child("specVersion");
    specVersion.append_child("major").text().set("1");
    specVersion.append_child("minor").text().set("0");

    root.append_child("URLBase").text().set(server_config.base_url.c_str());

    auto device = root.append_child("device");
    // TODO: Clean up duplication between upnp services and dsicovery
    device.append_child("deviceType").text().set("urn:schemas-upnp-org:device:MediaServer:1");
    device.append_child("friendlyName").text().set(fmt::format("EEMS at {}", server_config.host_name).c_str());
    device.append_child("UDN").text().set(fmt::format("uuid:{}", to_string(server_config.uuid)).c_str());

    {
        auto dlna_doc = device.append_child("dlna:X_DLNADOC");
        dlna_doc.append_attribute("xmlns:dlna").set_value("urn:schemas-dlna-org:device-1-0");
        dlna_doc.text().set("DMS-1.50");
    }

    auto service_list = device.append_child("serviceList");

    {
        auto service = service_list.append_child("service");
        service.append_child("serviceType").text().set("urn:schemas-upnp-org:service:ContentDirectory:1");
        service.append_child("serviceId").text().set("urn:upnp-org:serviceId:ContentDirectory");
        service.append_child("SCPDURL").text().set("upnp/cds.xml");
        service.append_child("controlURL").text().set("upnp/cds");
        service.append_child("eventSubURL").text().set("upnp/event/cds");
    }

    {
        auto service = service_list.append_child("service");
        service.append_child("serviceType").text().set("urn:schemas-upnp-org:service:ConnectionManager:1");
        service.append_child("serviceId").text().set("urn:upnp-org:serviceId:ConnectionManager");
        service.append_child("SCPDURL").text().set("upnp/cm.xml");
        service.append_child("controlURL").text().set("upnp/cm");
        service.append_child("eventSubURL").text().set("upnp/event/cm");
    }

    beast::flat_buffer result;
    buffer_writer writer{result};

    doc.print(writer);

    return result;
}

auto serialize_media_object(pugi::xml_node& didl_root, std::string_view content_base, MediaObject const& object) -> bool
{
    auto resource_url = [content_base](int64_t id) {
        return fmt::format("{}/content/{}", content_base, id);
    };
    auto serialize_common_fields = [&object, resource_url](pugi::xml_node& node) {
        node.append_attribute("id").set_value(object.id()->id());
        node.append_attribute("parentID").set_value(object.parent_id()->id());
        node.append_attribute("restricted").set_value("1");

        node.append_child("dc:title").text().set(as_cstring<char>(*object.dc_title()));
        node.append_child("upnp:class").text().set(as_cstring<char>(*object.upnp_class()));
        if (auto days = object.dc_date(); days)
        {
            date::year_month_day const date{date::sys_days{std::chrono::days{days}}};
            node.append_child("dc:date").text().set(
                fmt::format("{:04}-{:02}-{:02}",
                            static_cast<int>(date.year()),
                            static_cast<unsigned>(date.month()),
                            static_cast<unsigned>(date.day()))
                    .c_str());
        }
        ranges::for_each(fb_vector_view{object.artwork()}, [&node, resource_url](Artwork const& aw) {
            auto const id = aw.ref_nested_root()->key_as_ResourceKey()->id();
            // TODO: Some set dlna:protocolInfo extension to JPEG_TN, but it seems to be ignored.
            auto const url = resource_url(id);
            node.append_child("upnp:albumArtURI").text().set(url.c_str());
            auto aw_node = node.append_child("xbmc:artwork");
            aw_node.text().set(url.c_str());
            switch (aw.type())
            {
            case ArtworkType::Poster:
                aw_node.append_attribute("type").set_value("poster");
                break;
            case ArtworkType::Thumbnail:
                aw_node.append_attribute("type").set_value("thumb");
            };
        });
    };

    switch (object.data_type())
    {
    case ObjectUnion::MediaItem:
    {
        auto& item = *static_cast<MediaItem const*>(object.data());
        auto node = didl_root.append_child("item");
        serialize_common_fields(node);
        ranges::for_each(fb_vector_view{item.resources()},
                         [&node, resource_url](ResourceRef const& r) {
                             if (auto res_key = r.ref_nested_root()->key_as_ResourceKey(); res_key)
                             {
                                 auto res = node.append_child("res");
                                 res.append_attribute("protocolInfo").set_value(as_cstring<char>(*r.protocol_info()));
                                 res.text().set(resource_url(res_key->id()).c_str());
                             }
                             else
                             {
                                 throw std::runtime_error{"Resource ref has no valid resource key"};
                             }
                         });
    }
    break;
    case ObjectUnion::MediaContainer:
    {
        // auto& container = *static_cast<MediaContainer const*>(object.data());
        auto node = didl_root.append_child("container");
        serialize_common_fields(node);
    }
    break;

    case ObjectUnion::NONE:
        spdlog::warn("Unknown object {} type: {}", object.id()->id(), object.data_type());
        return false;
    }
    return true;
}

inline auto add_soap_envelope(pugi::xml_document& xml_doc) -> pugi::xml_node
{
    auto soap_root = xml_doc.append_child("s:Envelope");
    soap_root.append_attribute("xmlns:s").set_value("http://schemas.xmlsoap.org/soap/envelope/");
    soap_root.append_attribute("s:encodingStyle").set_value("http://schemas.xmlsoap.org/soap/encoding/");
    return soap_root.append_child("s:Body");
}

auto browse_response(store_service::list_result_view const& list, std::string_view base_url) -> beast::flat_buffer
{
    auto [xml_doc, didl_root] = generate_preamble("DIDL-Lite", "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
    didl_root.append_attribute("xmlns:upnp").set_value("urn:schemas-upnp-org:metadata-1-0/upnp/");
    didl_root.append_attribute("xmlns:dc").set_value("http://purl.org/dc/elements/1.1/");
    didl_root.append_attribute("xmlns:xbmc").set_value("urn:schemas-xbmc-org:metadata-1-0/");

#if __INTELLISENSE__ == 1
    auto const count = 0;
    auto const size = 0;
#else
    auto const count = ranges::count_if(list, std::bind_front(&serialize_media_object, std::ref(didl_root), base_url));
    auto const size = ranges::size(list);
#endif

    beast::flat_buffer result;
    buffer_writer writer{result};

    xml_doc.print(writer);
    // Unfortunately PugiXML doesn't provide string_view-like interfaces, only C-strings...
    append_null(result);

    xml_doc.reset();
    auto soap_body = add_soap_envelope(xml_doc);

    // TODO: This is service type / action + Response
    auto response = soap_body.append_child("u:BrowseResponse");
    response.append_attribute("xmlns:u").set_value("urn:schemas-upnp-org:service:ContentDirectory:1");
    response.append_child("NumberReturned").text().set(count);
    response.append_child("TotalMatches").text().set(size);
    response.append_child("UpdateID").text().set("0");
    response.append_child("Result").text().set(static_cast<char const*>(result.data().data()));

    // Just reuse same writer / buffer...
    result.consume(result.size());
    xml_doc.print(writer);

    return result;
}

auto error_response(int code, char const* description) -> beast::flat_buffer
{
    pugi::xml_document xml_doc;

    auto soap_body = add_soap_envelope(xml_doc);
    auto fault = soap_body.append_child("s:Fault");
    fault.append_child("faultcode").text().set("s:Client");
    fault.append_child("faultstring").text().set("UPnPError");
    auto error = fault.append_child("detail").append_child("UPnPError");
    error.append_attribute("xmlns").set_value("urn:schemas-upnp-org:control-1-0");
    error.append_child("errorCode").text().set(code);
    error.append_child("errorDescription").text().set(description);

    beast::flat_buffer result;
    buffer_writer writer{result};
    xml_doc.print(writer);

    return result;
}

}