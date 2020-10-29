#ifndef EEMS_SERVER_H
#define EEMS_SERVER_H

#include "net.h"
#include "upnp.h"

#include <boost/asio/awaitable.hpp>

namespace eems
{
class server
{
  public:
    explicit server(upnp_service& upnp_service)
        : upnp_service_{upnp_service}
    {
    }

    auto run_server() -> net::awaitable<void>;

  private:
    auto handle_connections(net::ip::tcp::socket socket) -> net::awaitable<void>;

  private:
    upnp_service& upnp_service_;
};
}

#endif
