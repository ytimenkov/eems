#include "directory_service.h"

#include "ranges.h"
#include "spirit.h"

#include <fmt/core.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

namespace eems
{

inline auto get_u8_string_view(flatbuffers::Vector<uint8_t> const& vec)
    -> std::u8string_view
{
    return {reinterpret_cast<char8_t const*>(vec.Data()), vec.size()};
}

auto directory_service::browse(std::u8string_view id, browse_flag mode) -> std::vector<directory_element>
{
    int64_t parentId;
    if (!parse(id, x3::int64, parentId))
    {
        // TODO: find out proper exception type.
        throw std::runtime_error(fmt::format("Invalid id: {}", reinterpret_cast<std::string_view const&>(id)));
    }
    // TODO: Here there are no checks for case when field doesn't exist and
    // pointers are dereferenced immediately. This is because all fields are
    // optional in the flatbuffers and we need to have validation at some point:
    // either when reading from DB or throughout the code.
    auto result = store_service_.list(ContainerKey{parentId});
    return result.items | views::transform([id](auto const& e) -> directory_element {
               auto dc_title = get_u8_string_view(*e->dc_title());
               auto upnp_class = get_u8_string_view(*e->upnp_class());
               return item{
                   fmt::format(u8"{}", e->id()->id()),
                   fmt::format(u8"{}", e->parent_id()->id()), // Or id, unless the metadata is being queried
                   std::u8string{dc_title},
                   std::u8string{upnp_class},
                   (*e->resources()) | views::transform([](Resource const* db_res) {
                       return resource{
                           std::u8string{get_u8_string_view(*(db_res->protocol_info()))},
                           std::u8string{get_u8_string_view(*(db_res->location()))},
                       };
                   }) | ranges::to<std::vector>()};
           }) |
           ranges::to<std::vector>();
}

}
