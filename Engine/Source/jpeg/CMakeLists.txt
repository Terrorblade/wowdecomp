file(GLOB PRIVATE_SOURCES CONFIGURE_DEPENDS "*.c" "*.cpp")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

set_source_files_properties(${PRIVATE_SOURCES} PROPERTIES LANGUAGE CXX)

add_library(jpeg STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(jpeg
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
