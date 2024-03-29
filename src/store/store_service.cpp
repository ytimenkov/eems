#include "store_service.h"

#include "../ranges.h"
#include "fb_converters.h"

#include <leveldb/write_batch.h>
#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
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
}

// Define comparison operators for keys this way because those classes are
// generated and there is no way to add body to them.
inline auto operator<=>(ObjectKey const& lhs, ObjectKey const& rhs)
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
    auto const cmp = [&]()
    {
        switch (lhs->key_type())
        {
        case KeyUnion::ObjectKey:
            return *lhs->key_as<ObjectKey>() <=> *rhs->key_as<ObjectKey>();
        case KeyUnion::ResourceKey:
            return *lhs->key_as<ResourceKey>() <=> *rhs->key_as<ResourceKey>();
        case KeyUnion::NONE:
            spdlog::error("Unexpected key type: {}", fmt::underlying(lhs->key_type()));
        }
        return std::strong_ordering::equivalent;
    }();
    return ordering_to_int(cmp);
}

auto store_service::open_db(store_config const& config)
    -> bool
{
    leveldb::DB* db = nullptr;
    leveldb::Options options{};
    options.comparator = &comparator_;
    leveldb::Status status = leveldb::DB::Open(options, config.db_path.native(), &db);
    if (status.ok())
    {
        db_.reset(db);
        spdlog::info("Opening existing DB");
        // TODO: Validate DB.
        return true;
    }
    else if (!status.IsInvalidArgument())
    {
        spdlog::error("Failed to open/create DB: {}", status.ToString());
        throw std::runtime_error(status.ToString());
    }

    spdlog::info("Initializing new DB");

    options.create_if_missing = true;
    options.error_if_exists = true;
    status = leveldb::DB::Open(options, config.db_path.native(), &db);
    db_.reset(db);

    if (!status.ok())
    {
        spdlog::error("Failed to open/create DB: {}", status.ToString());
        throw std::runtime_error(status.ToString());
    }

    container_meta root_container{
        .id{0},
        .parent_id{-1},
        .upnp_class{upnp_container_class}};
    auto container_buf = serialize_container({}, root_container);
    status = db_->Put(leveldb::WriteOptions{},
                      serialize_key(ObjectKey{0}),
                      leveldb::Slice{reinterpret_cast<char const*>(container_buf.data()), container_buf.size()});
    if (!status.ok())
    {
        throw std::runtime_error("Failed to create root container");
    }
    return false;
}

store_service::~store_service() noexcept = default;

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
        spdlog::debug("At key type {}, Seeking to previous", fmt::underlying(flatbuffers::GetRoot<LibraryKey>(it->key().data())->key_type()));
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
        spdlog::debug("Sought to: {}, no keys with requested type", fmt::underlying(key->key_type()));
        return 0;
    }
    return key->key_as<TKey>()->id() + 1;
}

template auto store_service::get_next_id<ResourceKey>() const -> int64_t;
template auto store_service::get_next_id<ObjectKey>() const -> int64_t;

inline auto store_service::create_iterator() const -> std::unique_ptr<::leveldb::Iterator>
{
    return std::unique_ptr<leveldb::Iterator>{db_->NewIterator(leveldb::ReadOptions{})};
}

