#include "scan_service.h"

#include "ranges.h"

#include <fmt/ostream.h>
#include <range/v3/action/push_back.hpp>
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
    };
    if (auto it = EXTENSIONS.find(path.extension()); it != EXTENSIONS.end())
    {
        return it->second;
    }
    return std::nullopt;
}

auto get_upnp_class(std::u8string_view mime_type) -> std::u8string_view
{
    if (mime_type.starts_with(u8"video/"))
        return u8"object.item.videoItem";
    else
        return u8"object.item";
}

inline auto create_u8_string(std::u8string_view data, flatbuffers::FlatBufferBuilder& builder)
{
    return builder.CreateVector(reinterpret_cast<uint8_t const*>(data.data()), data.length());
}

auto scan_service::scan_directory(fs::path const& path, int64_t& resource_id)
    -> scan_service::scan_result
{
    spdlog::debug("Scanning: {}", path);
    scan_result result;
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

        std::vector<flatbuffers::Offset<ResourceRef>> item_resources;
        flatbuffers::FlatBufferBuilder fbb{};

        // Main resource.
        {
            flatbuffers::FlatBufferBuilder resource_fbb{};
            auto location = create_u8_string(item.path().generic_u8string(), resource_fbb);
            auto mime = create_u8_string(*mime_type, resource_fbb);

            ResourceBuilder resource_builder{resource_fbb};
            resource_builder.add_location(location);
            resource_builder.add_mime_type(mime);
            resource_fbb.Finish(resource_builder.Finish());
            auto resource_key = ResourceKey{resource_id++};
            result.resources.emplace_back(resource_key, resource_fbb.Release());

            auto key = serialize_key(resource_key);
            spdlog::debug("Assigning resource key: {}", resource_key.id());
            auto key_off = fbb.CreateVector(reinterpret_cast<uint8_t const*>(key.data()), key.length());
            auto protocol_info = create_u8_string(fmt::format(u8"http-get:*:{}:*", *mime_type), fbb);
            ResourceRefBuilder ref_builder{fbb};
            ref_builder.add_ref(key_off);
            ref_builder.add_protocol_info(protocol_info);
            item_resources.emplace_back(ref_builder.Finish());
        }
        auto dc_title = create_u8_string(item.path().filename().generic_u8string(), fbb);
        auto upnp_class = create_u8_string(get_upnp_class(*mime_type), fbb);
        auto resources_off = fbb.CreateVector(item_resources);

        auto item_builder = MediaItemBuilder(fbb);
        item_builder.add_dc_title(dc_title);
        item_builder.add_upnp_class(upnp_class);
        item_builder.add_resources(resources_off);
        // Add keys to be updated later.
        {
            auto item_id = ItemKey{};
            item_builder.add_id(&item_id);
        }
        {
            auto parent_id = ContainerKey{};
            item_builder.add_parent_id(&parent_id);
        }
        fbb.Finish(item_builder.Finish());
        result.items.emplace_back(fbb.Release());

        spdlog::info("Discovered media item [{}] at {}", to_byte_range(*mime_type), item.path());
    }
    return result;
}

auto scan_service::scan_all(fs::path const& root, store_service& store) -> void
{
    auto directories = std::vector<fs::path>{root};
    auto parent_id = std::string{"0"};
    auto resource_id = store.get_next_id<ResourceKey>();
    while (!directories.empty())
    {
        auto scan_result = scan_directory(directories.back(), resource_id);
        store.put_items({}, std::move(scan_result.items), std::move(scan_result.resources));
        directories.pop_back();
        ranges::push_back(directories, scan_result.directories);
    }
}

}
