
#include <windows.h>
#include <new>
#include "WoW/Source/Battlenet/Battlenet.h"

// OFFSET: 0x009C28B0
void* Battlenet::WindowsLibrary::GetProc(const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(this->m_module), name));
}

// OFFSET: 0x009C28D0
void* Battlenet::WindowsLibrary::GetProc(uint32_t ordinal) {
    if (ordinal > 0xFFFF) {
        return nullptr;
    }

    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(this->m_module), MAKEINTRESOURCEA(ordinal)));
}

// OFFSET: 0x009C2900
Battlenet::Library* Battlenet::Library::Load(const char* path, Allocator& allocator) {
    int32_t length = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);

    if (length <= 0) {
        return nullptr;
    }

    wchar_t* widePath = static_cast<wchar_t*>(allocator.Alloc(2 * length));

    if (!widePath) {
        return nullptr;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, length) > 0) {
        HMODULE module = LoadLibraryW(widePath);

        if (module) {
            void* memory = allocator.Alloc(sizeof(WindowsLibrary));

            if (memory) {
                WindowsLibrary* library = new (memory) WindowsLibrary();
                library->m_module = module;
                library->m_allocator = &allocator;
                allocator.Free(widePath);
                return library;
            }
        }
    }

    allocator.Free(widePath);
    return nullptr;
}

// OFFSET: 0x009C2A30
Battlenet::WindowsLibrary::~WindowsLibrary() {
    FreeLibrary(static_cast<HMODULE>(this->m_module));
}

