
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <windows.h>
#include "Engine/Source/Storm/Source/IOUnit/IOFileUnit.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/SOption.h"

namespace Storm {

// OFFSET: 0x00779C30
static HANDLE OpenFileHandle(const IOFileParam* param) {
    HANDLE handle;

    if (g_opt.unicodeenabled) {
        WCHAR widename[MAX_PATH];
        SUniConvertUTF8to16(reinterpret_cast<uint16_t*>(widename), MAX_PATH, param->m_name, STORM_MAX_STR, nullptr, nullptr);
        handle = CreateFileW(widename, param->m_access, param->m_share, nullptr, param->m_creation, param->m_flags, nullptr);
    } else {
        handle = CreateFileA(param->m_name, param->m_access, param->m_share, nullptr, param->m_creation, param->m_flags, nullptr);
    }

    return handle != INVALID_HANDLE_VALUE ? handle : nullptr;
}

// OFFSET: 0x00779F50
static uint32_t OsPathGetRootChars(const char* path) {
    uint32_t length = SStrLen(path);

    if (length < 2) {
        return 0;
    }

    if (path[1] == ':') {
        return path[2] == '\\' ? 3 : 2;
    }

    if (path[0] != '\\' || path[1] != '\\') {
        return 0;
    }

    const char* current = path + 2;

    for (int32_t i = 0; i < 2; i++) {
        if (current) {
            current = SStrChr(current, '\\') + 1;
        }
    }

    return current ? static_cast<uint32_t>(current - path) : length;
}

// INLINED
IOUnit::IOUnit() {
    this->m_refcount = nullptr;
    this->m_count = 1;
}

// INLINED
void IOUnit::Release() {
    IORefcount* refcount = this->m_refcount;

    if (--this->m_count == 0) {
        delete this;
    }

    if (refcount) {
        refcount->Release();
    }
}

// INLINED
IOWin32FileUnit::IOWin32FileUnit(void* handle) {
    this->m_handle = handle;
    this->m_position = -1;
    this->Seek(0);
}

// OFFSET: 0x00779CD0
bool IOWin32FileUnit::Seek(int64_t offset) {
    if (this->m_position != offset) {
        LONG high = static_cast<LONG>(offset >> 32);

        if (SetFilePointer(this->m_handle, static_cast<LONG>(offset), &high, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
            this->m_position = -1;
            return false;
        }

        this->m_position = offset;
    }

    return true;
}

// OFFSET: 0x00779D30
bool IOWin32FileUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    uint32_t minimum = 1;
    uint32_t localRead;

    if (!bytesRead) {
        minimum = count;
        bytesRead = &localRead;
    }

    *bytesRead = 0;

    for (int32_t tries = 0; tries < 3; tries++) {
        if (!this->Seek(offset)) {
            return false;
        }

        BOOL result = ReadFile(this->m_handle, buffer, count, reinterpret_cast<LPDWORD>(bytesRead), nullptr);
        this->m_position = offset + *bytesRead;

        if (result) {
            return *bytesRead >= minimum;
        }

        this->m_position = -1;
        Sleep(10);
    }

    return false;
}

// OFFSET: 0x00779E20
bool IOWin32FileUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    DWORD written = 0;

    if (!this->Seek(offset)) {
        return false;
    }

    if (WriteFile(this->m_handle, buffer, count, &written, nullptr) && written == count) {
        this->m_position = offset + written;
        return true;
    }

    this->m_position = -1;

    return false;
}

// OFFSET: 0x00779E90
bool IOWin32FileUnit::Flush() {
    return FlushFileBuffers(this->m_handle) != 0;
}

// OFFSET: 0x00779EB0
bool IOWin32FileUnit::GetFileSize(int64_t* size) {
    DWORD high;
    DWORD low = ::GetFileSize(this->m_handle, &high);

    if (low == INVALID_FILE_SIZE && GetLastError()) {
        *size = 0;
        return false;
    }

    *size = (static_cast<int64_t>(high) << 32) | low;

    return true;
}

// OFFSET: 0x00779F10
bool IOWin32FileUnit::SetFileSize(int64_t size) {
    if (!this->Seek(size)) {
        return false;
    }

    return SetEndOfFile(this->m_handle) != 0;
}

// OFFSET: 0x008C32F0
void* IOWin32FileUnit::Win32Handle() {
    return this->m_handle;
}

// OFFSET: 0x00779FB0
bool IOUnbufferedFileUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    if (!(this->m_alignMask & reinterpret_cast<uintptr_t>(buffer)) && !(this->m_alignMask & count)) {
        return this->m_unit->Read(buffer, offset, count, bytesRead);
    }

    if (!this->m_buffer) {
        return false;
    }

    uint32_t total = 0;
    bool result = true;

    while (count) {
        uint32_t chunk = count >= 0x100000 ? 0x100000 : count;
        uint32_t size = chunk;

        if (chunk & this->m_alignMask) {
            size = (this->m_alignMask + chunk) & ~this->m_alignMask;
        }

        uint32_t read;
        result = this->m_unit->Read(this->m_buffer, offset, size, &read);

        if (!result) {
            break;
        }

        read = size < chunk ? size : chunk;
        memcpy(buffer, this->m_buffer, read);
        buffer = static_cast<uint8_t*>(buffer) + read;
        offset += read;
        count -= read;
        total += read;

        if (read < chunk) {
            break;
        }
    }

    if (!bytesRead) {
        return count == 0;
    }

    *bytesRead = total;

    return result;
}

