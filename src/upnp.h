#ifndef EEMS_UPNP_H
#define EEMS_UPNP_H

#include "fs.h"
#include "net.h"

#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>

namespace eems
{
auto handle_upnp_request(net::use_awaitable_t<>::as_default_on_t<beast::tcp_stream>& stream,
                         http::header<true, http::fields> const& req,
                         fs::path::iterator begin, fs::path::iterator end)
    -> net::awaitable<void>;
}

#endif
