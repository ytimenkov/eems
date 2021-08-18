#include "movie_scanner.h"

#include "../ranges.h"
#include "../spirit.h"
#include "../store/fb_converters.h"

#include <boost/algorithm/string/trim.hpp>
#include <chrono>
#include <date/date.h>
#include <fmt/ostream.h>
#include <fmt/xchar.h>
#include <map>
#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <regex>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace eems
{

constexpr std::u8string_view movies_folder_name{u8"Movies"};
constexpr std::u8string_view upnp_movie_class{u8"object.item.videoItem.movie"};

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
        {fs::path{".avi"}, u8"video/x-msvideo"},
        {fs::path{".mpg"}, u8"video/mpeg"},

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

auto normalize_title(std::u8string title) -> std::tuple<std::string, int>
{
    // TODO: This is a bit messy because std::regex doesn't support unicode.
    static std::regex const re_wordsep{"[\\._](?=\\w)"};
    static std::regex const re_year("\\(?([12]\\d{3})\\)?");

    auto ntitle = std::regex_replace(reinterpret_cast<char const*>(title.c_str()), re_wordsep, " ");
    int year = 0;
    if (std::smatch m; std::regex_search(ntitle, m, re_year))
    {
        parse(std::basic_string_view{m[1].first, m[1].second}, x3::int64, year);
        ntitle.erase(m[0].first, ntitle.end());
    }
    boost::algorithm::trim(ntitle);
    return {std::move(ntitle), year};
}

inline auto classify_artwork(std::u8string_view name)
    -> std::optional<ArtworkType>
{
    if (name.find(u8"poster") != name.npos)
        return ArtworkType::Poster;
    else if (name.find(u8"thumb") != name.npos)
        return ArtworkType::Thumbnail;

    return std::nullopt;
}

inline auto CreateResourceRef(flatbuffers::FlatBufferBuilder& fbb,
                              std::string const& key, file_info const& info)
    -> flatbuffers::Offset<ResourceRef>
{
    auto key_off = CreateLibraryKey(fbb, key);
    auto protocol_info = put_string(fmt::format(u8"http-get:*:{}:*", info.mime_type), fbb);
    ResourceRefBuilder ref_builder{fbb};
    ref_builder.add_ref(key_off);
    ref_builder.add_protocol_info(protocol_info);
    return ref_builder.Finish();
}

struct object_composer
{
    movie_scanner& context;
    std::u8string folder_name;

    std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>> resources;
    std::map<std::u8string, file_info, std::less<>> subtitles_;
    std::map<std::u8string, file_info, std::less<>> artwork_;
    ObjectKey parent_id;

    auto artwork(fs::path const& path, std::u8string_view mime_type)
        -> void
    {
        artwork_.emplace(std::piecewise_construct,
                         std::forward_as_tuple(path.filename().generic_u8string()),
                         std::forward_as_tuple(mime_type, path));
    }

    auto subtitles(fs::path const& path, std::u8string_view mime_type)
        -> void
    {
        subtitles_.emplace(std::piecewise_construct,
                           std::forward_as_tuple(path.filename().generic_u8string()),
                           std::forward_as_tuple(mime_type, path));
    }

    auto operator()(std::pair<fs::path, file_info> const& p)
        -> flatbuffers::DetachedBuffer
    {
        auto& [name, info] = p;

        std::vector<flatbuffers::Offset<ResourceRef>> item_resources;
        std::vector<flatbuffers::Offset<Artwork>> item_artwork;
        flatbuffers::FlatBufferBuilder fbb{};

        // Main resource.
        item_resources.emplace_back(
            CreateResourceRef(fbb, store_resource(info), info));

        flatbuffers::Offset<MediaObjectRef> album_art{};

        auto resource_prefix = folder_name.empty() ? name.stem().generic_u8string() : std::u8string{};

        for (auto art_it = artwork_.lower_bound(resource_prefix); art_it != artwork_.end(); ++art_it)
        {
            if (!art_it->first.starts_with(resource_prefix))
                break;

            auto name_suffix = static_cast<std::u8string_view>(art_it->first);
            name_suffix.remove_prefix(resource_prefix.size());
            if (auto art_type = classify_artwork(name_suffix); art_type)
            {
                spdlog::debug("Detected item artwork type {}", EnumNameArtworkType(*art_type));
                item_artwork.emplace_back(
                    CreateArtwork(fbb,
                                  CreateLibraryKey(fbb, store_resource(art_it->second)),
                                  *art_type));
            }
        }
        if (item_artwork.empty())
        {
            if (auto [folder_artwork, art_type] = get_folder_artwork(); folder_artwork)
            {
                spdlog::debug("No item artworkm Adding folder artwork");
                item_artwork.emplace_back(
                    CreateArtwork(fbb,
                                  CreateLibraryKey(fbb, store_resource(folder_artwork->second)),
                                  art_type));
            }
        }

        for (auto subs_it = subtitles_.lower_bound(resource_prefix); subs_it != subtitles_.end(); ++subs_it)
        {
            if (!subs_it->first.starts_with(resource_prefix))
                break;

            item_resources.emplace_back(
                CreateResourceRef(fbb, store_resource(subs_it->second), subs_it->second));
        }

        auto data_off = CreateMediaItem(fbb, fbb.CreateVector(item_resources));
        // NOTE: Normally partition is enough, but FB offers only this API 😥
        auto artwork_off = put_sorted_vector(fbb, std::move(item_artwork));
        auto [title, year] = normalize_title(folder_name.empty() ? resource_prefix : folder_name);
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
        {
            auto item_id = context.next_object_key();
            object_builder.add_id(&item_id);
        }
        object_builder.add_parent_id(&parent_id);
        object_builder.add_data_type(ObjectUnion::MediaItem);
        object_builder.add_data(data_off.Union());
        fbb.Finish(object_builder.Finish());
        return fbb.Release();
    }

    auto store_resource(file_info const& info) -> std::string
    {
        auto& res = resources.emplace_back(context.serialize_resource(info));
        return serialize_key(std::get<ResourceKey>(res));
    }

    auto get_folder_artwork() -> std::pair<std::pair<std::u8string const, file_info> const*, ArtworkType>
    {
        for (auto& [candidate, type] :
             {
                 std::pair{u8"poster.jpg", ArtworkType::Poster},
                 std::pair{u8"folder.jpg", ArtworkType::Thumbnail},
             })
        {
            if (auto it = artwork_.find(candidate); it != artwork_.end())
            {
                return {std::to_address(it), type};
            }
        }
        return {nullptr, ArtworkType::Thumbnail};
    }
};

auto movie_scanner::scan_directory(fs::path const& path, movies_library_config const& config, ObjectKey parent)
    -> std::vector<std::tuple<fs::path, ObjectKey>>
{
    spdlog::info("Scanning for movies: {}", path);

    std::vector<std::tuple<fs::path, ObjectKey>> directories{};

    std::map<fs::path, file_info> videos;
    object_composer composer{.context = *this, .parent_id = parent};

    for (auto item : fs::directory_iterator{path})
    {
        if (item.is_directory())
        {
            directories.emplace_back(item, parent);
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
            auto path = item.path();
            auto file_name = path.filename();
            videos.emplace(std::piecewise_construct,
                           std::forward_as_tuple(std::move(file_name)),
                           std::forward_as_tuple(*mime_type, std::move(path)));
        }
        else if (is_image_type(*mime_type))
        {
            composer.artwork(item.path(), *mime_type);
        }
        else if (is_text_type(*mime_type))
        {
            composer.subtitles(item.path(), *mime_type);
        }
        else
        {
            spdlog::debug("Skipping unknown {}", item.path());
        }
    }

    if (config.use_collections)
    {
        auto const artwork = composer.get_folder_artwork();

        const auto add_collection = [&]() mutable -> bool
        {
            auto const movies_count = videos.size();
            if (movies_count == 1)
                return false;
            if (movies_count > 1)
                return true;
            return artwork.first && !directories.empty();
        }();

        if (add_collection)
        {
            composer.parent_id = create_container(
                path.stem().generic_u8string(),
                {artwork.first ? &artwork.first->second : nullptr, artwork.second},
                parent);

            if (artwork.first)
            {
                ranges::for_each(directories, [collection_key = composer.parent_id](auto& tup)
                                 {
                                     spdlog::debug("Assigned parent {} to {}", collection_key.id(), std::get<fs::path>(tup));
                                     std::get<ObjectKey>(tup) = collection_key;
                                 });
            }
        }
    }

    if (config.use_folder_names && videos.size() == 1)
        composer.folder_name = path.stem().u8string();

    if (videos.size() > 0)
    {
        store_.put_items(
            composer.parent_id,
            views::transform(videos, std::ref(composer)) | ranges::to<std::vector>(),
            std::move(std::move(composer.resources)));
    }

    return directories;
}

auto movie_scanner::scan_all(fs::path const& root, movies_library_config const& original_config) -> void
{
    next_resource_id_ = store_.get_next_id<ResourceKey>();
    next_object_id_ = store_.get_next_id<ObjectKey>();
    auto directories = std::vector<std::tuple<fs::path, ObjectKey>>{{root, get_movies_folder_id()}};

    auto config = original_config;
    config.use_collections = false;
    while (!directories.empty())
    {
        auto& [path, parent] = directories.back();
        auto new_directories = scan_directory(path, config, parent);
        directories.pop_back();
        ranges::push_back(directories, new_directories);
        config.use_collections = original_config.use_collections;
    }
}

auto movie_scanner::get_movies_folder_id() -> ObjectKey
{
    if (movies_folder_.id() > 0)
        return movies_folder_;

    ObjectKey const root_key{};

    {
        auto root_container_view = store_.list(root_key);
        if (auto it = ranges::find_if(root_container_view, [](MediaObject const& item)
                                      { return as_string_view<char8_t>(*item.dc_title()) == movies_folder_name; });
            it != ranges::end(root_container_view))
        {
            return movies_folder_ = *it->id();
        }
    }

    flatbuffers::FlatBufferBuilder fbb{};

    auto const new_key = next_object_key();

    auto title_off = put_string_view(movies_folder_name, fbb);
    auto class_off = put_string_view<char8_t>(upnp_container_class, fbb);
    auto container_off = CreateMediaContainer(fbb);

    MediaObjectBuilder builder{fbb};
    builder.add_id(&new_key);
    builder.add_parent_id(&root_key);
    builder.add_dc_title(title_off);
    builder.add_upnp_class(class_off);
    builder.add_data_type(ObjectUnion::MediaContainer);
    builder.add_data(container_off.Union());

    fbb.Finish(builder.Finish());
    std::vector<flatbuffers::DetachedBuffer> items;
    items.emplace_back(fbb.Release());
    store_.put_items(root_key, std::move(items), {});

    return movies_folder_ = new_key;
}

auto movie_scanner::create_container(std::u8string_view name,
                                     std::tuple<file_info const*, ArtworkType> artwork,
                                     ObjectKey parent)
    -> ObjectKey
{
    std::vector<flatbuffers::DetachedBuffer> items;
    std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>> resources;

    container_meta meta{
        .id{next_object_key()},
        .parent_id{parent},
        .dc_title{std::u8string{name}},
        .upnp_class{upnp_container_class}};

    if (auto [info, art_type] = artwork; info)
    {
        auto& [res_key, res_buf] = resources.emplace_back(serialize_resource(*info));
        meta.artwork.emplace_back(serialize_key(res_key), art_type);
    }

    items.emplace_back(serialize_container({}, meta));

    store_.put_items(meta.parent_id, std::move(items), std::move(resources));

    return meta.id;
}

auto movie_scanner::serialize_resource(file_info const& info)
    -> std::tuple<ResourceKey, flatbuffers::DetachedBuffer>
{
    flatbuffers::FlatBufferBuilder resource_fbb{};
    auto const location = put_string(info.path.native(), resource_fbb);
    auto const mime = put_string_view(info.mime_type, resource_fbb);

    ResourceBuilder resource_builder{resource_fbb};
    resource_builder.add_location(location);
    resource_builder.add_mime_type(mime);
    resource_fbb.Finish(resource_builder.Finish());
    auto const resource_key = next_resource_key();
    spdlog::info("Assigning resource key: {} to {}", resource_key.id(), info.path);
    return {resource_key, resource_fbb.Release()};
}

inline auto movie_scanner::next_resource_key() -> ResourceKey
{
    return next_resource_id_++;
}

inline auto movie_scanner::next_object_key() -> ObjectKey
{
    return next_object_id_++;
}

}
