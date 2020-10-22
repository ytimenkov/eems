#ifndef EEMS_LOGGING_H
#define EEMS_LOGGING_H

#include <spdlog/logger.h>

namespace eems
{
auto intialize_logging(std::string const& file_name) -> std::shared_ptr<spdlog::logger>;

}

#endif
