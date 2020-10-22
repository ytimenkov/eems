#ifndef EEMS_UPNP_H
#define EEMS_UPNP_H

#include "fs.h"
#include "http_messages.h"

namespace eems
{
auto handle_upnp_request(tcp_stream& stream, http_request const& req,
                         fs::path::iterator begin, fs::path::iterator end)
    -> net::awaitable<void>;
}

#endif
