#ifndef EEMS_STORE_SERVICE_H
#define EEMS_STORE_SERVICE_H

#include "../ranges.h"
#include "../store_config.h"
#include "schema_generated.h"

#include <leveldb/comparator.h>
#include <leveldb/db.h>
#include <range/v3/view/facade.hpp>
#include <string_view>

namespace eems
{

constexpr auto upnp_container_class{u8"object.container"};

struct container_meta
{
    ObjectKey id;
    ObjectKey parent_id;
    std::u8string dc_title;
    std::u8string upnp_class;
    std::vector<std::tuple<std::string, ArtworkType>> artwork;
};

auto serialize_container(std::vector<std::string> const& contents, container_meta const& meta)
    -> flatbuffers::DetachedBuffer;

class store_service
{
public:
    ~store_service() noexcept;

    template <typename TKey>
    auto get_next_id() const -> int64_t;

    auto put_items(ObjectKey parent,
                   std::vector<flatbuffers::DetachedBuffer>&& items,
                   std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>>&& resources) -> void;

    class list_result_view : public ranges::view_facade<list_result_view>
    {
    public:
        explicit list_result_view(std::unique_ptr<leveldb::Iterator>&& db_it, std::vector<std::string>&& objects)
            : db_it_{std::move(db_it)},
              objects_{std::move(objects)}
        {
        }

    private:
        friend ranges::range_access;

        class cursor
        {
        public:
            cursor() noexcept = default;
            explicit cursor(std::vector<std::string>::const_iterator key_it, leveldb::Iterator* db_it) noexcept
                : key_it_{key_it}, db_it_{db_it} {}

            auto read() const -> MediaObject const&;

            auto next() noexcept -> void
            {
                ++key_it_;
            }

            auto prev() noexcept -> void
            {
                --key_it_;
            }

            auto equal(cursor const& other) const noexcept -> bool
            {
                return key_it_ == other.key_it_;
            }

            auto distance_to(cursor const& other) const
            {
                return ranges::distance(key_it_, other.key_it_);
            }

            auto advance(ranges::iter_difference_t<std::vector<std::string>> n) noexcept -> void
            {
                return ranges::advance(key_it_, n);
            }

        private:
            std::vector<std::string>::const_iterator key_it_{};
            leveldb::Iterator* db_it_{nullptr};
        };

        auto begin_cursor() const { return cursor{objects_.begin(), db_it_.get()}; }
        auto end_cursor() const { return cursor{objects_.end(), nullptr}; }

        std::unique_ptr<leveldb::Iterator> db_it_;
        std::vector<std::string> objects_;
    };

    auto list(ObjectKey id) -> list_result_view;
    auto get(ObjectKey id) -> list_result_view;

    struct resource_result
    {
        Resource const* resource;
        std::unique_ptr<leveldb::Iterator> buffer;
    };

    auto get_resource(ResourceKey id) -> resource_result;

    auto open_db(store_config const& config) -> bool;

private:
    struct fb_comparator final : leveldb::Comparator
    {
        int Compare(leveldb::Slice const& lhs_s, leveldb::Slice const& rhs_s) const override;
        const char* Name() const override { return "FlatBufferKeyComparator"; }
        void FindShortestSeparator(std::string* start, leveldb::Slice const& limit) const override {}
        void FindShortSuccessor(std::string* key) const override {}
    };

    auto create_iterator() const -> std::unique_ptr<::leveldb::Iterator>;

    auto deserialize_container(std::string const& key, ::leveldb::Iterator& iter, bool meta)
        -> std::tuple<std::vector<std::string>, std::unique_ptr<container_meta>>;

private:
    fb_comparator comparator_;
    std::unique_ptr<::leveldb::DB> db_;
    int64_t id_{0};
};
}

#endif
