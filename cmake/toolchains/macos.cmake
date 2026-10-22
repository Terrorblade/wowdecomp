# Cross compiles the macOS client with clang and ld64.lld against a macOS SDK.
# Tools/Cross/cross.py sysroot macos checks out the SDK (ThirdParty/macos-sdk) and fetches SDL2 and xvidcore
# into .cross/macos. WOWDECOMP_MACOS_SDK can point at another SDK, such as the one from xcrun --show-sdk-path on a Mac.
# WOWDECOMP_MACOS_ARCH picks arm64 or x86_64.

set(CMAKE_SYSTEM_NAME Darwin)

if (NOT WOWDECOMP_MACOS_ARCH)
    set(WOWDECOMP_MACOS_ARCH arm64)
endif ()
set(WOWDECOMP_MACOS_ARCH "${WOWDECOMP_MACOS_ARCH}" CACHE STRING "arm64 or x86_64")

if (NOT WOWDECOMP_CROSS_ROOT)
    get_filename_component(WOWDECOMP_CROSS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.cross/macos" ABSOLUTE)
endif ()
set(WOWDECOMP_CROSS_ROOT "${WOWDECOMP_CROSS_ROOT}" CACHE PATH "macOS SDK and dependency root")

if (NOT WOWDECOMP_MACOS_SDK)
    get_filename_component(WOWDECOMP_MACOS_SDK "${CMAKE_CURRENT_LIST_DIR}/../../ThirdParty/macos-sdk/MacOSX26.5.sdk" ABSOLUTE)
endif ()
set(WOWDECOMP_MACOS_SDK "${WOWDECOMP_MACOS_SDK}" CACHE PATH "macOS SDK")
set(WOWDECOMP_MACOS_DEPS "${WOWDECOMP_CROSS_ROOT}/deps-${WOWDECOMP_MACOS_ARCH}")

if (NOT EXISTS "${WOWDECOMP_MACOS_SDK}/usr/include/stdio.h")
    message(FATAL_ERROR "No macOS SDK at ${WOWDECOMP_MACOS_SDK}. Run: python Tools/Cross/cross.py sysroot macos")
endif ()

set(CMAKE_SYSTEM_PROCESSOR ${WOWDECOMP_MACOS_ARCH})
set(CMAKE_OSX_ARCHITECTURES ${WOWDECOMP_MACOS_ARCH} CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "")
set(CMAKE_OSX_SYSROOT "${WOWDECOMP_MACOS_SDK}" CACHE PATH "")

# Full paths: CMake looks the Objective-C compiler up without the host's .exe suffix.
find_program(WOWDECOMP_CLANG clang REQUIRED)
find_program(WOWDECOMP_CLANGXX clang++ REQUIRED)
set(CMAKE_C_COMPILER ${WOWDECOMP_CLANG})
set(CMAKE_CXX_COMPILER ${WOWDECOMP_CLANGXX})
set(CMAKE_OBJC_COMPILER ${WOWDECOMP_CLANG})
set(CMAKE_OBJCXX_COMPILER ${WOWDECOMP_CLANGXX})
set(CMAKE_C_COMPILER_TARGET ${WOWDECOMP_MACOS_ARCH}-apple-macos11)
set(CMAKE_CXX_COMPILER_TARGET ${WOWDECOMP_MACOS_ARCH}-apple-macos11)
set(CMAKE_OBJC_COMPILER_TARGET ${WOWDECOMP_MACOS_ARCH}-apple-macos11)
set(CMAKE_OBJCXX_COMPILER_TARGET ${WOWDECOMP_MACOS_ARCH}-apple-macos11)
# CMake's Objective-C compiler identification ignores *_COMPILER_TARGET and would build for the host.
set(CMAKE_OBJC_FLAGS_INIT "--target=${WOWDECOMP_MACOS_ARCH}-apple-macos11")
set(CMAKE_OBJCXX_FLAGS_INIT "--target=${WOWDECOMP_MACOS_ARCH}-apple-macos11")
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)
set(CMAKE_INSTALL_NAME_TOOL llvm-install-name-tool)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

set(CMAKE_FIND_ROOT_PATH "${WOWDECOMP_MACOS_DEPS};${WOWDECOMP_CROSS_ROOT};${WOWDECOMP_MACOS_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_FRAMEWORK LAST)

set(WOWDECOMP_MACOS_FRAMEWORKS "${WOWDECOMP_CROSS_ROOT}/Frameworks" CACHE PATH "SDL2.framework location")
list(APPEND CMAKE_FRAMEWORK_PATH "${WOWDECOMP_MACOS_FRAMEWORKS}")
