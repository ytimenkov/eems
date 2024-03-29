add_library(store)

target_sources(store PRIVATE
    fb_converters.h
    fb_vector_view.h
    store_service.cpp
    store_service.h
    )

target_sources(store PRIVATE
    schema.fbs
)

target_link_libraries(store
    PUBLIC
    flatbuffers::libflatbuffers
    leveldb::leveldb
    range-v3::range-v3
    PRIVATE
    spdlog::spdlog
    )

#target_include_directories(store PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

function(add_flatc_commands target)
    get_target_property(srcs ${target} SOURCES)
    get_target_property(src_dir ${target} SOURCE_DIR)
    # get_target_property(bin_dir ${target} BINARY_DIR)
    set(bin_dir ${src_dir})
    foreach(src ${srcs})
        get_filename_component(ext ${src} EXT)
        if(${ext} STREQUAL .fbs)
            get_filename_component(fname ${src} NAME_WE)
            set(generated_name "${fname}_generated.h")
            add_custom_command(OUTPUT ${bin_dir}/${generated_name}
                COMMAND flatbuffers::flatc --scoped-enums --gen-mutable -o ${bin_dir} --cpp ${src}
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
