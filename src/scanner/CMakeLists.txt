add_library(scanner)

target_sources(scanner PRIVATE
    movie_scanner.cpp
    movie_scanner.h
    )

target_link_libraries(scanner
    PUBLIC
    store
    PRIVATE
    Boost::Boost
    fmt::fmt
    spdlog::spdlog
    )

target_include_directories(scanner PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
