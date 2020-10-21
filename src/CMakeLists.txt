add_subdirectory(asio)

add_executable(eems)

target_sources(eems PRIVATE
    main.cpp
    logging.cpp
    )

target_link_libraries(eems PUBLIC
    std::coroutines
    Boost::Boost
    fmt::fmt
    spdlog::spdlog
    pugixml::pugixml
    asio
    )
