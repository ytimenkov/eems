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

auto root_device_description(char const* url_base) -> beast::flat_buffer
{
    pugi::xml_document doc;
    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto root = doc.append_child("root");
    root.append_attribute("xmlns") = "urn:schemas-upnp-org:device-1-0";

    auto specVersion = root.append_child("specVersion");
    specVersion.append_child("major").append_child(pugi::node_pcdata).set_value("1");
    specVersion.append_child("minor").append_child(pugi::node_pcdata).set_value("0");

    root.append_child("URLBase").append_child(pugi::node_pcdata).set_value(url_base);

    beast::flat_buffer result;
    buffer_writer writer{result};

    doc.print(writer);

    return result;
}
} // namespace eems