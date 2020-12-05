#ifndef EEMS_CONFIG_H
#define EEMS_CONFIG_H

#include "fs.h"

#include <vector>

namespace eems
{
struct db_config
{
    fs::path path;
};
struct data_config
{
    std::vector<fs::path> content_directories;
};

struct config
{
    data_config data;
    db_config db;
};

auto load_configuration(int argc, char const* argv[])
    -> config;

}
#endif