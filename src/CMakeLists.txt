add_subdirectory(asio)

add_executable(eems)

target_sources(eems PRIVATE
    directory_service.cpp
    directory_service.h
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
    range-v3::range-v3
    asio
    )
