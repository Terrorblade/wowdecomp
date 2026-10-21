file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

if (WOWDECOMP_PLATFORM_LINUX)
    list(FILTER PRIVATE_SOURCES EXCLUDE REGEX "/UILCD/(Windows/|LCD\\.cpp$)")
elseif (NOT WIN32)
    list(FILTER PRIVATE_SOURCES EXCLUDE REGEX "/UILCD/Windows/")
endif ()

add_library(Ui STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Ui
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

if (WIN32)
    target_link_libraries(Ui
        PRIVATE
            lgLcd
    )
endif ()
