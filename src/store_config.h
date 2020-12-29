#ifndef EEMS_STORE_CONFIG_H
#define EEMS_STORE_CONFIG_H

#include "fs.h"

namespace eems
{

struct store_config
{
    fs::path db_path{"/var/lib/eems/db"};
};

}

#endif