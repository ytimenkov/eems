#ifndef EEMS_LOGGING_H
#define EEMS_LOGGING_H

#include "logging_config.h"

#include <spdlog/logger.h>

namespace eems
{
auto intialize_logging(logging_config const& config) -> std::shared_ptr<spdlog::logger>;

}

#endif
