#include "config.h"
#include "discovery_service.h"
#include "logging.h"
#include "scanner/movie_scanner.h"
#include "server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

int main(int argc, char const* argv[])
{
    auto config = eems::load_configuration(argc, argv);

    spdlog::set_default_logger(std::move(eems::intialize_logging(config.logging)));

    eems::store_service store_service{};
    eems::upnp_service upnp_service{store_service, config.server};
    eems::content_service content_service{store_service};
    eems::server server{config.server, upnp_service, content_service};
    eems::discovery_service discovery_service{config.server};

    if (auto const db_existed = store_service.open_db(config.db); !db_existed)
    {
        // Fresh DB, scan.
        eems::movie_scanner movie_scanner{store_service};
        for (auto& dir : config.data.content_directories)
        {
            spdlog::info("Scanning: {}", dir.path);
            std::visit(eems::lambda_visitor{
                           [&path = dir.path, &movie_scanner](eems::movies_library_config const& config) {
                               movie_scanner.scan_all(path, config);
                           }},
                       dir.scanner_config);
        }
    }

    boost::asio::io_context io_context{1};

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    boost::asio::co_spawn(io_context, server.run_server(), boost::asio::detached);
    boost::asio::co_spawn(io_context, discovery_service.run_service(), boost::asio::detached);

    io_context.run();

    return 0;
}
