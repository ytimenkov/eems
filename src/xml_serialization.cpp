#include "xml_serialization.h"

#include "net.h"
#include "ranges.h"
#include "store/fb_converters.h"

#include <boost/uuid/uuid_io.hpp>
#include <fmt/core.h>
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

auto serialize(pugi::xml_node& didl_root, std::string_view content_base, MediaObject const& object) -> bool
{
    auto serialize_common_fields = [&object](pugi::xml_node& node) {
        node.append_attribute("id").set_value(object.id()->id());
        node.append_attribute("parentID").set_value(object.parent_id()->id());
        node.append_attribute("restricted").set_value("1");

        node.append_child("dc:title").text().set(as_cstring<char>(*object.dc_title()));
        node.append_child("upnp:class").text().set(as_cstring<char>(*object.upnp_class()));
    };

    switch (object.data_type())
    {
    case ObjectUnion::MediaItem:
    {
        auto& item = *static_cast<MediaItem const*>(object.data());
        auto node = didl_root.append_child("item");
        serialize_common_fields(node);
        if (auto resources = item.resources(); resources)
        {
            ranges::for_each(*resources, [&node, &object, content_base](ResourceRef const* r) {
                if (auto res_key = r->ref_nested_root()->key_as_ResourceKey(); res_key)
                {
                    auto res = node.append_child("res");
                    res.append_attribute("protocolInfo").set_value(as_cstring<char>(*r->protocol_info()));
                    res.text().set(fmt::format("{}{}", content_base, res_key->id()).c_str());
                }
                else
                {
                    spdlog::error("Resource ref has no valid key: {}", object.id()->id());
                }
            });
        }
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

auto browse_response(pugi::xml_document const& didl_doc, std::size_t count) -> beast::flat_buffer
{
    beast::flat_buffer result;
    buffer_writer writer{result};

    didl_doc.print(writer);
    // Unfortunately PugiXML doesn't provide string_view-like interfaces, only C-strings...
    append_null(result);

    spdlog::debug("Returning DIDL:\n{}", static_cast<char const*>(result.data().data()));

    auto soap_doc = pugi::xml_document{};
    auto soap_root = soap_doc.append_child("s:Envelope");
    soap_root.append_attribute("xmlns:s").set_value("http://schemas.xmlsoap.org/soap/envelope/");
    soap_root.append_attribute("s:encodingStyle").set_value("http://schemas.xmlsoap.org/soap/encoding/");
    auto soap_body = soap_root.append_child("s:Body");

    // TODO: This is service type / action + Response
    auto response = soap_body.append_child("u:BrowseResponse");
    response.append_attribute("xmlns:u").set_value("urn:schemas-upnp-org:service:ContentDirectory:1");
    response.append_child("NumberReturned").text().set(count);
    response.append_child("TotalMatches").text().set("0");
    response.append_child("UpdateID").text().set("0");
    response.append_child("Result").text().set(static_cast<char const*>(result.data().data()));

    // Just reuse same writer / buffer...
    result.consume(result.size());
    soap_doc.print(writer);

    return result;
}

}