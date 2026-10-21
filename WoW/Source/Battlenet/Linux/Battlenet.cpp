#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <dlfcn.h>
#include <new>
#include "WoW/Source/Battlenet/Battlenet.h"

// WindowsLibrary keeps its name (Battlenet.h and the portable code use it) but wraps a dlopen handle.

void* Battlenet::WindowsLibrary::GetProc(const char* name) {
    return dlsym(this->m_module, name);
}

// ELF shared objects have no ordinal exports, so lookups by ordinal fail like a missing export does on Windows.
void* Battlenet::WindowsLibrary::GetProc(uint32_t ordinal) {
    return nullptr;
}

// The callers ask for "<name>.dll" (a PE module, which dlopen rejects); this fails the same way a missing DLL does.
Battlenet::Library* Battlenet::Library::Load(const char* path, Allocator& allocator) {
    void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);

    if (!module) {
        return nullptr;
    }

    void* memory = allocator.Alloc(sizeof(WindowsLibrary));

    if (!memory) {
        dlclose(module);
        return nullptr;
    }

    WindowsLibrary* library = new (memory) WindowsLibrary();
    library->m_module = module;
    library->m_allocator = &allocator;
    return library;
}

Battlenet::WindowsLibrary::~WindowsLibrary() {
    if (this->m_module) {
        dlclose(this->m_module);
    }
}

#endif
