#include "config.h"
#include "logging.h"
#include "scan_service.h"
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
    // TODO: Configure log file name
    spdlog::set_default_logger(std::move(eems::intialize_logging("eems.log")));

    spdlog::info("Configs: {}", config.data.content_directories);

    auto store_service = eems::store_service{config.db};
    auto upnp_service = eems::upnp_service{store_service};
    auto content_service = eems::content_service{store_service};
    auto server = eems::server{upnp_service, content_service};
    auto scan_service = eems::scan_service{};

    // TODO: This is temporary:
    for (auto& path : config.data.content_directories)
    {
        scan_service.scan_all(path, store_service);
    }

    boost::asio::io_context io_context{1};

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    boost::asio::co_spawn(io_context, server.run_server(), boost::asio::detached);

    io_context.run();

    return 0;
}
