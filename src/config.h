#ifndef EEMS_CONFIG_H
#define EEMS_CONFIG_H

#include "fs.h"

#include <boost/uuid/uuid.hpp>
#include <vector>

namespace eems
{
struct db_config
{
    fs::path path{"/var/lib/eems/db"};
};
struct data_config
{
    std::vector<fs::path> content_directories;
};

struct server_config
{
    boost::uuids::uuid uuid;
    unsigned short listen_port{0};
    std::string host_name;
    std::string base_url;
};

struct config
{
    data_config data;
    db_config db;
    server_config server;
};

auto load_configuration(int argc, char const* argv[])
    -> config;

}
#endif