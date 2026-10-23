file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

# The auth server only accepts builds it knows, set this to the commit it expects when the decomp HEAD is not one of them.
set(HOT_COMMIT_OVERRIDE "" CACHE STRING "40 character commit hash sent to the server instead of the git HEAD")

set(HOT_VERSION_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(HOT_VERSION_HEADER "${HOT_VERSION_DIR}/HoT/BuildVersion.h")
set(HOT_VERSION_ARGS
    "-DHOT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DHOT_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/BuildVersion.h.in"
    "-DHOT_OUTPUT=${HOT_VERSION_HEADER}"
    "-DHOT_COMMIT_OVERRIDE=${HOT_COMMIT_OVERRIDE}"
)
execute_process(COMMAND ${CMAKE_COMMAND} ${HOT_VERSION_ARGS} -P "${CMAKE_CURRENT_SOURCE_DIR}/BuildVersion.cmake")

add_custom_target(HoTBuildVersion
    BYPRODUCTS "${HOT_VERSION_HEADER}"
    COMMAND ${CMAKE_COMMAND} ${HOT_VERSION_ARGS} -P "${CMAKE_CURRENT_SOURCE_DIR}/BuildVersion.cmake"
    COMMENT "Resolving git commit hash for HoT"
    VERBATIM
)

add_library(HoT STATIC
    ${PRIVATE_SOURCES}
)

add_dependencies(HoT HoTBuildVersion)

# The MSDF packer uses SSE4.1 intrinsics unconditionally, MSVC allows that without a flag but clang and gcc don't.
if (NOT MSVC AND NOT WOWDECOMP_CPU_ARM64)
    set_source_files_properties(MSDF/Font.cpp PROPERTIES COMPILE_OPTIONS "-msse4.1")
endif ()

# The game editor UI is compiled into the exe, Resources.cpp serves it to SFile.
file(GLOB_RECURSE HOT_EDITOR_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Interface/*")
set(HOT_EDITOR_UI_SOURCE "${HOT_VERSION_DIR}/HoT/EditorResources.cpp")
add_custom_command(
    OUTPUT "${HOT_EDITOR_UI_SOURCE}"
    COMMAND ${CMAKE_COMMAND} "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/Editor/Interface" "-DOUTPUT=${HOT_EDITOR_UI_SOURCE}" -P "${CMAKE_CURRENT_SOURCE_DIR}/Editor/EmbedResources.cmake"
    DEPENDS ${HOT_EDITOR_UI_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/Editor/EmbedResources.cmake"
    COMMENT "Embedding the game editor UI"
    VERBATIM
)
target_sources(HoT PRIVATE "${HOT_EDITOR_UI_SOURCE}")

# The graphics profiler window is embedded the same way and served as Interface\GraphicsProfiler\...
file(GLOB_RECURSE HOT_PROFILER_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Profiler/Interface/*")
set(HOT_PROFILER_UI_SOURCE "${HOT_VERSION_DIR}/HoT/ProfilerResources.cpp")
add_custom_command(
    OUTPUT "${HOT_PROFILER_UI_SOURCE}"
    COMMAND ${CMAKE_COMMAND} "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/Profiler/Interface" "-DOUTPUT=${HOT_PROFILER_UI_SOURCE}" "-DSYMBOL=g_profilerFiles" "-DCOUNT=g_profilerFileCount" -P "${CMAKE_CURRENT_SOURCE_DIR}/Editor/EmbedResources.cmake"
    DEPENDS ${HOT_PROFILER_UI_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/Editor/EmbedResources.cmake"
    COMMENT "Embedding the graphics profiler UI"
    VERBATIM
)
target_sources(HoT PRIVATE "${HOT_PROFILER_UI_SOURCE}")

target_include_directories(HoT
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${HOT_VERSION_DIR}
)

# The Discord SDK is loaded at runtime, the client just needs it next to the exe.
if (WIN32)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Discord/redist/discord_game_sdk.dll" "${CMAKE_SOURCE_DIR}/bin/Win/discord_game_sdk_64.dll" COPYONLY)
endif ()

# MSDF text, FreeType and msdfgen come from ThirdParty.
target_link_libraries(HoT
    PUBLIC
        freetype
)

if (TARGET msdfgen::msdfgen-ext)
    target_link_libraries(HoT
        PUBLIC
            msdfgen::msdfgen-core
            msdfgen::msdfgen-ext
    )
endif ()

# The model editor decodes imported images through WIC.
if (WIN32)
    target_link_libraries(HoT
        PUBLIC
            windowscodecs
            ole32
    )
endif ()

# The streaming client's HTTP is libcurl everywhere, built from ThirdParty/curl on Windows. SHA-256 and ECDSA come
# from OpenSSL on Linux and from CommonCrypto and Security.framework on macOS, in place of BCrypt.
if (WOWDECOMP_PLATFORM_POSIX)
    find_package(CURL REQUIRED)
endif ()

target_link_libraries(HoT
    PUBLIC
        CURL::libcurl
)

if (WOWDECOMP_PLATFORM_LINUX)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(HoT
        PUBLIC
            OpenSSL::Crypto
    )
elseif (WOWDECOMP_PLATFORM_MAC)
    target_link_libraries(HoT
        PUBLIC
            "-framework Security"
            "-framework CoreFoundation"
    )
endif ()

option(AUTO_UPDATER_IGNORES_EXE "Background downloader leaves the running client exe and discord_game_sdk_64.dll alone" OFF)
if (AUTO_UPDATER_IGNORES_EXE)
    target_compile_definitions(HoT PRIVATE AUTO_UPDATER_IGNORES_EXE)
endif ()
