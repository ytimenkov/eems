#ifndef EEMS_SERVER_CONFIG_H
#define EEMS_SERVER_CONFIG_H

#include <boost/uuid/uuid.hpp>
#include <string>

namespace eems
{

struct server_config
{
    boost::uuids::uuid uuid;
    unsigned short listen_port{0};
    std::string host_name;
    std::string name;
    std::string base_url;
};

}

#endif