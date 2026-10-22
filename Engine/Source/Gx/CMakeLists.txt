file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.mm")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

if (WOWDECOMP_PLATFORM_POSIX)
    list(FILTER PRIVATE_SOURCES EXCLUDE REGEX "/CGxDevice(D3d|D3d9Ex)/")
elseif (NOT WIN32)
    list(FILTER PRIVATE_SOURCES EXCLUDE REGEX "/CGxDevice(D3d|D3d9Ex|OpenGL)/")
endif ()

add_library(Gx STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Gx
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

if (WOWDECOMP_PLATFORM_LINUX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Gx
        PUBLIC
            SDL2::SDL2
            OpenGL::GL
    )
elseif (WOWDECOMP_PLATFORM_MAC)
    target_link_libraries(Gx
        PUBLIC
            "-framework AppKit"
            "-framework CoreGraphics"
            "-framework Metal"
            "-framework QuartzCore"
    )
else ()
    target_link_libraries(Gx
        PRIVATE
            d3d9.lib
    )
endif ()

if (WIN32)
    target_link_libraries(Gx
        PRIVATE
            nvapi
            opengl32.lib
    )
endif ()
