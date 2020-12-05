add_library(store)

find_program(FLATC_PATH flatc)

target_sources(store PRIVATE
    store_service.cpp
    store_service.h
    )

target_sources(store PRIVATE
    schema.fbs
)

target_link_libraries(store
    PUBLIC
    flatbuffers::flatbuffers
    leveldb::leveldb
    PRIVATE
    range-v3::range-v3
    spdlog::spdlog
    )

target_include_directories(store PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

function(add_flatc_commands target)
    get_target_property(srcs ${target} SOURCES)
    get_target_property(src_dir ${target} SOURCE_DIR)
    get_target_property(bin_dir ${target} BINARY_DIR)
    foreach(src ${srcs})
        get_filename_component(ext ${src} EXT)
        if(${ext} STREQUAL .fbs)
            get_filename_component(fname ${src} NAME_WE)
            set(generated_name "${fname}_generated.h")
            add_custom_command(OUTPUT ${generated_name}
                COMMAND ${FLATC_PATH} --scoped-enums --gen-mutable -o ${bin_dir} --cpp ${src}
                MAIN_DEPENDENCY ${src}
                WORKING_DIRECTORY ${src_dir}
                VERBATIM
                )
            target_sources(${target} PRIVATE ${generated_name})
            set_property(SOURCE ${generated_name} PROPERTY GENERATED TRUE)
        endif()
    endforeach(src srcs)
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

add_flatc_commands(store)
