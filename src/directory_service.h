#ifndef EEMS_DIRECTORY_SERVICE_H
#define EEMS_DIRECTORY_SERVICE_H

#include "config.h"

#include <string>
#include <variant>
#include <vector>

namespace eems
{
enum class browse_flag
{
    metadata,
    direct_children,
};

struct object
{
    std::u8string id;
    std::u8string parent_id;
    std::u8string dc_title;
    std::u8string upnp_class;
};

struct item : object
{
};

struct container : object
{
};

using directory_element = std::variant<item, container>;

class directory_service
{
  public:
    explicit directory_service(data_config const& config)
        : config_{config}
    {
    }

    // TODO: There are filter and search criteria and start index / count
    // TODO: Also returns dumb NumberReturned value and TotalMatches and UpdateID
    auto browse(std::u8string_view id, browse_flag mode) -> std::vector<directory_element>;

  private:
    data_config const& config_;
};
}
#endif