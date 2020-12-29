#ifndef EEMS_MOVIE_SCANNER_H
#define EEMS_MOVIE_SCANNER_H

#include "../fs.h"
#include "../store/store_service.h"
#include "../data_config.h"

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

    auto scan_directory(fs::path const& path, int64_t& resource_id) -> movie_scanner::scan_result;

private:
    store_service& store_;
};

}

#endif
