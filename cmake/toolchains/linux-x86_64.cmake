# Cross compiles the Linux x86_64 client with clang and lld against a Debian sysroot.
# Tools/Cross/cross.py sysroot linux builds the sysroot into .cross/linux-x86_64.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if (NOT WOWDECOMP_SYSROOT)
    get_filename_component(WOWDECOMP_SYSROOT "${CMAKE_CURRENT_LIST_DIR}/../../.cross/linux-x86_64" ABSOLUTE)
endif ()
set(WOWDECOMP_SYSROOT "${WOWDECOMP_SYSROOT}" CACHE PATH "Linux sysroot")

if (NOT EXISTS "${WOWDECOMP_SYSROOT}/usr/include/stdio.h")
    message(FATAL_ERROR "No Linux sysroot at ${WOWDECOMP_SYSROOT}. Run: python Tools/Cross/cross.py sysroot linux")
endif ()

set(CMAKE_SYSROOT "${WOWDECOMP_SYSROOT}")
set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)

# The sysroot only has the libraries the client links, not the whole dependency closure of SDL2 and friends.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -Wl,--allow-shlib-undefined")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

set(CMAKE_FIND_ROOT_PATH "${WOWDECOMP_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${WOWDECOMP_SYSROOT}/usr/lib/x86_64-linux-gnu/pkgconfig:${WOWDECOMP_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${WOWDECOMP_SYSROOT}")
