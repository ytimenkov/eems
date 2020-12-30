#ifndef EEMS_MOVIE_SCANNER_H
#define EEMS_MOVIE_SCANNER_H

#include "../data_config.h"
#include "../fs.h"
#include "../store/store_service.h"

namespace eems
{
class movie_scanner
{
public:
    explicit movie_scanner(store_service& store)
        : store_{store}
    {
    }

    auto scan_all(fs::path const& root, movies_library_config const& config) -> void;

private:
    struct scan_result
    {
        std::vector<flatbuffers::DetachedBuffer> items;
        std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>> resources;
        std::vector<fs::path> directories;
    };

    auto scan_directory(fs::path const& path) -> movie_scanner::scan_result;

    auto create_container(std::u8string_view name) -> std::string;

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
