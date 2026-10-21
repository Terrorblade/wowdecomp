file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

add_library(WowServices STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(WowServices
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)
