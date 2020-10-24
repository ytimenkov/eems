#ifndef EEMS_SOAP_H
#define EEMS_SOAP_H

#include "http_messages.h"

namespace eems
{
auto handle_soap_request(http_request&& req) -> void;
}
#endif