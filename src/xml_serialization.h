#ifndef EEMS_XML_SERIALIZATION_H
#define EEMS_XML_SERIALIZATION_H

#include "directory_service.h"
#include "net.h"

#include <boost/beast/core/flat_buffer.hpp>

namespace eems
{
auto root_device_description(char const* url_base) -> beast::flat_buffer;
auto list_response(std::vector<directory_element> const& contents, std::u8string_view content_base)
    -> beast::flat_buffer;
}

#endif