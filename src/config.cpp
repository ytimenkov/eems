#include "config.h"

#include <boost/program_options.hpp>
#include <toml.hpp>

namespace eems
{
namespace po = boost::program_options;

auto load_configuration(int argc, char const* argv[])
    -> data_config
{
    auto desc = po::options_description{"Allowed options"};
    desc.add_options()                                                        //
        ("config,c", po::value<fs::path>()->required(), "Configuration file") //
        ;
    auto vm = po::variables_map{};
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    auto result = data_config{};
    auto data = toml::parse(vm["config"].as<fs::path>());
    auto const& content = toml::find<toml::array>(data, "content");
    for (auto const& table : content)
    {
        result.content_directories.emplace_back(
            static_cast<std::string const&>(toml::find<toml::string>(table, "path")));
    }
    return result;
}
}