// OFFSET: 0x0077A0C0
bool IOUnbufferedFileUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    if (!(this->m_alignMask & reinterpret_cast<uintptr_t>(buffer)) && !(this->m_alignMask & count)) {
        return this->m_unit->Write(buffer, offset, count);
    }

    if (!this->m_buffer) {
        return false;
    }

    bool result = true;

    while (count) {
        uint32_t chunk = count >= 0x100000 ? 0x100000 : count;
        memcpy(this->m_buffer, buffer, chunk);
        buffer = static_cast<const uint8_t*>(buffer) + chunk;

        uint32_t size = chunk;

        if (chunk & this->m_alignMask) {
            size = (this->m_alignMask + chunk) & ~this->m_alignMask;
        }

        result = this->m_unit->Write(this->m_buffer, offset, size);

        if (!result) {
            break;
        }

        offset += chunk;
        count -= chunk;
    }

    if (offset > this->m_size) {
        this->m_size = offset;
    }

    return result;
}

// OFFSET: 0x0077A190
bool IOUnbufferedFileUnit::Flush() {
    return this->m_unit->Flush();
}

// OFFSET: 0x0077A1A0
bool IOUnbufferedFileUnit::SetFileSize(int64_t size) {
    this->m_size = size;

    return this->m_unit->SetFileSize((size + this->m_alignMask) & ~this->m_alignMask);
}

// OFFSET: 0x0077A1E0
bool IOUnbufferedFileUnit::GetFileSize(int64_t* size) {
    *size = this->m_size;

    return true;
}

// OFFSET: 0x0077A200
IOUnit::~IOUnit() {
}

// OFFSET: 0x0077A230
IOWin32FileUnit::~IOWin32FileUnit() {
    CloseHandle(this->m_handle);
    this->m_handle = nullptr;
}

// OFFSET: 0x0077A270
IOUnbufferedFileUnit::IOUnbufferedFileUnit(void* handle, const IOFileParam* param) {
    this->m_unit = new IOWin32FileUnit(handle);
    this->m_size = 0;
    this->m_unit->GetFileSize(&this->m_size);
    SStrCopy(this->m_path, param->m_name, sizeof(this->m_path));
    this->m_buffer = VirtualAlloc(nullptr, 0x100000, MEM_COMMIT, PAGE_READWRITE);

    char rootPath[MAX_PATH];
    SStrCopy(rootPath, this->m_path, OsPathGetRootChars(this->m_path) + 1);

    DWORD sectorsPerCluster;
    DWORD bytesPerSector;
    DWORD freeClusters;
    DWORD totalClusters;

    if (!GetDiskFreeSpaceA(rootPath, &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters) || ((bytesPerSector - 1) & bytesPerSector)) {
        this->m_alignMask = 0xFFFF;
    } else {
        this->m_alignMask = bytesPerSector - 1;
    }
}

// OFFSET: 0x0077A380
void* IOUnbufferedFileUnit::Win32Handle() {
    return this->m_unit->Win32Handle();
}

// OFFSET: 0x0077A390
IOUnbufferedFileUnit* IOUnbufferedFileUnit::Open(const IOFileParam* param) {
    HANDLE handle = OpenFileHandle(param);

    if (!handle) {
        return nullptr;
    }

    return new IOUnbufferedFileUnit(handle, param);
}

// OFFSET: 0x0077A3E0
IOWin32FileUnit* IOWin32FileUnit::Open(const IOFileParam* param) {
    HANDLE handle = OpenFileHandle(param);

    if (!handle) {
        return nullptr;
    }

    return new IOWin32FileUnit(handle);
}

// OFFSET: 0x0077A450
IOUnit* IOFileUnit::Create(const IOFileParam& param, IOUnit* next) {
    if (next) {
        return nullptr;
    }

    if (param.m_flags & FILE_FLAG_NO_BUFFERING) {
        return IOUnbufferedFileUnit::Open(&param);
    }

    return IOWin32FileUnit::Open(&param);
}

// OFFSET: [0x0077A480, 0x0077A530]
IOUnbufferedFileUnit::~IOUnbufferedFileUnit() {
    VirtualFree(this->m_buffer, 0, MEM_RELEASE);

    if (this->m_unit) {
        delete this->m_unit;
    }

    IOFileParam param;
    param.m_name = this->m_path;
    param.m_access = GENERIC_WRITE;
    param.m_share = FILE_SHARE_READ;
    param.m_creation = OPEN_EXISTING;
    param.m_flags = FILE_ATTRIBUTE_NORMAL;

    IOUnit* unit = IOWin32FileUnit::Open(&param);

    if (unit) {
        unit->SetFileSize(this->m_size);
        unit->Release();
    }
}

}

