#ifndef EEMS_XML_SERIALIZATION_H
#define EEMS_XML_SERIALIZATION_H

#include "net.h"
#include "server_config.h"
#include "store/store_service.h"

#include <boost/beast/core/flat_buffer.hpp>

namespace eems
{

auto root_device_description(server_config const& server_config)
    -> beast::flat_buffer;

auto browse_response(store_service::list_result_view const& list,
                     uint32_t start_index, uint32_t requested_count,
                     std::string_view base_url)
    -> beast::flat_buffer;

auto error_response(int code, char const* description)
    -> beast::flat_buffer;

}

#endif