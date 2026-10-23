add_library(zlib STATIC
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/gzio.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/uncompr.c
    zlib/zutil.c
)

target_include_directories(zlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zlib
)

add_library(blast STATIC
    zlib/contrib/blast/blast.c
)

target_include_directories(blast
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zlib/contrib/blast
)

add_library(bzip2 STATIC
    bzip2/blocksort.c
    bzip2/bzlib.c
    bzip2/compress.c
    bzip2/crctable.c
    bzip2/decompress.c
    bzip2/huffman.c
    bzip2/randtable.c
)

target_include_directories(bzip2
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/bzip2
)

add_library(lzma STATIC
    lzma/LzmaDecode.c
)

target_include_directories(lzma
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lzma
)

if (WIN32)
    add_library(nvapi INTERFACE)

    target_include_directories(nvapi
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/nvapi
    )

    target_link_libraries(nvapi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/nvapi/amd64/nvapi64.lib)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    # Linux renders Direct3D 9 on Vulkan through dxvk. The headers are vendored, the prebuilt library ships next
    # to the client and is loaded at runtime.
    add_library(dxvk_native INTERFACE)
    target_include_directories(dxvk_native INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/dxvk-native/include)

    set(DXVK_VERSION 3.1.1)
    set(DXVK_ARCHIVE ${CMAKE_BINARY_DIR}/dxvk-native/dxvk-native-${DXVK_VERSION}-steamrt-sniper.tar.gz)
    set(DXVK_D3D9_LIBRARY ${CMAKE_BINARY_DIR}/dxvk-native/usr/lib/libdxvk_d3d9.so.0.30101)

    if (NOT EXISTS ${DXVK_D3D9_LIBRARY})
        file(DOWNLOAD
            https://github.com/doitsujin/dxvk/releases/download/v${DXVK_VERSION}/dxvk-native-${DXVK_VERSION}-steamrt-sniper.tar.gz
            ${DXVK_ARCHIVE}
            EXPECTED_HASH SHA256=506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7
            SHOW_PROGRESS
        )
        file(ARCHIVE_EXTRACT INPUT ${DXVK_ARCHIVE} DESTINATION ${CMAKE_BINARY_DIR}/dxvk-native PATTERNS usr/lib/libdxvk_d3d9.so.0.30101)
    endif ()

    set(WOWDECOMP_DXVK_D3D9_LIBRARY ${DXVK_D3D9_LIBRARY} CACHE INTERNAL "")
endif ()

# FIX: HoT MSDF text needs a modern FreeType and msdfgen, the client's 2.0 copy stays in freetype/ unused.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
add_subdirectory(freetype-2.14.1 EXCLUDE_FROM_ALL)
if (NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif ()

set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
add_subdirectory(msdfgen EXCLUDE_FROM_ALL)

# Controller: gamepad input comes from SDL3, built static with only the joystick side enabled.
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_UNINSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_VIDEO OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
    set(SDL_HAPTIC ON CACHE BOOL "" FORCE)
    set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
    set(SDL_POWER ON CACHE BOOL "" FORCE)
    set(SDL_SENSOR ON CACHE BOOL "" FORCE)
    # Video is off, so the X11/Wayland check SDL does on Unix is moot.
    set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
    add_subdirectory(SDL EXCLUDE_FROM_ALL)
else ()
    message(WARNING "ThirdParty/SDL is missing, run git submodule update --init. Gamepad support is disabled.")
endif ()

# HoT streaming: Windows builds libcurl from source with Schannel, Linux and macOS use the system libcurl.
if (WIN32)
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/curl/CMakeLists.txt)
        message(FATAL_ERROR "ThirdParty/curl is missing, run git submodule update --init ThirdParty/curl.")
    endif ()

    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
    set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(CURL_USE_PKGCONFIG OFF CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE STRING "" FORCE)
    set(CURL_BROTLI OFF CACHE STRING "" FORCE)
    set(CURL_ZSTD OFF CACHE STRING "" FORCE)
    set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
    set(USE_WIN32_IDN OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_GSSAPI OFF CACHE BOOL "" FORCE)
    add_subdirectory(curl EXCLUDE_FROM_ALL)
endif ()

add_library(expat STATIC
    expat/lib/xmlparse.c
    expat/lib/xmlrole.c
    expat/lib/xmltok.c
)

target_compile_definitions(expat
    PUBLIC
        _STATIC
    PRIVATE
        COMPILED_FROM_DSP
        _LIB
)

target_include_directories(expat
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/expat/lib
)

add_library(pcre STATIC
    pcre/pcre.c
    pcre/get.c
    pcre/study.c
    pcre/maketables.c
)

target_compile_definitions(pcre
    PUBLIC
        PCRE_STATIC
    PRIVATE
        SUPPORT_UTF8
        SUPPORT_UCP
)

target_include_directories(pcre
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/pcre
)

add_library(DivxDecoder INTERFACE)

target_include_directories(DivxDecoder
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/DivxDecoder
)

target_link_libraries(DivxDecoder
    INTERFACE
        XvidDecoder
)

if (WIN32)
    add_library(lgLcd STATIC
        lgLcd/lglcd.cpp
    )

    target_include_directories(lgLcd
        PUBLIC
            ${CMAKE_SOURCE_DIR}
    )
endif ()
