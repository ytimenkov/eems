#include "logging.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace eems
{
std::shared_ptr<spdlog::logger> intialize_logging(logging_config const& config)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    console_sink->set_pattern("%Y-%m-%d %X %^%6l%$: %v");

    std::vector<spdlog::sink_ptr> sinks;
    sinks.emplace_back(std::move(console_sink));

    spdlog::level::level_enum level{spdlog::level::info};

    if (!config.path.empty())
    {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.path.native(), config.truncate);
        file_sink->set_level(spdlog::level::debug);
        file_sink->set_pattern("%Y-%m-%d %X %6l: [%s:%#] %!(): %v");
        sinks.emplace_back(std::move(file_sink));
        level = spdlog::level::debug;
    }
    auto new_logger = std::make_shared<spdlog::logger>("basic_logger", std::begin(sinks), std::end(sinks));
    new_logger->flush_on(level);
    new_logger->set_level(level);
    return new_logger;
}
}
