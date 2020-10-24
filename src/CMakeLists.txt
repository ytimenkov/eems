add_subdirectory(asio)

add_executable(eems)

target_sources(eems PRIVATE
    fs.h
    http_messages.cpp
    http_messages.h
    logging.cpp
    logging.h
    main.cpp
    net.h
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
    skyr::skyr-url
    asio
    )
