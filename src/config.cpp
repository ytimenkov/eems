#include "config.h"

#include "net.h"

#include <boost/asio/ip/host_name.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <toml.hpp>

namespace eems
{
namespace po = boost::program_options;

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
    auto data = toml::parse(vm["config"].as<fs::path>());
    auto const& data_table = data.as_table();

    auto const& content = toml::find<toml::array>(data, "content");
    for (auto const& table : content)
    {
        result.data.content_directories.emplace_back(
            static_cast<std::string const&>(toml::find<toml::string>(table, "path")));
    }
    auto const& db = toml::find(data, "db");
    result.db.path = static_cast<std::string const&>(toml::find<toml::string>(db, "path"));

    if (auto server_it = data_table.find("server"); server_it != data_table.end())
    {
        auto& server = server_it->second.as_table();
        if (auto uuid_it = server.find("uuid"); uuid_it != server.end())
        {
            result.server.uuid = boost::uuids::string_generator{}(static_cast<std::string const&>(toml::get<toml::string>(uuid_it->second)));
        }
    }
    if (result.server.uuid.is_nil())
    {
        result.server.uuid = boost::uuids::name_generator_latest{boost::uuids::ns::dns()}(net::ip::host_name());
    }

    return result;
}
}
