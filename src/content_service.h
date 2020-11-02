#ifndef EEMS_CONTENT_SERVICE_H
#define EEMS_CONTENT_SERVICE_H

#include "fs.h"
#include "http_messages.h"

namespace eems
{
class content_service
{
public:
    explicit content_service()
    {
    }

    auto handle_request(tcp_stream& stream, http_request&& req, fs::path sub_path)
        -> net::awaitable<void>;
};

}

#endif
