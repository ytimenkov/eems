add_executable(eems)

target_sources(eems PUBLIC
    main.cpp
    )

target_link_libraries(eems PUBLIC
    std::coroutines
    Boost::Boost
    fmt::fmt
    )
