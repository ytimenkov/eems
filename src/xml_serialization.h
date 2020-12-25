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

auto browse_response(store_service::list_result_view const& list, std::string_view base_url) -> beast::flat_buffer;

auto generate_preamble(char const* root_element, char const* root_ns)
    -> std::tuple<pugi::xml_document, pugi::xml_node>;

}

#endif