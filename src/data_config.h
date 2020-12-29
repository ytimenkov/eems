#ifndef EEMS_DATA_CONFIG_H
#define EEMS_DATA_CONFIG_H

#include "fs.h"

#include <variant>
#include <vector>

namespace eems
{
    
struct movies_library_config
{
    bool use_folder_names{true};
};

struct directory_config
{
    fs::path path;
    std::variant<movies_library_config> scanner_config;
};

struct data_config
{
    std::vector<directory_config> content_directories;
};
}
#endif