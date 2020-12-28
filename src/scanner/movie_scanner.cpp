#include "movie_scanner.h"

#include "../ranges.h"
#include "../spirit.h"
#include "../store/fb_converters.h"

#include <boost/algorithm/string/trim.hpp>
#include <chrono>
#include <date/date.h>
#include <fmt/ostream.h>
#include <map>
#include <range/v3/action/push_back.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <regex>
#include <spdlog/spdlog.h>
#include <unordered_map>

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
    -> std::optional<std::u8string_view>
{
    static auto const EXTENSIONS = std::unordered_map<fs::path, std::u8string_view, hasher>{
        {fs::path{".mkv"}, u8"video/x-matroska"},
        {fs::path{".mp4"}, u8"video/mp4"},

        {fs::path{".jpg"}, u8"image/jpeg"},

        {fs::path{".srt"}, u8"text/srt"},
    };
    if (auto it = EXTENSIONS.find(path.extension()); it != EXTENSIONS.end())
    {
        return it->second;
    }
    return std::nullopt;
}

inline auto is_video_type(std::u8string_view mime_type)
{
    return mime_type.starts_with(u8"video/");
}

inline auto is_image_type(std::u8string_view mime_type)
{
    return mime_type.starts_with(u8"image/");
}

inline auto is_text_type(std::u8string_view mime_type)
{
    return mime_type.starts_with(u8"text/");
}

auto get_upnp_class(std::u8string_view mime_type) -> std::u8string_view
{
    if (is_video_type(mime_type))
        return u8"object.item.videoItem";
    else
        return u8"object.item";
}

auto normalize_title(std::u8string title) -> std::tuple<std::string, int>
{
    // TODO: This is a bit messy because std::regex doesn't support unicode.
    static std::regex const re_wordsep{"[\\._](?=\\w)"};
    static std::regex const re_year("\\(?([12]\\d{3})\\)?");

    auto ntitle = std::regex_replace(reinterpret_cast<char const*>(title.c_str()), re_wordsep, " ");
    int year = 0;
    if (std::smatch m; std::regex_search(ntitle, m, re_year))
    {
#if !(__INTELLISENSE__ == 1)
        parse(std::basic_string_view{m[1].first, m[1].second}, x3::int64, year);
#endif
        ntitle.erase(m[0].first, ntitle.end());
    }
    boost::algorithm::trim(ntitle);
    return {std::move(ntitle), year};
}

constexpr std::u8string_view upnp_movie_class{u8"object.item.videoItem.movie"};

struct file_info
{
    std::u8string_view mime_type;
};

inline auto classify_artwork(std::u8string_view name)
    -> std::optional<ArtworkType>
{
    if (name.find(u8"poster") != name.npos)
        return ArtworkType::Poster;
    else if (name.find(u8"thumb") != name.npos)
        return ArtworkType::Thumbnail;

    return std::nullopt;
}

struct object_composer
{
    int64_t& resource_id;
    fs::path const& base_path;

    std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>> resources;
    std::map<std::u8string, file_info, std::less<>> subtitles_;
    std::map<std::u8string, file_info, std::less<>> artwork_;

    auto artwork(std::u8string&& name, std::u8string_view mime_type)
        -> void
    {
        artwork_.emplace(std::piecewise_construct,
                         std::forward_as_tuple(std::move(name)),
                         std::forward_as_tuple(mime_type));
    }

    auto subtitles(std::u8string&& name, std::u8string_view mime_type)
        -> void
    {
        subtitles_.emplace(std::piecewise_construct,
                           std::forward_as_tuple(std::move(name)),
                           std::forward_as_tuple(mime_type));
    }

