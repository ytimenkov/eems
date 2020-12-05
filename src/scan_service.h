#ifndef EEMS_SCAN_SERVICE_H
#define EEMS_SCAN_SERVICE_H

#include "fs.h"
#include "store/store_service.h"

namespace eems
{
class scan_service
{
public:
    struct scan_result
    {
        std::vector<flatbuffers::DetachedBuffer> items;
        std::vector<fs::path> directories;
    };

    auto scan_directory(fs::path const& path) -> scan_result;

    auto scan_all(fs::path const& root, store_service& store) -> void;

private:
};

}

#endif
