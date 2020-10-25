#ifndef EEMS_SOAP_H
#define EEMS_SOAP_H

#include "http_messages.h"

#include <pugixml.hpp>

namespace eems
{
struct soap_action_info
{
    std::string service_id;
    std::string action;
    pugi::xml_document doc;
    pugi::xml_node params;
};

auto parse_soap_request(http_request& req) -> soap_action_info;
}
#endif