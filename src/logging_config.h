#ifndef EEMS_LOGGING_CONFIG_H
#define EEMS_LOGGING_CONFIG_H

#include "fs.h"

namespace eems
{
struct logging_config
{
    bool truncate{false};
    fs::path path;
};

}

#endif
