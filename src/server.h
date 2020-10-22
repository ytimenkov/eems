#ifndef EEMS_SERVER_H
#define EEMS_SERVER_H

#include "net.h"

#include <boost/asio/awaitable.hpp>

namespace eems
{
auto run_server() -> net::awaitable<void>;
}

#endif
