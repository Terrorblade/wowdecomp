cmake_minimum_required(VERSION 3.10)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR
        "In-source builds not allowed.
        Please make a new directory (called a build directory) and run CMake from there.
        You may need to remove CMakeCache.txt."
    )
endif ()

project(wowdecomp CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(WOWDECOMP_PLATFORM_WIN 1)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    # Native Linux port: the Win32/Win64 platform sources are swapped for Linux/ ones (POSIX + SDL2), and the
    # client renders through the OpenGL device.
    set(WOWDECOMP_PLATFORM_LINUX 1)
    set(WOWDECOMP_PLATFORM_POSIX 1)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # Native macOS port: builds the Linux/ sources too, and a Mac/ file (.cpp or Objective-C++ .mm) replaces the Linux/
    # file of the same name. The window, input and audio are Cocoa and CoreAudio, and the client renders through the
    # native Metal device (Gx/CGxDeviceMetal), which runs the game's Direct3D 9 shaders translated to MSL.
    set(WOWDECOMP_PLATFORM_MAC 1)
    set(WOWDECOMP_PLATFORM_POSIX 1)
else ()
    message(FATAL_ERROR "wowdecomp targets Windows, Linux and macOS only.")
endif ()

# Each module globs its sources; this drops the ones that belong to another platform. On macOS a Mac/Name.cpp or
# Mac/Name.mm replaces Linux/Name.cpp, and the Linux SDL and OpenGL code is left out.
function(wowdecomp_filter_platform_sources var)
    if (WOWDECOMP_PLATFORM_LINUX)
        list(FILTER ${var} EXCLUDE REGEX "/[Ww]in(32|64)/|/Mac/|/CGxDeviceMetal/")
    elseif (WOWDECOMP_PLATFORM_MAC)
        list(FILTER ${var} EXCLUDE REGEX "/[Ww]in(32|64)/|/CGxDeviceOpenGL/|/Os/Linux/OsSdl\\.cpp$|/fmodex/linux/src/fmod_output_sdl\\.cpp$")
        set(sources)

        foreach (source ${${var}})
            if (source MATCHES "^(.*)/Linux/([^/.]*)\\.[^./]+$")
                if (EXISTS "${CMAKE_MATCH_1}/Mac/${CMAKE_MATCH_2}.cpp" OR EXISTS "${CMAKE_MATCH_1}/Mac/${CMAKE_MATCH_2}.mm")
                    continue ()
                endif ()
            endif ()

            list(APPEND sources ${source})
        endforeach ()

        set(${var} ${sources})
    else ()
        list(FILTER ${var} EXCLUDE REGEX "/Linux/|/Mac/|/CGxDeviceMetal/")
    endif ()
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

# x64 is the target. Win32 stays buildable: it is the configuration whose struct layouts can be
# checked against Wow.exe 3.3.5a (12340).
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WOWDECOMP_ARCH_64 1)
    add_compile_definitions(WOWDECOMP_ARCH_64=1)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(WOWDECOMP_ARCH_32 1)
    add_compile_definitions(WOWDECOMP_ARCH_32=1)
    # x87 floating point like the VS2005 original; keeps Win32 code comparable with the binary (re/tools/verify.py).
    if (MSVC)
        add_compile_options(/arch:IA32 /fp:fast)
    endif ()
    # 32-bit time_t like the original build: time/localtime/mktime resolve to _time32/_localtime32/_mktime32.
    add_compile_definitions(_USE_32BIT_TIME_T)
else ()
    message(FATAL_ERROR "Unsupported pointer size.")
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WOWDECOMP_PLATFORM_WIN)
    add_compile_definitions(
        WIN32
        _WINDOWS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
else ()
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "The Linux and macOS builds need clang (-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++).")
    endif ()
    add_compile_definitions(WOWDECOMP_PLATFORM_POSIX=1)
    if (WOWDECOMP_PLATFORM_LINUX)
        add_compile_definitions(WOWDECOMP_PLATFORM_LINUX=1)
        find_package(SDL2 REQUIRED CONFIG)
    else ()
        add_compile_definitions(WOWDECOMP_PLATFORM_MAC=1)
        enable_language(OBJC OBJCXX)
        # The Cocoa and Metal code in the .mm files is written for ARC.
        add_compile_options($<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>)
    endif ()
    find_package(Threads REQUIRED)
    # The modules are static libraries that reference each other in cycles, as MSVC's linker allows; lld resolves
    # archive members regardless of order.
    add_link_options(-fuse-ld=lld)
    link_libraries(Threads::Threads ${CMAKE_DL_LIBS})
endif ()

# Off by default: the client checks MPQ file MD5s (signaturefile) and GlueXML/FrameXML/AddOn .toc.sig files like the
# original. ON accepts unsigned or modified game data and interface files.
option(WOWDECOMP_DISABLE_SIGNATURE_CHECKS "Skip MPQ file signature and .toc signature checks" OFF)
if (WOWDECOMP_DISABLE_SIGNATURE_CHECKS)
    add_compile_definitions(DISABLE_SIGNATURE_CHECKS=1)
endif ()

