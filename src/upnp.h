#ifndef EEMS_UPNP_H
#define EEMS_UPNP_H

#include "fs.h"
#include "http_messages.h"
#include "store/store_service.h"

namespace eems
{
struct soap_action_info;
class upnp_service
{
public:
    explicit upnp_service(store_service& store_service,
                          server_config const& server_config)
        : store_service_{store_service},
          server_config_{server_config}
    {
    }

    auto handle_upnp_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
        -> net::awaitable<void>;

private:
    auto handle_cds_browse(tcp_stream& stream, http_request&& req, soap_action_info const& soap_req)
        -> net::awaitable<void>;

private:
    store_service& store_service_;
    server_config const& server_config_;
};
}

#endif
