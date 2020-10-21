#include <spdlog/logger.h>
#include <string_view>

namespace eems
{
using logger_sp = std::shared_ptr<spdlog::logger>;
logger_sp intialize_logging(std::string const& file_name);

} // namespace eems
