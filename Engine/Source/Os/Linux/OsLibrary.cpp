#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstring>
#include <dlfcn.h>
#include "Engine/Source/Os/Linux/OsPath.h"
#include "Engine/Source/Os/OsLibrary.h"

void* OsLoadLibrary(const char* fileName) {
    if (!fileName) {
        return dlopen(nullptr, RTLD_NOW);
    }

    // A bare name goes through the loader's search path, like LoadLibrary; a path is a game path.
    if (!strchr(fileName, '\\') && !strchr(fileName, '/')) {
        return dlopen(fileName, RTLD_NOW | RTLD_LOCAL);
    }

    char path[4096];

    if (!OsLinuxResolvePath(fileName, path, sizeof(path), false)) {
        return nullptr;
    }

    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void OsFreeLibrary(void* module) {
    if (module) {
        dlclose(module);
    }
}

void* OsGetLibraryEntryPoint(void* module, const char* name) {
    return dlsym(module, name);
}

#endif
