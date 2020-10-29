#include "xml_serialization.h"

#include "net.h"

#include <pugixml.hpp>

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

auto append_null(beast::flat_buffer& buffer)
{
    auto buf = buffer.prepare(1);
    constexpr auto null_char = '\0';
    std::memcpy(buf.data(), &null_char, 1);
    buffer.commit(1);
}

auto generate_preamble(char const* root_element, char const* root_ns)
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

auto root_device_description(char const* url_base) -> beast::flat_buffer
{
    auto [doc, root] = generate_preamble("root", "urn:schemas-upnp-org:device-1-0");
    auto specVersion = root.append_child("specVersion");
    specVersion.append_child("major").text().set("1");
    specVersion.append_child("minor").text().set("0");

    root.append_child("URLBase").text().set(url_base);

    beast::flat_buffer result;
    buffer_writer writer{result};

    doc.print(writer);

    return result;
}

auto serialize_common_fields(pugi::xml_node& node, object const& elem) -> void
{
    node.append_attribute("id").set_value(reinterpret_cast<char const*>(elem.id.c_str()));
    node.append_attribute("parentID").set_value(reinterpret_cast<char const*>(elem.parent_id.c_str()));
    node.append_attribute("restricted").set_value("1");

    node.append_child("dc:title").text().set(reinterpret_cast<char const*>(elem.dc_title.c_str()));
    node.append_child("upnp:class").text().set(reinterpret_cast<char const*>(elem.upnp_class.c_str()));
}

auto serialize(pugi::xml_node& node, item const& elem)
{
    auto item = node.append_child("item");
    serialize_common_fields(item, elem);
}

auto serialize(pugi::xml_node& node, container const& elem)
{
    auto container = node.append_child("container");
    serialize_common_fields(container, elem);
}

auto list_response(std::vector<directory_element> const& contents) -> beast::flat_buffer
{
    auto [didl_doc, didl_root] = generate_preamble("DIDL-Lite", "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
    didl_root.append_attribute("xmlns:upnp").set_value("urn:schemas-upnp-org:metadata-1-0/upnp/");
    didl_root.append_attribute("xmlns:dc").set_value("http://purl.org/dc/elements/1.1/");

    for (auto const& elem : contents)
    {
        std::visit([&didl_root](auto const& elem) { serialize(didl_root, elem); }, elem);
    }

    beast::flat_buffer result;
    buffer_writer writer{result};

    didl_doc.print(writer);
    // Unfortunately PugiXML doesn't provide string_view-like interfaces, only C-strings...
    append_null(result);

    auto soap_doc = pugi::xml_document{};
    auto soap_root = soap_doc.append_child("s:Envelope");
    soap_root.append_attribute("xmlns:s").set_value("http://schemas.xmlsoap.org/soap/envelope/");
    soap_root.append_attribute("s:encodingStyle").set_value("http://schemas.xmlsoap.org/soap/encoding/");
    auto soap_body = soap_root.append_child("s:Body");

    // TODO: This is service type / action + Response
    auto response = soap_body.append_child("u:BrowseResponse");
    response.append_attribute("xmlns:u").set_value("urn:schemas-upnp-org:service:ContentDirectory:1");
    response.append_child("NumberReturned").text().set(contents.size());
    response.append_child("TotalMatches").text().set("0");
    response.append_child("UpdateID").text().set("0");
    response.append_child("Result").text().set(static_cast<char const*>(result.data().data()));

    // Just reuse same writer / buffer...
    result.consume(result.size());
    soap_doc.print(writer);

    return result;
}

}