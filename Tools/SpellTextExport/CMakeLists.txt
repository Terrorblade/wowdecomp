# Writes to MySQL through MariaDB Connector/C, the tool is skipped without it.
set(MARIADB_ROOT "" CACHE PATH "MariaDB or Connector/C install holding include/mysql and lib/mariadbclient.lib, for SpellTextExport")
find_path(MARIADB_INCLUDE_DIR mysql.h HINTS ${MARIADB_ROOT}/include/mysql ${MARIADB_ROOT}/include/mariadb ${MARIADB_ROOT}/include)
find_library(MARIADB_CLIENT_LIBRARY NAMES mariadbclient HINTS ${MARIADB_ROOT}/lib ${MARIADB_ROOT}/lib/mariadb)

if (NOT MARIADB_INCLUDE_DIR OR NOT MARIADB_CLIENT_LIBRARY)
    message(STATUS "SpellTextExport: MariaDB Connector/C not found, set MARIADB_ROOT to build it")
    return()
endif ()

get_filename_component(MARIADB_LIBRARY_DIR ${MARIADB_CLIENT_LIBRARY} DIRECTORY)
set(MARIADB_SHA2_PLUGIN ${MARIADB_LIBRARY_DIR}/plugin/caching_sha2_password.dll)

if (NOT EXISTS ${MARIADB_SHA2_PLUGIN})
    message(STATUS "SpellTextExport: ${MARIADB_SHA2_PLUGIN} not found, the tool is skipped")
    return()
endif ()

# MySQL 8 logins need this connector plugin DLL, it is embedded so the exe runs on its own.
set(SHA2_PLUGIN_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/CachingSha2Plugin.h)
file(READ ${MARIADB_SHA2_PLUGIN} SHA2_PLUGIN_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," SHA2_PLUGIN_BYTES ${SHA2_PLUGIN_HEX})
file(WRITE ${SHA2_PLUGIN_HEADER} "static const unsigned char s_sha2Plugin[] = {${SHA2_PLUGIN_BYTES}};\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MARIADB_SHA2_PLUGIN})

# The client's top-level sources hold globals the libraries reference, WinMain stays out.
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/WoW/Source/*.cpp")
wowdecomp_filter_platform_sources(CLIENT_SOURCES)

add_executable(SpellTextExport SpellTextExport.cpp ${CLIENT_SOURCES} ${CMAKE_SOURCE_DIR}/WoW/Source/Win64/BotCheck.cpp)

set_target_properties(SpellTextExport PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/Tools
)

foreach (config ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
    string(TOUPPER ${config} config)
    set_target_properties(SpellTextExport PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_SOURCE_DIR}/bin/Tools)
endforeach ()

target_include_directories(SpellTextExport
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${MARIADB_INCLUDE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Movies are never played, so the tool runs without XvidDecoder.dll beside it.
target_link_options(SpellTextExport PRIVATE "/DELAYLOAD:XvidDecoder.dll")

# The parser reaches most of the client, so it links what the game does.
target_link_libraries(SpellTextExport
    PRIVATE
        BLPFile
        Base
        BlizzardCore
        ComSat
        ConsoleBase
        Event
        FFX
        Frame
        FrameScript
        lua
        FrameXML
        Gx
        Gxu
        Images
        Model2
        Os
        Scrn
        Services
        ShaderEffect
        Tempest
        SoundEngine
        SteelseriesMouse
        Storm
        WoWCommon
        Battlenet
        Console
        ConsoleDevice
        DB
        Game
        Glue
        LiquidSystem
        Magic
        Minigame
        Net
        Object
        ObjectAlloc
        ObjectMgrClient
        SoundInterface
        UIUtil
        Ui
        WorldClient
        WowServices
        WowSvcs
        HoT
        Controller
        delayimp
        ${MARIADB_CLIENT_LIBRARY}
        ws2_32
        secur32
        crypt32
        shlwapi
        bcrypt
)
