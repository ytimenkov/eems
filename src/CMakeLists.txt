add_subdirectory(asio)
add_subdirectory(store)

add_executable(eems)

target_sources(eems PRIVATE
    config.cpp
    config.h
    content_service.cpp
    content_service.h
    discovery_service.cpp
    discovery_service.h
    fs.h
    http_messages.cpp
    http_messages.h
    http_serialize.h
    logging.cpp
    logging.h
    main.cpp
    net.h
    scan_service.cpp
    scan_service.h
    server.cpp
    server.h
    soap.cpp
    soap.h
    upnp.cpp
    upnp.h
    xml_serialization.cpp
    xml_serialization.h
    )

target_link_libraries(eems PUBLIC
    std::coroutines
    Boost::Boost
    fmt::fmt
    spdlog::spdlog
    pugixml::pugixml
    range-v3::range-v3
    toml11::toml11
    asio
    store
    )