auto store_service::put_items(ObjectKey parent,
                              std::vector<flatbuffers::DetachedBuffer>&& items,
                              std::vector<std::tuple<ResourceKey, flatbuffers::DetachedBuffer>>&& resources)
    -> void
{
    auto const parent_key = serialize_key(parent);
    auto [container_data, container_meta] = deserialize_container(parent_key, *create_iterator(), true);

    // Process resources first because items' references to resources need to be updated.
    leveldb::WriteBatch batch{};

    for (auto&& [res_key, res_buf] : resources)
    {
        batch.Put(
            serialize_key(res_key),
            leveldb::Slice{reinterpret_cast<char const*>(res_buf.data()), res_buf.size()});
    }

    for (auto&& item_buf : items)
    {
        auto item = flatbuffers::GetRoot<MediaObject>(item_buf.data());
        spdlog::info("Adding new item with key {} (parent {}), name: {}",
                     item->id()->id(), parent.id(), as_string_view<char>(*item->dc_title()));
        // TODO: parent id parameter is not needed
        if (item->parent_id()->id() != parent.id())
            throw std::logic_error("Parent mismatch");

        auto key = serialize_key(*item->id());
        batch.Put(
            key,
            leveldb::Slice{reinterpret_cast<char const*>(item_buf.data()), item_buf.size()});

        container_data.emplace_back(std::move(key));
    }

    auto container_buf = serialize_container(container_data, *container_meta);
    batch.Put(parent_key,
              leveldb::Slice{reinterpret_cast<char const*>(container_buf.data()), container_buf.size()});

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

auto store_service::list_result_view::cursor::read() const
    -> MediaObject const&
{
    db_it_->Seek(*key_it_);
    if (!db_it_->Valid() || db_it_->key() != *key_it_)
    {
        spdlog::error("Inconsistent directory: {}", *key_it_);
        throw std::runtime_error("DB state is corrupted: non-existing element");
    }
    return *flatbuffers::GetRoot<MediaObject>(db_it_->value().data());
}

auto store_service::list(ObjectKey id)
    -> store_service::list_result_view
{
    auto it = create_iterator();
    auto [objects, meta] = deserialize_container(serialize_key(id), *it, false);

    // static_assert(ranges::random_access_iterator<decltype(ranges::begin(std::declval<list_result_view>()))>);

    return list_result_view{std::move(it), std::move(objects)};
}

auto store_service::get(ObjectKey id)
    -> store_service::list_result_view
{
    return list_result_view{create_iterator(), {serialize_key(id)}};
}

auto store_service::get_resource(ResourceKey id)
    -> resource_result
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

auto store_service::deserialize_container(std::string const& key, ::leveldb::Iterator& iter, bool include_meta)
    -> std::tuple<std::vector<std::string>, std::unique_ptr<container_meta>>
{
    iter.Seek(key);
    if (!iter.Valid() || iter.key() != key)
    {
        throw std::runtime_error("Container not found");
    }
    auto media_object = flatbuffers::GetRoot<MediaObject>(iter.value().data());
    if (media_object->data_type() != ObjectUnion::MediaContainer)
    {
        throw std::runtime_error(fmt::format("Object is not a container. type={}", fmt::underlying(media_object->data_type())));
    }
    auto container = static_cast<MediaContainer const*>(media_object->data());

    std::unique_ptr<container_meta> meta{};

    if (include_meta)
    {
        meta = std::make_unique<container_meta>();

        meta->id = *media_object->id();
        meta->parent_id = *media_object->parent_id();
        meta->dc_title = as_string_view<char8_t>(*media_object->dc_title());
        meta->upnp_class = as_string_view<char8_t>(*media_object->upnp_class());

        if (auto artwork = media_object->artwork(); artwork)
        {
            ranges::push_back(meta->artwork,
                              views::transform(*artwork, [](Artwork const* item)
                                               { return std::tuple{as_library_key(*item->ref()), item->type()}; }));
        }
    }

    std::vector<std::string> container_data;
    if (auto objects = container->objects(); objects)
    {
        ranges::push_back(container_data,
                          views::transform(*objects, [](MediaObjectRef const* ref)
                                           { return as_library_key(*ref->ref()); }));
    }
    return {std::move(container_data), std::move(meta)};
}

auto serialize_container(std::vector<std::string> const& contents, container_meta const& meta)
    -> flatbuffers::DetachedBuffer
{
    flatbuffers::FlatBufferBuilder fbb{};

    auto container_off = CreateMediaContainer(
        fbb,
        fbb.CreateVector(
            contents |
            views::transform([&fbb](auto const& key_buf)
                             { return CreateMediaObjectRef(fbb, CreateLibraryKey(fbb, key_buf)); }) |
            ranges::to<std::vector>()));

    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Artwork>>> artwork_off{};
    if (!meta.artwork.empty())
    {
        artwork_off = fbb.CreateVector(
            meta.artwork | views::transform([&fbb](auto const& tup)
                                            { return CreateArtwork(fbb, CreateLibraryKey(fbb, std::get<0>(tup)), std::get<1>(tup)); }) |
            ranges::to<std::vector>());
    }

    auto title_off = put_string(meta.dc_title, fbb);
    auto class_off = put_string(meta.upnp_class, fbb);

    MediaObjectBuilder builder{fbb};
    builder.add_id(&meta.id);
    builder.add_parent_id(&meta.parent_id);
    builder.add_dc_title(title_off);
    builder.add_upnp_class(class_off);
    builder.add_artwork(artwork_off);
    builder.add_data_type(ObjectUnion::MediaContainer);
    builder.add_data(container_off.Union());

    fbb.Finish(builder.Finish());
    return fbb.Release();
}

}
