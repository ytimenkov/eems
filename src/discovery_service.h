#ifndef EEMS_DISCOVERY_SERVICE_H
#define EEMS_DISCOVERY_SERVICE_H

#include "net.h"
#include "server_config.h"

#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <optional>

namespace eems
{
class discovery_service
{
public:
    explicit discovery_service(server_config const& config)
        : config_{config}
    {
    }

    auto run_service()
        -> net::awaitable<void>;

private:
    auto handle_request(http::request<http::empty_body>&& req)
        -> std::optional<http::response<http::empty_body>>;

private:
    server_config const& config_;
};
}

#endif
