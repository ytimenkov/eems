#ifndef EEMS_STORE_SERVICE_H
#define EEMS_STORE_SERVICE_H

#include "../config.h"
#include "schema_generated.h"

#include <leveldb/comparator.h>
#include <leveldb/db.h>
#include <string_view>

namespace eems
{
class store_service
{
public:
    explicit store_service(db_config const& config);
    ~store_service() noexcept;

    auto put_item(ContainerKey parent, flatbuffers::DetachedBuffer&& item) -> void;

    struct list_result
    {
        std::vector<MediaItem const*> items;
        std::unique_ptr<leveldb::Iterator> memory;
    };

    auto list(ContainerKey id) -> list_result;

private:
    struct fb_comparator final : leveldb::Comparator
    {
        int Compare(leveldb::Slice const& lhs_s, leveldb::Slice const& rhs_s) const override;
        const char* Name() const override { return "FlatBufferKeyComparator"; }
        void FindShortestSeparator(std::string* start, leveldb::Slice const& limit) const override {}
        void FindShortSuccessor(std::string* key) const override {}
    };

private:
    fb_comparator comparator_;
    std::unique_ptr<::leveldb::DB> db_;
    int64_t id_{0};
};
}

#endif
