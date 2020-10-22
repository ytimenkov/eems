#include "logging.h"
#include "server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <spdlog/spdlog.h>

int main()
{
    // TODO: Configure log file name
    spdlog::set_default_logger(std::move(eems::intialize_logging("eems.log")));

    boost::asio::io_context io_context{1};

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    boost::asio::co_spawn(io_context, eems::run_server(), boost::asio::detached);

    io_context.run();

    return 0;
}
