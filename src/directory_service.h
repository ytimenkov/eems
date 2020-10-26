#ifndef EEMS_DIRECTORY_SERVICE_H
#define EEMS_DIRECTORY_SERVICE_H

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
    std::string id;
    std::string parent_id;
    std::string dc_title;
    std::string upnp_class;
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
    // TODO: There are filter and search criteria and start index / count
    // TODO: Also returns dumb NumberReturned value and TotalMatches and UpdateID
    auto browse(std::string_view id, browse_flag mode) -> std::vector<directory_element>;

  private:
};
}
#endif