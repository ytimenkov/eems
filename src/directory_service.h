#ifndef EEMS_DIRECTORY_SERVICE_H
#define EEMS_DIRECTORY_SERVICE_H

#include "store/store_service.h"

#include <optional>
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

struct resource
{
    std::u8string protocol_info;
    std::u8string url;
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
    std::vector<resource> resources;
};

struct container : object
{
};

using directory_element = std::variant<item, container>;

class directory_service
{
public:
    explicit directory_service(store_service& store_service)
        : store_service_{store_service}
    {
    }

    // TODO: There are filter and search criteria and start index / count
    // TODO: Also returns dumb NumberReturned value and TotalMatches and UpdateID
    auto browse(std::u8string_view id, browse_flag mode) -> std::vector<directory_element>;

private:
    store_service& store_service_;
};

}
#endif