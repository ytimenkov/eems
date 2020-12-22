#ifndef EEMS_CONTENT_SERVICE_H
#define EEMS_CONTENT_SERVICE_H

#include "fs.h"
#include "http_messages.h"
#include "store/store_service.h"

#include <boost/beast/core/file.hpp>
#include <boost/beast/http/buffer_body.hpp>

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
    auto create_response(fs::path const& sub_path, http_request const& req)
        -> std::tuple<http::response<http::buffer_body>, beast::file, std::uintmax_t>;

private:
    store_service& store_service_;
};

}

#endif
