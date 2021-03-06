add_subdirectory(asio)
add_subdirectory(store)
add_subdirectory(scanner)

add_executable(eems)

target_sources(eems PRIVATE
    as_result.h
    config.cpp
    config.h
    content_service.cpp
    content_service.h
    data_config.h
    discovery_service.cpp
    discovery_service.h
    fs.h
    http_messages.cpp
    http_messages.h
    http_serialize.h
    logging_config.h
    logging.cpp
    logging.h
    main.cpp
    net.h
    ranges.h
    server_config.h
    server.cpp
    server.h
    soap.cpp
    soap.h
    spirit.h
    store_config.h
    upnp.cpp
    upnp.h
    xml_serialization.cpp
    xml_serialization.h
    )

target_link_libraries(eems PUBLIC
    std::coroutines
    Boost::headers
    fmt::fmt
    spdlog::spdlog
    pugixml::pugixml
    range-v3::range-v3
    toml11::toml11
    date::date
    asio
    store
    scanner
    PRIVATE
    Boost::program_options
    )
