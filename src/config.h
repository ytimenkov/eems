#ifndef EEMS_CONFIG_H
#define EEMS_CONFIG_H

#include "data_config.h"
#include "server_config.h"
#include "store_config.h"

namespace eems
{
struct config
{
    data_config data;
    store_config db;
    server_config server;
};

auto load_configuration(int argc, char const* argv[])
    -> config;

template <class... Ts>
struct lambda_visitor : Ts...
{
    using Ts::operator()...;
};
// NOTE: cppreference says it's not needed for C++20, but gcc 10 doesn't understand it yet.
template <class... Ts>
lambda_visitor(Ts...) -> lambda_visitor<Ts...>;

}
#endif