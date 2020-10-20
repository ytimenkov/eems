add_subdirectory(asio)

add_executable(eems)

target_sources(eems PRIVATE
    main.cpp
    )

target_link_libraries(eems PUBLIC
    std::coroutines
    Boost::Boost
    fmt::fmt
    asio
    )
