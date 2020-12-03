#ifndef EEMS_STORE_SERVICE_H
#define EEMS_STORE_SERVICE_H

#include "schema_generated.h"

#include <string_view>

namespace eems
{
class store_service
{
public:
    auto put_item(ContainerKey parent, flatbuffers::FlatBufferBuilder&& item) -> void;
    auto list(ContainerKey id) -> std::vector<MediaItem const*>;

private:
    std::vector<flatbuffers::FlatBufferBuilder> items_;
};
}

#endif
