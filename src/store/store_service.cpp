#include "store_service.h"

#include "../ranges.h"

#include <leveldb/write_batch.h>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

namespace eems
{

namespace
{
inline auto ordering_to_int(std::strong_ordering v) -> int
{
    if (v < 0)
        return -1;
    else if (v > 0)
        return +1;
    else
        return 0;
}

template <typename T>
class mutable_vector_view : public ranges::view_facade<mutable_vector_view<T>>
{
public:
    mutable_vector_view() = default;
    explicit mutable_vector_view(flatbuffers::Vector<flatbuffers::Offset<T>>* data)
        : data_{data}
    {
    }

private:
    friend ranges::range_access;

    class cursor
    {
    public:
        cursor() noexcept = default;
        explicit cursor(flatbuffers::Vector<flatbuffers::Offset<T>>::const_iterator it) noexcept
            : it_{it} {}

        auto read() const -> T& { return const_cast<T&>(**it_); }
        auto next() noexcept -> void { ++it_; }
        auto equal(const cursor& rhs) const -> bool { return it_ == rhs.it_; }

    private:
        flatbuffers::Vector<flatbuffers::Offset<T>>::const_iterator it_;
    };

    auto begin_cursor() const { return data_ ? cursor{data_->cbegin()} : cursor{}; }
    auto end_cursor() const { return data_ ? cursor{data_->cend()} : cursor{}; }

private:
    flatbuffers::Vector<flatbuffers::Offset<T>>* data_{nullptr};
};
}

// Define comparison operators for keys this way because those classes are
// generated and there is no way to add body to them.
inline auto
operator<=>(ContainerKey const& lhs, ContainerKey const& rhs)
{
    return lhs.id() <=> rhs.id();
}

inline auto operator<=>(ItemKey const& lhs, ItemKey const& rhs)
{
    return lhs.id() <=> rhs.id();
}

inline auto operator<=>(ResourceKey const& lhs, ResourceKey const& rhs)
{
    return lhs.id() <=> rhs.id();
}

int store_service::fb_comparator::Compare(leveldb::Slice const& lhs_s, leveldb::Slice const& rhs_s) const
{
    auto const lhs = flatbuffers::GetRoot<LibraryKey>(lhs_s.data());
    auto const rhs = flatbuffers::GetRoot<LibraryKey>(rhs_s.data());
    if (auto cmp = lhs->key_type() <=> rhs->key_type(); cmp != 0)
    {
        return ordering_to_int(cmp);
    }
    auto const cmp = [&]() {
        switch (lhs->key_type())
        {
        case KeyUnion::ContainerKey:
            return *lhs->key_as<ContainerKey>() <=> *rhs->key_as<ContainerKey>();
        case KeyUnion::ItemKey:
            return *lhs->key_as<ItemKey>() <=> *rhs->key_as<ItemKey>();
        case KeyUnion::ResourceKey:
            return *lhs->key_as<ResourceKey>() <=> *rhs->key_as<ResourceKey>();
        case KeyUnion::NONE:
            spdlog::error("Unexpected key type: {}", lhs->key_type());
        }
        return std::strong_ordering::equivalent;
    }();
    return ordering_to_int(cmp);
}

store_service::store_service(db_config const& config)

{
    leveldb::DB* db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    // While developing...
    options.error_if_exists = true;
    options.comparator = &comparator_;
    leveldb::Status status = leveldb::DB::Open(options, config.path.native(), &db);
    db_.reset(db);
    if (!status.ok())
    {
        spdlog::error("Failed to open/create DB: {}", status.ToString());
        throw std::runtime_error(status.ToString());
    }
}

store_service::~store_service() = default;

template <typename TKey>
inline auto store_service::get_next_id() const -> int64_t
{
    // TODO: Need to create proper unit tests for it.
    auto it = create_iterator();
    it->Seek(serialize_key(TKey{std::numeric_limits<int64_t>::max()}));
    if (!it->Valid())
    {
        spdlog::debug("No keys, seeking to last");
        it->SeekToLast();
    }
    else
    {
        spdlog::debug("At key type {}, Seeking to previous", flatbuffers::GetRoot<LibraryKey>(it->key().data())->key_type());
        it->Prev();
    }
    if (!it->Valid())
    {
        spdlog::debug("No key with requested type");
        return 0;
    }
    auto key = flatbuffers::GetRoot<LibraryKey>(it->key().data());
    if (key->key_type() != KeyUnionTraits<TKey>::enum_value)
    {
        spdlog::debug("Sought to: {}, no keys with requested type", key->key_type());
        return 0;
    }
    return key->key_as<TKey>()->id() + 1;
}

template auto store_service::get_next_id<ResourceKey>() const -> int64_t;

inline auto store_service::create_iterator() const -> std::unique_ptr<::leveldb::Iterator>
{
    return std::unique_ptr<leveldb::Iterator>{db_->NewIterator(leveldb::ReadOptions{})};
}

auto store_service::put_items(ContainerKey parent,
                              std::vector<flatbuffers::DetachedBuffer>&& items,
                              std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>>&& resources) -> void
{
    // Process resources first because items' references to resources need to be updated.
    leveldb::WriteBatch batch{};

    for (auto&& [res_key, res_buf] : resources)
    {
        batch.Put(
            serialize_key(res_key),
            leveldb::Slice{reinterpret_cast<char const*>(res_buf.data()), res_buf.size()});
    }

    for (auto&& [item_buf, item_id] : views::zip(items, views::iota(get_next_id<ItemKey>())))
    {
        auto item = GetMutableMediaItem(item_buf.data());
        item->mutable_id()->mutate_id(item_id);

        spdlog::debug("Adding new item with key: {}", item_id);

        batch.Put(
            serialize_key(ItemKey{item_id}),
            leveldb::Slice{reinterpret_cast<char const*>(item_buf.data()), item_buf.size()});
    }
    auto status = db_->Write(leveldb::WriteOptions{}, &batch);
    if (!status.ok())
    {
        spdlog::error("Failed to commit DB transaction: {}", status.ToString());
    }

    // TODO: Need to think whether we want to use fb as a key or maybe its better with a plain struct...
    // It is 3 times  bigger than needed (8+type vs 32 bits) but still fairly small.
    // Main inconvenience is creating an additionall buffer for slice (which for 32 bytes is definitely heap-allocated).
    // Also need to check how well the key works for listings of elements inside container.
    // Also the DB will (most likely) store resources - id to path mappings. They need to be handled as well.

    // On the other hand requesting for long keys (like full path name) is highly desirable
    // and there could be a benefit in using complex keys.
    // Or serializing path will also require a copy and therefore it is still possible to
    // reserve certain prefix except for / (or drive letter) for virtual listings...
}

auto store_service::list(ContainerKey id) -> store_service::list_result_view
{
    auto it = create_iterator();
    it->Seek(serialize_key(ItemKey{0}));
    return list_result_view{std::move(it)};
}

auto store_service::get_resource(ResourceKey id) -> resource_result
{
    resource_result result{nullptr, create_iterator()};
    auto key_s = serialize_key(id);
    result.buffer->Seek(key_s);
    if (!result.buffer->Valid() || result.buffer->key() != key_s)
    {
        result.buffer.reset();
    }
    else
    {
        result.resource = flatbuffers::GetRoot<Resource>(result.buffer->value().data());
    }

    return result;
}

}
