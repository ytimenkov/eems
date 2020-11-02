#include "directory_service.h"

#include "ranges.h"

#include <fmt/core.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace ranges
{
template <>
inline constexpr bool enable_borrowed_range<std::filesystem::directory_iterator> = true;
}

namespace eems
{

namespace
{
struct hasher
{
    auto operator()(fs::path const& path) const
    {
        return hash_value(path);
    }
};
}

auto get_mime_type(fs::path const& path)
    -> std::optional<std::string_view>
{
    static auto const EXTENSIONS = std::unordered_map<fs::path, std::string_view, hasher>{
        {fs::path{".mkv"}, "video/x-matroska"},
        {fs::path{".mp4"}, "video/mp4"},
    };
    if (auto it = EXTENSIONS.find(path.extension()); it != EXTENSIONS.end())
    {
        return it->second;
    }
    return std::nullopt;
}

auto directory_service::browse(std::u8string_view id, browse_flag mode) -> std::vector<directory_element>
{
    if (id == u8"0")
    {
        return config_.content_directories |
               views::transform([](auto const& path) -> directory_element {
                   // TODO: This may be empty if ends with slash, need to map
                   return container{path.generic_u8string(), u8"0", path.filename().generic_u8string(), u8"object.container"};
               }) |
               ranges::to<std::vector>();
    }

    return fs::directory_iterator{id} |
           views::filter([](fs::directory_entry const& e) {
               if (e.is_directory())
                   return true;
               else if (e.is_regular_file() && get_mime_type(e))
                   return true;

               return false;
           }) |
           views::transform([&id](fs::directory_entry const& e) -> directory_element {
               if (e.is_directory())
               {
                   return container{e.path().generic_u8string(), std::u8string{id}, e.path().filename().generic_u8string(), u8"object.container"};
               }
               auto mime_type = get_mime_type(e);
               //auto result = directory_element{ std::in_place_type_t<item>};
               return item{e.path().generic_u8string(),
                           std::u8string{id},
                           e.path().filename().generic_u8string(),
                           u8"object.item.videoItem",
                           // TODO: We need full URL here... IIRC VLC didn't handle relative URLs well...
                           {{fmt::format(u8"http-get:*:{}:*", reinterpret_cast<std::u8string_view const&>(*mime_type)), e.path().generic_u8string()}}};
           }) |
           ranges::to<std::vector>();
}

}
