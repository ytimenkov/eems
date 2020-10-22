add_subdirectory(asio)

add_executable(eems)

target_sources(eems PRIVATE
    logging.cpp
    logging.h
    main.cpp
    server.cpp
    server.h
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
