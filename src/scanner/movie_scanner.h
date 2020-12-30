#ifndef EEMS_MOVIE_SCANNER_H
#define EEMS_MOVIE_SCANNER_H

#include "../data_config.h"
#include "../fs.h"
#include "../store/store_service.h"

namespace eems
{

struct file_info
{
    std::u8string_view mime_type;
    fs::path path;
};

class movie_scanner
{
public:
    explicit movie_scanner(store_service& store)
        : store_{store}
    {
    }

    auto scan_all(fs::path const& root, movies_library_config const& config)
        -> void;

private:
    auto scan_directory(fs::path const& path, movies_library_config const& config, ObjectKey parent)
        -> std::vector<std::tuple<fs::path, ObjectKey>>;

    auto create_container(std::u8string_view name,
                          std::tuple<file_info const*, ArtworkType> artwork)
        -> ObjectKey;

    auto serialize_resource(file_info const& info)
        -> std::tuple<ResourceKey, flatbuffers::DetachedBuffer>;

    auto get_movies_folder_id() -> ObjectKey;

    auto next_resource_key() -> ResourceKey;
    auto next_object_key() -> ObjectKey;

    friend struct object_composer;

private:
    store_service& store_;
    ObjectKey movies_folder_{-1};
    int64_t next_resource_id_{-1};
    int64_t next_object_id_{-1};
};

}

#endif
