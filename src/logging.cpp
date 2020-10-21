#include "logging.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace eems
{
std::shared_ptr<spdlog::logger> intialize_logging(std::string const& file_name)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    console_sink->set_pattern("%Y-%m-%d %X %^%6l%$: %v");

    // Make this configurable
    constexpr bool truncate_file = true;
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_name, truncate_file);
    file_sink->set_level(spdlog::level::debug);
    file_sink->set_pattern("%Y-%m-%d %X %6l: [%s:%#] %!(): %v");

    std::initializer_list<spdlog::sink_ptr> sinks = {console_sink, file_sink};
    auto new_logger = std::make_shared<spdlog::logger>("basic_logger", sinks);
    new_logger->flush_on(spdlog::level::debug);
    new_logger->set_level(spdlog::level::debug);
    // spdlog::set_default_logger(std::move(new_logger));
    return new_logger;
}
} // namespace eems
