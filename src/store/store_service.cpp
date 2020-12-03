#include "store_service.h"

#include "../ranges.h"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace eems
{
auto store_service::put_item(ContainerKey parent, flatbuffers::FlatBufferBuilder&& item) -> void
{
    auto mutator = GetMutableMediaItem(item.GetBufferPointer());
    // Super dirty way to generate ID...
    mutator->mutable_id()->mutate_id(items_.size() + 1);
    items_.emplace_back(std::move(item));
}

auto store_service::list(ContainerKey id) -> std::vector<MediaItem const*>
{
    return items_ | views::transform([](flatbuffers::FlatBufferBuilder const& builder) {
               return flatbuffers::GetRoot<MediaItem>(builder.GetBufferPointer());
           }) |
           ranges::to<std::vector>();
}

}
