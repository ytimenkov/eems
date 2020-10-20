add_library(asio)
target_sources(asio PRIVATE
    asio.cpp
    beast.cpp
)
target_compile_definitions(asio PUBLIC
    BOOST_ASIO_SEPARATE_COMPILATION
    BOOST_ASIO_NO_TS_EXECUTORS
    BOOST_ASIO_NO_DEPRECATED
    BOOST_BEAST_USE_STD_STRING_VIEW
    BOOST_BEAST_SEPARATE_COMPILATION
    )
target_link_libraries(asio PUBLIC
    Boost::Boost
    )
