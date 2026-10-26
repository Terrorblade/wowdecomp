file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

add_library(Storm STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Storm
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Storm
    PUBLIC
        blast
        bzip2
        zlib
)

if (WOWDECOMP_PLATFORM_MAC)
    target_link_libraries(Storm
        PUBLIC
            "-framework CoreFoundation"
    )
endif ()
