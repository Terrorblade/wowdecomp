get_directory_property(SOUNDENGINE_DEFINITIONS COMPILE_DEFINITIONS)
list(REMOVE_ITEM SOUNDENGINE_DEFINITIONS WIN32_LEAN_AND_MEAN NOMINMAX)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${SOUNDENGINE_DEFINITIONS}")

set(FMODEX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fmodex)
set(FMODEX_FLAC_DIR ${FMODEX_DIR}/lib/flac-1.1.4)

if (WOWDECOMP_PLATFORM_POSIX)
    # FMOD OS layer + SDL2 output for Linux (fmodex/linux/src). macOS builds the same OS layer, with a file in
    # fmodex/mac/src replacing the linux/src one of the same name, and outputs to CoreAudio instead of SDL2.
    set(FMODEX_PLATFORM_DIR ${FMODEX_DIR}/linux/src)
    set(FMODEX_PLATFORM_GLOBS "${FMODEX_DIR}/linux/src/*.cpp")

    if (WOWDECOMP_PLATFORM_MAC)
        file(GLOB FMODEX_MAC_SOURCES CONFIGURE_DEPENDS "${FMODEX_DIR}/mac/src/*.cpp")
        file(GLOB FMODEX_LINUX_SOURCES CONFIGURE_DEPENDS "${FMODEX_DIR}/linux/src/*.cpp")
        set(FMODEX_PLATFORM_GLOBS ${FMODEX_MAC_SOURCES})

        foreach (source ${FMODEX_LINUX_SOURCES})
            get_filename_component(name ${source} NAME)

            if (NOT EXISTS "${FMODEX_DIR}/mac/src/${name}")
                list(APPEND FMODEX_PLATFORM_GLOBS ${source})
            endif ()
        endforeach ()
    endif ()
    set(FMODEX_WMSDK_INCLUDE_DIR)
else ()
    set(FMODEX_PLATFORM_DIR ${FMODEX_DIR}/win32/src)
    set(FMODEX_PLATFORM_GLOBS "${FMODEX_DIR}/win32/src/*.cpp" "${FMODEX_DIR}/win32/src/asio/*.cpp")
    set(FMODEX_WMSDK_INCLUDE_DIR ${FMODEX_DIR}/win32/lib/wmsdk/include)
endif ()

file(GLOB FMODEX_SOURCES CONFIGURE_DEPENDS
    "${FMODEX_DIR}/src/*.cpp"
    ${FMODEX_PLATFORM_GLOBS}
    "${FMODEX_DIR}/lib/ogg_vorbis/ogg/src/*.c"
    "${FMODEX_DIR}/lib/ogg_vorbis/vorbis/lib/*.c"
    "${FMODEX_FLAC_DIR}/src/libFLAC/*.c"
    "${FMODEX_DIR}/lib/openal/utils/*.cpp"
    "${FMODEX_DIR}/lib/freeverb/*.cpp"
    "${FMODEX_DIR}/lib/sfx/foreverb/*.cpp"
)
wowdecomp_filter_platform_sources(FMODEX_SOURCES)

if (WOWDECOMP_PLATFORM_POSIX)
    # OpenAL output is part of the Windows output set; Linux outputs live in fmodex/linux/src.
    list(FILTER FMODEX_SOURCES EXCLUDE REGEX "/lib/openal/")
endif ()

add_library(fmodex STATIC ${FMODEX_SOURCES})

target_include_directories(fmodex
    PUBLIC
        ${FMODEX_DIR}/src
    PRIVATE
        ${FMODEX_PLATFORM_DIR}
        ${FMODEX_DIR}/lib/ogg_vorbis/ogg/include
        ${FMODEX_DIR}/lib/ogg_vorbis/vorbis/include
        ${FMODEX_DIR}/lib/ogg_vorbis/vorbis/lib
        ${FMODEX_FLAC_DIR}/include
        ${FMODEX_FLAC_DIR}/src/libFLAC/include
        ${FMODEX_DIR}/lib/openal/include
        ${FMODEX_WMSDK_INCLUDE_DIR}
        ${FMODEX_DIR}/lib/sfx/foreverb
        ${FMODEX_DIR}/lib/freeverb
)

target_compile_definitions(fmodex
    PRIVATE
        FLAC__NO_DLL
        FLAC__HAS_OGG=1
)

if (WOWDECOMP_PLATFORM_LINUX)
    target_link_libraries(fmodex
        PRIVATE
            SDL2::SDL2
            Os
        INTERFACE
            ${CMAKE_DL_LIBS}
    )
elseif (WOWDECOMP_PLATFORM_MAC)
    target_link_libraries(fmodex
        PRIVATE
            Os
        INTERFACE
            "-framework AudioToolbox"
            "-framework AudioUnit"
            "-framework CoreAudio"
            ${CMAKE_DL_LIBS}
    )
else ()
    target_link_libraries(fmodex
        INTERFACE
            winmm
            ws2_32
            msacm32
    )
endif ()

if (MSVC)
    target_compile_options(fmodex PRIVATE /MP)
endif ()

file(GLOB SOUNDENGINE_SOURCES CONFIGURE_DEPENDS "*.cpp")
wowdecomp_filter_platform_sources(SOUNDENGINE_SOURCES)

add_library(SoundEngine STATIC
    ${SOUNDENGINE_SOURCES}
)

target_include_directories(SoundEngine
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(SoundEngine
    PUBLIC
        fmodex
)
