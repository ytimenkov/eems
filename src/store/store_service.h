#ifndef EEMS_STORE_SERVICE_H
#define EEMS_STORE_SERVICE_H

#include "../config.h"
#include "../ranges.h"
#include "schema_generated.h"

#include <leveldb/comparator.h>
#include <leveldb/db.h>
#include <range/v3/view/facade.hpp>
#include <string_view>

namespace eems
{
class store_service
{
public:
    explicit store_service(db_config const& config);
    ~store_service() noexcept;

    auto put_item(ContainerKey parent, flatbuffers::DetachedBuffer&& item) -> void;

    class list_result_view : public ranges::view_facade<list_result_view>
    {
    public:
        explicit list_result_view(std::unique_ptr<leveldb::Iterator>&& iter)
            : iter_{std::move(iter)}
        {
        }

    private:
        friend ranges::range_access;

        class cursor
        {
        public:
            cursor() noexcept = default;
            explicit cursor(leveldb::Iterator* iter) noexcept
                : iter_{iter} {}

            auto read() const -> MediaItem const&
            {
                return *flatbuffers::GetRoot<MediaItem>(iter_->value().data());
            }

            auto next() noexcept -> void
            {
                iter_->Next();
            }

            auto equal(ranges::default_sentinel_t) const -> bool
            {
                return !iter_->Valid() || flatbuffers::GetRoot<LibraryKey>(iter_->key().data())->key_type() != KeyUnion::ItemKey;
            }

        private:
            leveldb::Iterator* iter_{nullptr};
        };

        auto begin_cursor() const { return cursor{iter_.get()}; }
        auto end_cursor() const { return ranges::default_sentinel; }

        std::unique_ptr<leveldb::Iterator> iter_;
    };

    auto list(ContainerKey id) -> list_result_view;

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
