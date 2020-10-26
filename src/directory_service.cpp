#include "directory_service.h"

namespace eems
{
auto directory_service::browse(std::string_view id, browse_flag mode) -> std::vector<directory_element>
{
    return {
        container{"1", "0", "video", "object.container"},
        item{"2", "0", "Movie", "object.item.videoItem"},
    };
}

}
