#ifndef EEMS_XML_SERIALIZATION_H
#define EEMS_XML_SERIALIZATION_H

#include "config.h"
#include "net.h"
#include "store/store_service.h"

#include <boost/beast/core/flat_buffer.hpp>
#include <pugixml.hpp>

namespace eems
{
auto root_device_description(server_config const& server_config) -> beast::flat_buffer;
auto browse_response(pugi::xml_document const& didl_doc, std::size_t count) -> beast::flat_buffer;

auto generate_preamble(char const* root_element, char const* root_ns)
    -> std::tuple<pugi::xml_document, pugi::xml_node>;

auto serialize(pugi::xml_node& didl_root, std::string_view content_base, MediaObject const& object) -> bool;
}

#endif