    auto operator()(std::pair<fs::path, file_info> const& p)
        -> flatbuffers::DetachedBuffer
    {
        auto& [name, info] = p;

        std::vector<flatbuffers::Offset<ResourceRef>> item_resources;
        std::vector<flatbuffers::Offset<Artwork>> item_artwork;
        flatbuffers::FlatBufferBuilder fbb{};

        // Main resource.
        item_resources.emplace_back(add_resource(
            store_resource(name, info), info, fbb));

        flatbuffers::Offset<MediaObjectRef> album_art{};

        auto name_only = name.stem().generic_u8string();

        for (auto art_it = artwork_.lower_bound(name_only); art_it != artwork_.end(); ++art_it)
        {
            if (!art_it->first.starts_with(name_only))
                break;

            auto name_suffix = static_cast<std::u8string_view>(art_it->first);
            name_suffix.remove_prefix(name_only.size());
            if (auto art_type = classify_artwork(name_suffix); art_type)
            {
                item_artwork.emplace_back(
                    CreateArtwork(fbb,
                                  put_key(store_resource(art_it->first, art_it->second),
                                          fbb),
                                  *art_type));
            }
        }
        if (auto folder_jpg_it = artwork_.find(u8"folder.jpg"); folder_jpg_it != artwork_.end())
        {
            item_artwork.emplace_back(
                CreateArtwork(fbb,
                              put_key(store_resource(
                                          folder_jpg_it->first,
                                          folder_jpg_it->second),
                                      fbb),
                              ArtworkType::Thumbnail));
        }

        for (auto subs_it = subtitles_.lower_bound(name_only); subs_it != subtitles_.end(); ++subs_it)
        {
            if (!subs_it->first.starts_with(name_only))
                break;

            item_resources.emplace_back(add_resource(
                store_resource(subs_it->first, subs_it->second), subs_it->second, fbb));
        }

        auto data_off = CreateMediaItem(fbb, fbb.CreateVector(item_resources));
        // NOTE: Normally partition is enough, but FB offers only this API 😥
        auto artwork_off = put_sorted_vector(fbb, std::move(item_artwork));
        auto [title, year] = normalize_title(name_only);
        auto dc_title = put_string(title, fbb);
        auto upnp_class = put_string_view(upnp_movie_class, fbb);

        auto object_builder = MediaObjectBuilder(fbb);
        object_builder.add_dc_title(dc_title);
        object_builder.add_upnp_class(upnp_class);
        object_builder.add_artwork(artwork_off);
        if (year)
        {
            object_builder.add_dc_date(static_cast<date::sys_days>(
                                           date::year_month_day{date::year{year}, date::January, date::day{1}})
                                           .time_since_epoch()
                                           .count());
        }
        // Add keys to be updated later.
        {
            auto item_id = ObjectKey{};
            object_builder.add_id(&item_id);
        }
        {
            auto parent_id = ObjectKey{};
            object_builder.add_parent_id(&parent_id);
        }
        object_builder.add_data_type(ObjectUnion::MediaItem);
        object_builder.add_data(data_off.Union());
        fbb.Finish(object_builder.Finish());
        return fbb.Release();
    }

    auto add_resource(std::string const& key, file_info const& info,
                      flatbuffers::FlatBufferBuilder& fbb) -> flatbuffers::Offset<ResourceRef>
    {
        auto key_off = fbb.CreateVector(reinterpret_cast<uint8_t const*>(key.data()), key.length());
        auto protocol_info = put_string(fmt::format(u8"http-get:*:{}:*", info.mime_type), fbb);
        ResourceRefBuilder ref_builder{fbb};
        ref_builder.add_ref(key_off);
        ref_builder.add_protocol_info(protocol_info);
        return ref_builder.Finish();
    }

    auto store_resource(fs::path const& name, file_info const& info) -> std::string
    {
        flatbuffers::FlatBufferBuilder resource_fbb{};
        auto location = put_string((base_path / name).native(), resource_fbb);
        auto mime = put_string_view(info.mime_type, resource_fbb);

        ResourceBuilder resource_builder{resource_fbb};
        resource_builder.add_location(location);
        resource_builder.add_mime_type(mime);
        resource_fbb.Finish(resource_builder.Finish());
        auto resource_key = ResourceKey{resource_id++};
        spdlog::debug("Assigning resource key: {} to {}", resource_key.id(), name);
        resources.emplace_back(resource_key, resource_fbb.Release());
        return serialize_key(resource_key);
    }
};

auto movie_scanner::scan_directory(fs::path const& path, int64_t& resource_id)
    -> movie_scanner::scan_result
{
    spdlog::debug("Scanning: {}", path);

    scan_result result;
    std::map<fs::path, file_info> videos;
    object_composer composer{resource_id, path};

    for (auto item : fs::directory_iterator{path})
    {
        if (item.is_directory())
        {
            result.directories.emplace_back(item);
            continue;
        }
        else if (!item.is_regular_file())
        {
            continue;
        }

        auto const mime_type = get_mime_type(item);
        if (!mime_type)
            continue;

        if (is_video_type(*mime_type))
        {
            videos.emplace(std::piecewise_construct,
                           std::forward_as_tuple(item.path().filename()),
                           std::forward_as_tuple(*mime_type));
        }
        else if (is_image_type(*mime_type))
        {
            composer.artwork(item.path().filename().generic_u8string(), *mime_type);
        }
        else if (is_text_type(*mime_type))
        {
            composer.subtitles(item.path().filename().generic_u8string(), *mime_type);
        }
        else
        {
            spdlog::debug("Skipping unknown {}", item.path());
        }
    }

#if !(__INTELLISENSE__ == 1)
    result.items = views::transform(videos, std::ref(composer)) | ranges::to<std::vector>();
#endif
    result.resources = std::move(composer.resources);

    return result;
}

auto movie_scanner::scan_all(fs::path const& root, store_service& store) -> void
{
    auto directories = std::vector<fs::path>{root};
    auto resource_id = store.get_next_id<ResourceKey>();
    while (!directories.empty())
    {
        auto scan_result = scan_directory(directories.back(), resource_id);
        store.put_items({1}, std::move(scan_result.items), std::move(scan_result.resources));
        directories.pop_back();
        ranges::push_back(directories, scan_result.directories);
    }
}

}