#include "config.h"

#include "net.h"

#include <boost/asio/ip/host_name.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <fmt/core.h>
#include <toml.hpp>

namespace eems
{
namespace po = boost::program_options;
using namespace std::string_literals;

struct string_hash : std::hash<std::string_view>
{
    using is_transparent = std::true_type;
};

template <typename K, typename V>
using toml_table_container = std::unordered_map<K, V, string_hash, std::equal_to<>>;

using toml_value = toml::basic_value<toml::discard_comments, toml_table_container, std::vector>;
using toml_table = toml_value::table_type;
using toml_array = toml_value::array_type;

template <typename T, typename F>
inline auto try_get(toml_table const& table, std::string const& value, F func)
{
    // TODO: Unfortunately GCC 10's STL doesn't support C++20 transparent key search...
    if (auto it = table.find(value); it != table.end())
    {
        func(toml::get<T>(it->second));
    }
}

auto load_movies_config(toml_table const& data)
    -> movies_library_config
{
    movies_library_config result{};
    try_get<bool>(data, "use_folder_names"s, [&](auto& val) {
        result.use_folder_names = val;
    });
    return result;
}

auto load_data_config(toml_array const& data, data_config& config)
    -> void
{
    for (auto const& table : data)
    {
        auto& type = toml::find<std::string>(table, "type"s);
        auto& path = toml::find<std::string>(table, "path"s);

        if (type == "movies")
        {
            config.content_directories.emplace_back(path, load_movies_config(table.as_table()));
        }
        else
        {
            throw std::runtime_error(fmt::format("Unknown content type at line {}", table.location().line()));
        }
    }
}

auto load_db_config(toml_table const& data, store_config& config)
    -> void
{
    try_get<std::string>(data, "path"s, [&](auto& val) {
        config.db_path = val;
    });
}

auto load_server_config(toml_table const& data, server_config& config)
    -> void
{
    try_get<std::string>(data, "uuid"s, [&](auto& val) {
        config.uuid = boost::uuids::string_generator{}(val);
    });

    try_get<toml::integer>(data, "port"s, [&](auto val) {
        // TODO: check for overflow.
        config.listen_port = val;
    });

    try_get<std::string>(data, "hostname"s, [&](auto& val) {
        config.host_name = val;
    });

    try_get<std::string>(data, "name"s, [&](auto& val) {
        config.name = val;
    });
}

auto load_logging_config(toml_table const& data, logging_config& config)
    -> void
{
    try_get<std::string>(data, "path"s, [&](auto& val) {
        config.path = val;
    });
    try_get<bool>(data, "truncate"s, [&](auto& val) {
        config.truncate = val;
    });
}

auto load_configuration(int argc, char const* argv[])
    -> config
{
    auto desc = po::options_description{"Allowed options"};
    desc.add_options()                                                        //
        ("config,c", po::value<fs::path>()->required(), "Configuration file") //
        ;
    auto vm = po::variables_map{};
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    auto result = config{};
    auto data = toml::parse<toml::discard_comments, toml_table_container>(vm["config"].as<fs::path>());
    auto const& data_table = data.as_table();

    try_get<toml_array>(data_table, "content"s, [&config = result.data](auto& data) {
        load_data_config(data, config);
    });

    try_get<toml_table>(data_table, "db"s, [&config = result.db](auto& data) {
        load_db_config(data, config);
    });

    try_get<toml_table>(data_table, "server"s, [&config = result.server](auto& data) {
        load_server_config(data, config);
    });

    try_get<toml_table>(data_table, "logging"s, [&config = result.logging](auto& data) {
        load_logging_config(data, config);
    });

    // Now we bind to 0.0.0.0 and this must know own remote name or ip address.
    // However there is no standard way in asio to enumerate all interfaces to listen
    // so the best way is to rely on DNS working.
    if (result.server.host_name.empty())
    {
        result.server.host_name = net::ip::host_name();
    }

    if (result.server.name.empty())
    {
        result.server.name = fmt::format("EEMSat {}", result.server.host_name);
    }

    if (result.server.uuid.is_nil())
    {
        result.server.uuid = boost::uuids::name_generator_latest{boost::uuids::ns::dns()}(result.server.host_name);
    }

    return result;
}
}
