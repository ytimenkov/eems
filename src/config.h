#ifndef EEMS_CONFIG_H
#define EEMS_CONFIG_H

#include "fs.h"

#include <vector>

namespace eems
{
struct data_config
{
    std::vector<fs::path> content_directories;
};

auto load_configuration(int argc, char const* argv[])
    -> data_config;

}
#endif