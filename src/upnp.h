#ifndef EEMS_UPNP_H
#define EEMS_UPNP_H

#include "fs.h"
#include "http_messages.h"

namespace eems
{
auto handle_upnp_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
    -> net::awaitable<void>;
}

#endif