# Third-party code is linked, not decompiled, for now.
if (EXISTS ${CMAKE_SOURCE_DIR}/ThirdParty/CMakeLists.txt)
    add_subdirectory(ThirdParty)
endif ()

if (WOWDECOMP_PLATFORM_MAC)
    set(WOWDECOMP_COMPAT_OS_HEADER ${CMAKE_SOURCE_DIR}/Engine/Source/Os/Mac/CompatOs.h)
else ()
    set(WOWDECOMP_COMPAT_OS_HEADER ${CMAKE_SOURCE_DIR}/Engine/Source/Os/Linux/CompatOs.h)
endif ()

if (NOT WOWDECOMP_PLATFORM_WIN)
    # The decompiled code is written against MSVC: keep its extensions (__int64, __declspec, ...) and the
    # semantics MSVC gives it (no strict aliasing, wrapping signed arithmetic). Compat.h fills in the rest of the
    # Windows names the portable code uses. ThirdParty is added before this so its libraries build as upstream wrote them.
    add_compile_options(
        -fms-extensions
        -fno-strict-aliasing
        -fwrapv
        -fno-delete-null-pointer-checks
        "SHELL:-include ${CMAKE_SOURCE_DIR}/Engine/Source/Os/Linux/Compat.h"
        "SHELL:-include ${WOWDECOMP_COMPAT_OS_HEADER}"
        $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-c++11-narrowing>
        $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-register>
        -Wno-microsoft
        -Wno-ignored-attributes
    )
endif ()

# Headers the code includes by their Linux names (malloc.h, GL/gl.h) that macOS keeps elsewhere.
if (WOWDECOMP_PLATFORM_MAC)
    include_directories(BEFORE ${CMAKE_SOURCE_DIR}/Engine/Source/Os/Mac/include)
endif ()

# arm64 (Apple Silicon): the SSE code builds through sse2neon, found under the usual x86 header names.
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(WOWDECOMP_CPU_ARM64 1)
    include_directories(BEFORE ${CMAKE_SOURCE_DIR}/ThirdParty/sse2neon-shim ${CMAKE_SOURCE_DIR}/ThirdParty/sse2neon)
endif ()

add_subdirectory(Engine/Source/BLPFile)
add_subdirectory(Engine/Source/Base)
add_subdirectory(Engine/Source/BlizzardCore)
add_subdirectory(Engine/Source/ComSat)
add_subdirectory(Engine/Source/ConsoleBase)
add_subdirectory(Engine/Source/Event)
add_subdirectory(Engine/Source/FFX)
add_subdirectory(Engine/Source/Frame)
add_subdirectory(Engine/Source/FrameScript)
add_subdirectory(Engine/Source/FrameXML)
add_subdirectory(Engine/Source/Gx)
add_subdirectory(Engine/Source/Gxu)
add_subdirectory(Engine/Source/Images)
add_subdirectory(Engine/Source/jpeg)
add_subdirectory(Engine/Source/lua-5.1)
add_subdirectory(Engine/Source/Model2)
add_subdirectory(Engine/Source/Os)
add_subdirectory(Engine/Source/pcre)
add_subdirectory(Engine/Source/Scrn)
add_subdirectory(Engine/Source/Services)
add_subdirectory(Engine/Source/ShaderEffect)
add_subdirectory(Engine/Source/SoundEngine)
add_subdirectory(Engine/Source/SteelseriesMouse)
add_subdirectory(Engine/Source/Storm)
add_subdirectory(Engine/Source/Tempest)
add_subdirectory(WoW/Common)
add_subdirectory(WoW/Source/Battlenet)
add_subdirectory(WoW/Source/Console)
add_subdirectory(WoW/Source/ConsoleDevice)
add_subdirectory(WoW/Source/DB)
add_subdirectory(WoW/Source/Game)
add_subdirectory(WoW/Source/Glue)
add_subdirectory(WoW/Source/Magic)
add_subdirectory(WoW/Source/LiquidSystem)
add_subdirectory(WoW/Source/Minigame)
add_subdirectory(WoW/Source/Net)
add_subdirectory(WoW/Source/Object)
add_subdirectory(WoW/Source/ObjectAlloc)
add_subdirectory(WoW/Source/ObjectMgrClient)
add_subdirectory(WoW/Source/SoundInterface)
add_subdirectory(WoW/Source/UIUtil)
add_subdirectory(WoW/Source/Ui)
add_subdirectory(WoW/Source/WorldClient)
add_subdirectory(WoW/Source/WowServices)
add_subdirectory(WoW/Source/WowSvcs)
add_subdirectory(WoW/Source)
add_subdirectory(HoT)
add_subdirectory(Controller)
if (WIN32)
    add_subdirectory(Tools/WowClient)
endif ()
if (WOWDECOMP_ARCH_64)
    add_subdirectory(Tools/XvidDecoder)
endif ()

foreach (target Wow WowClient MapViewer M2Viewer GxTest MpqTest BlpTest DbcTest AssetCheck)
    if (TARGET ${target})
        if (WOWDECOMP_PLATFORM_WIN)
            target_sources(${target} PRIVATE ${CMAKE_SOURCE_DIR}/waystone.rc)
        endif ()
    endif ()
endforeach ()
