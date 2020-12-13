#ifndef EEMS_CONTENT_SERVICE_H
#define EEMS_CONTENT_SERVICE_H

#include "fs.h"
#include "http_messages.h"
#include "store/store_service.h"

namespace eems
{
class content_service
{
public:
    explicit content_service(store_service& store_service)
        : store_service_{store_service}
    {
    }

    auto handle_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
        -> net::awaitable<void>;

private:
    store_service& store_service_;
};

}

#endif
