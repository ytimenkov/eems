#include "store_service.h"

#include <spdlog/spdlog.h>

namespace eems
{

namespace
{
template <typename TKey>
auto serialize_key(TKey const& key) -> std::string
{
    auto fbb = flatbuffers::FlatBufferBuilder{};
    auto key_buffer = CreateLibraryKey(fbb, KeyUnionTraits<TKey>::enum_value, fbb.CreateStruct(key).Union());
    fbb.Finish(key_buffer);
    return std::string{reinterpret_cast<char const*>(fbb.GetBufferPointer()), fbb.GetSize()};
}

inline auto ordering_to_int(std::strong_ordering v) -> int
{
    if (v < 0)
        return -1;
    else if (v > 0)
        return +1;
    else
        return 0;
}
}

// Define comparison operators for keys this way because those classes are
// generated and there is no way to add body to them.
inline auto operator<=>(ContainerKey const& lhs, ContainerKey const& rhs)
{
    return lhs.id() <=> rhs.id();
}

inline auto operator<=>(ItemKey const& lhs, ItemKey const& rhs)
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

auto store_service::put_item(ContainerKey parent, flatbuffers::DetachedBuffer&& item) -> void
{
    auto mutator = GetMutableMediaItem(item.data());
    // Super dirty way to generate ID...
    mutator->mutable_id()->mutate_id(++id_);

    // TODO: Need to think whether we want to use fb as a key or maybe its better with a plain struct...
    // It is 3 times  bigger than needed (8+type vs 32 bits) but still fairly small.
    // Main inconvenience is creating an additionall buffer for slice (which for 32 bytes is definitely heap-allocated).
    // Also need to check how well the key works for listings of elements inside container.
    // Also the DB will (most likely) store resources - id to path mappings. They need to be handled as well.

    // On the other hand requesting for long keys (like full path name) is highly desirable
    // and there could be a benefit in using complex keys.
    // Or serializing path will also require a copy and therefore it is still possible to
    // reserve certain prefix except for / (or drive letter) for virtual listings...
    auto key = serialize_key(*mutator->id());
    spdlog::debug("Serialized key of size {}", key.length());

    db_->Put(leveldb::WriteOptions(), key,
             leveldb::Slice{reinterpret_cast<char const*>(item.data()), item.size()});
}

auto store_service::list(ContainerKey id) -> store_service::list_result_view
{
    std::unique_ptr<leveldb::Iterator> it{db_->NewIterator(leveldb::ReadOptions{})};
    it->Seek(serialize_key(ItemKey{0}));
    return list_result_view{std::move(it)};
}

}
