file(GLOB PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")
wowdecomp_filter_platform_sources(PRIVATE_SOURCES)

if (WOWDECOMP_PLATFORM_LINUX)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS "Linux/*.cpp")
elseif (WOWDECOMP_ARCH_64)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS "Win64/*.cpp")
else ()
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS "Win32/*.cpp")
endif ()

add_executable(Wow WIN32 ${PRIVATE_SOURCES} ${PLATFORM_SOURCES})

if (WOWDECOMP_ARCH_64)
    set(WOW_OUTPUT_NAME Wow64)
else ()
    set(WOW_OUTPUT_NAME Wow32)
endif ()

if (WOWDECOMP_PLATFORM_LINUX)
    set(WOW_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/Linux)
else ()
    set(WOW_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/Win)
endif ()

set_target_properties(Wow PROPERTIES
    OUTPUT_NAME ${WOW_OUTPUT_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${WOW_OUTPUT_DIR}
)

foreach (config ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
    string(TOUPPER ${config} config)
    set_target_properties(Wow PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} ${WOW_OUTPUT_DIR})
endforeach ()

target_include_directories(Wow
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

if (WOWDECOMP_PLATFORM_WIN)
    target_link_options(Wow PRIVATE "/INCREMENTAL:NO" "/ENTRY:start")
endif ()

target_link_libraries(Wow
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
)
