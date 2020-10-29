#include "directory_service.h"

#include "ranges.h"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

namespace ranges
{
template <>
inline constexpr bool enable_borrowed_range<std::filesystem::directory_iterator> = true;
}

namespace eems
{
auto directory_service::browse(std::u8string_view id, browse_flag mode) -> std::vector<directory_element>
{
    if (id == u8"0")
    {
        return config_.content_directories |
               views::transform([](auto const& path) -> directory_element {
                   // TODO: This may be empty, need to map
                   return container{path.generic_u8string(), u8"0", path.filename().generic_u8string(), u8"object.container"};
               }) |
               ranges::to<std::vector>();
    }

    return fs::directory_iterator{id} |
           views::filter([](fs::directory_entry const& e) {
               if (e.is_directory())
                   return true;
               return false;
           }) |
           views::transform([&id](fs::directory_entry const& e) -> directory_element {
               return container{e.path().generic_u8string(), std::u8string{id}, e.path().filename().generic_u8string(), u8"object.container"};
               // if (e.is_directory()) {

               // }
           }) |
           ranges::to<std::vector>();
}

}
