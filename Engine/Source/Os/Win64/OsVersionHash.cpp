
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/OsVersionHash.h"
#include "Engine/Source/Storm/H/storm.h"

// OFFSET: 0x00871520
OsVersionHash::~OsVersionHash() {
    if (this->m_entries) {
        SMemFree(this->m_entries, __FILE__, __LINE__, 0);
    }
}

// OFFSET: 0x00871540
bool OsVersionHash::AddEntry(const OsVersionHashEntry& entry) {
    if (!this->m_entries) {
        return false;
    }

    if (this->m_count >= this->m_capacity) {
        this->m_capacity *= 2;
        this->m_entries = static_cast<OsVersionHashEntry*>(SMemReAlloc(this->m_entries, this->m_capacity * sizeof(OsVersionHashEntry), __FILE__, __LINE__, 0));

        if (!this->m_entries) {
            return false;
        }
    }

    this->m_entries[this->m_count] = entry;
    this->m_count++;

    return true;
}

// OFFSET: 0x008715B0
static int __cdecl CompareEntries(const void* a, const void* b) {
    const OsVersionHashEntry* entryA = static_cast<const OsVersionHashEntry*>(a);
    const OsVersionHashEntry* entryB = static_cast<const OsVersionHashEntry*>(b);

    if (entryA->m_offset < entryB->m_offset) {
        return -1;
    }

    return entryA->m_offset > entryB->m_offset;
}

// OFFSET: 0x008715D0
static void HashZeroes(IOsVersionHashCallback& callback, uint32_t size) {
    uint8_t zeroes[4096];
    memset(zeroes, 0, sizeof(zeroes));

    while (size) {
        uint32_t chunk = size > sizeof(zeroes) ? sizeof(zeroes) : size;
        callback.Process(zeroes, chunk);
        size -= chunk;
    }
}

// OFFSET: 0x00871630
static int32_t AddRelocationEntries(OsVersionHash& hash, const uint8_t* image, uint64_t imageBase, const IMAGE_DATA_DIRECTORY* directory, int32_t unrelocate, bool is32Bit) {
    if (!directory->VirtualAddress || !directory->Size) {
        return 1;
    }

    if (reinterpret_cast<intptr_t>(image) == imageBase) {
        return 1;
    }

    const uint8_t* block = image + directory->VirtualAddress;
    const uint8_t* end = block + directory->Size;

    while (block < end) {
        const IMAGE_BASE_RELOCATION* relocation = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(block);
        if (!relocation->SizeOfBlock) {
            break;
        }

        uint32_t count = (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        const uint16_t* fixup = reinterpret_cast<const uint16_t*>(relocation + 1);

        for (; count; count--, fixup++) {
            uint32_t type = *fixup >> 12;
            uint32_t size;

            switch (type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    continue;

                case IMAGE_REL_BASED_HIGH:
                case IMAGE_REL_BASED_LOW:
                    size = 2;
                    break;

                case IMAGE_REL_BASED_HIGHLOW:
                    size = 4;
                    break;

                case IMAGE_REL_BASED_DIR64:
                    size = 8;
                    break;

                default:
                    return 0;
            }

            OsVersionHashEntry entry;
            entry.m_offset = relocation->VirtualAddress + (*fixup & 0xFFF);
            entry.m_size = size;
            entry.m_type = 2;
            entry.m_param = type;

            if (!hash.AddEntry(entry)) {
                return 0;
            }
        }

        block = reinterpret_cast<const uint8_t*>(fixup);
    }

    return 1;
}

// OFFSET: 0x00871740
static int32_t AddHeaderEntries(OsVersionHash& hash, const uint8_t* image, uint64_t imageBase, uint32_t sizeOfHeaders, int32_t unrelocate, bool is32Bit) {
    if (sizeOfHeaders < sizeof(IMAGE_DOS_HEADER)) {
        return 0;
    }

    OsVersionHashEntry entry;
    entry.m_offset = offsetof(IMAGE_DOS_HEADER, e_csum);
    entry.m_size = sizeof(WORD);
    entry.m_type = 3;
    entry.m_param = 0;
    hash.AddEntry(entry);

    return 1;
}

// OFFSET: 0x00871790
static int32_t HashEntry(IOsVersionHashCallback& callback, const OsVersionHashEntry& entry, const uint8_t* image, uint64_t imageBase, const uint8_t* data, int32_t unrelocate, bool is32Bit) {
    uint64_t delta = reinterpret_cast<intptr_t>(image) - imageBase;

    switch (entry.m_type) {
        case 0: {
            if (entry.m_param) {
                callback.Process(image + entry.m_param, entry.m_size);
            } else {
                HashZeroes(callback, entry.m_size);
            }

            return 1;
        }

        case 1: {
            IMAGE_IMPORT_DESCRIPTOR descriptor = *reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(data);
            descriptor.TimeDateStamp = 0;
            descriptor.ForwarderChain = 0;
            callback.Process(&descriptor, sizeof(descriptor));

            return 1;
        }

        case 2: {
            if (!delta || !unrelocate) {
                callback.Process(data, entry.m_size);
                return 1;
            }

            switch (entry.m_param) {
                case IMAGE_REL_BASED_HIGH: {
                    uint32_t value = ((*reinterpret_cast<const uint16_t*>(data) << 16) - static_cast<uint32_t>(delta)) >> 16;
                    callback.Process(&value, sizeof(uint16_t));
                    return 1;
                }

                case IMAGE_REL_BASED_LOW: {
                    uint32_t value = static_cast<uint16_t>(*reinterpret_cast<const uint16_t*>(data) - static_cast<uint16_t>(delta));
                    callback.Process(&value, sizeof(uint16_t));
                    return 1;
                }

                case IMAGE_REL_BASED_HIGHLOW: {
                    uint32_t value = *reinterpret_cast<const uint32_t*>(data) - static_cast<uint32_t>(delta);
                    callback.Process(&value, sizeof(uint32_t));
                    return 1;
                }

                case IMAGE_REL_BASED_DIR64: {
                    uint64_t value = *reinterpret_cast<const uint64_t*>(data) - delta;
                    callback.Process(&value, sizeof(uint64_t));
                    return 1;
                }

                default:
                    return 0;
            }
        }

        case 3: {
            HashZeroes(callback, entry.m_size);
            return 1;
        }

        default:
            return 0;
    }
}

// OFFSET: 0x00871980
static int32_t HashSection(IOsVersionHashCallback& callback, OsVersionHash& hash, const uint8_t* image, uint64_t imageBase, const IMAGE_SECTION_HEADER* section, uint32_t alignment, int32_t unrelocate, bool is32Bit) {
    uint32_t size = section->Misc.VirtualSize;
    uint32_t address = section->VirtualAddress;
    uint32_t padding = ((size + alignment - 1) & ~(alignment - 1)) - size;

    if (section->Characteristics & IMAGE_SCN_MEM_WRITE) {
        HashZeroes(callback, size + padding);
        return 1;
    }

    uint32_t index = 0;
    while (index < hash.m_count && hash.m_entries[index].m_offset < address) {
        index++;
    }

    if (size) {
        const uint8_t* start = image + address;
        const uint8_t* data = start;

        do {
            const uint8_t* next = index < hash.m_count ? start + (hash.m_entries[index].m_offset - address) : nullptr;
            uint32_t chunk = static_cast<uint32_t>(start - data) + size;

            if (next && static_cast<uint32_t>(next - data) < chunk) {
                chunk = static_cast<uint32_t>(next - data);
            }

            if (chunk) {
                callback.Process(data, chunk);
                data += chunk;
            } else {
                OsVersionHashEntry entry = hash.m_entries[index];

                if (!HashEntry(callback, entry, image, imageBase, data, unrelocate, is32Bit)) {
                    return 0;
                }

                index++;
                data += entry.m_size;
            }
        } while (static_cast<uint32_t>(data - start) < size);
    }

    if (!padding) {
        return 1;
    }

    uint8_t* zeroes = static_cast<uint8_t*>(SMemAlloc(padding, __FILE__, __LINE__, 0));
    if (!zeroes) {
        return 0;
    }

    memset(zeroes, 0, padding);

    const uint8_t* data = zeroes;

    do {
        const uint8_t* next = index < hash.m_count ? zeroes + (hash.m_entries[index].m_offset - size - address) : nullptr;
        uint32_t chunk = static_cast<uint32_t>(zeroes - data) + padding;

        if (next && static_cast<uint32_t>(next - data) < chunk) {
            chunk = static_cast<uint32_t>(next - data);
        }

        if (chunk) {
            callback.Process(data, chunk);
            data += chunk;
        } else {
            OsVersionHashEntry entry = hash.m_entries[index];

            if (!HashEntry(callback, entry, image, imageBase, data, unrelocate, is32Bit)) {
                return 0;
            }

            index++;
            data += entry.m_size;
        }
    } while (static_cast<uint32_t>(data - zeroes) < padding);

    SMemFree(zeroes, __FILE__, __LINE__, 0);

    return 1;
}

// OFFSET: 0x00871BE0
OsVersionHash::OsVersionHash() : m_entries(nullptr) {
    this->m_entries = static_cast<OsVersionHashEntry*>(SMemAlloc(256 * sizeof(OsVersionHashEntry), __FILE__, __LINE__, 0));
    this->m_count = 0;
    this->m_capacity = 256;
}

// OFFSET: 0x00871C20
static int32_t AddImportEntries(OsVersionHash& hash, const uint8_t* image, const IMAGE_IMPORT_DESCRIPTOR* imports, uint32_t importAddress, uint32_t importSize, int32_t unrelocate, bool is32Bit) {
    if (!imports) {
        return 1;
    }

    if (importSize < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        return 1;
    }

    const IMAGE_IMPORT_DESCRIPTOR* descriptor = imports;

    do {
        if (!descriptor->Name || !descriptor->FirstThunk) {
            break;
        }

        uint32_t thunkSize;

        if (is32Bit) {
            const uint32_t* first = reinterpret_cast<const uint32_t*>(image + descriptor->FirstThunk);
            const uint32_t* thunk = first;

            while (*thunk) {
                thunk++;
            }

            thunkSize = static_cast<uint32_t>(thunk - first) * sizeof(uint32_t) + sizeof(uint32_t);
        } else {
            const uint64_t* first = reinterpret_cast<const uint64_t*>(image + descriptor->FirstThunk);
            const uint64_t* thunk = first;

            while (*thunk) {
                thunk++;
            }

            thunkSize = static_cast<uint32_t>(thunk - first) * sizeof(uint64_t) + sizeof(uint64_t);
        }

        OsVersionHashEntry thunks;
        thunks.m_offset = descriptor->FirstThunk;
        thunks.m_size = thunkSize;
        thunks.m_type = 0;
        thunks.m_param = descriptor->OriginalFirstThunk;

        if (!hash.AddEntry(thunks)) {
            return 0;
        }

        OsVersionHashEntry entry;
        entry.m_offset = importAddress;
        entry.m_size = sizeof(IMAGE_IMPORT_DESCRIPTOR);
        entry.m_type = 1;
        entry.m_param = 0;

        if (!hash.AddEntry(entry)) {
            return 0;
        }

        importAddress += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        descriptor++;
    } while (reinterpret_cast<const uint8_t*>(descriptor + 1) - reinterpret_cast<const uint8_t*>(imports) <= static_cast<intptr_t>(importSize));

    return 1;
}

// OFFSET: 0x00871D20
static int32_t ComputeChecksum(IOsVersionHashCallback& callback, const uint8_t* image, int32_t unrelocate) {
    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (!dosHeader->e_lfanew) {
        return 0;
    }

    const IMAGE_NT_HEADERS32* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE || ntHeaders->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)) {
        return 0;
    }

    uint64_t imageBase;
    uint32_t sectionAlignment;
    uint32_t fileAlignment;
    uint32_t sizeOfHeaders;
    const IMAGE_DATA_DIRECTORY* importDirectory;
    const IMAGE_DATA_DIRECTORY* relocationDirectory;
    bool is32Bit;

    if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        const IMAGE_OPTIONAL_HEADER32& optionalHeader = ntHeaders->OptionalHeader;
        if (optionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT) {
            return 0;
        }

        fileAlignment = optionalHeader.FileAlignment;
        sectionAlignment = optionalHeader.SectionAlignment;
        imageBase = optionalHeader.ImageBase;
        sizeOfHeaders = optionalHeader.SizeOfHeaders;
        importDirectory = &optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        relocationDirectory = &optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        is32Bit = true;
    } else if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        const IMAGE_OPTIONAL_HEADER64& optionalHeader = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ntHeaders)->OptionalHeader;
        if (optionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT) {
            return 0;
        }

        fileAlignment = optionalHeader.FileAlignment;
        sectionAlignment = optionalHeader.SectionAlignment;
        imageBase = optionalHeader.ImageBase;
        sizeOfHeaders = optionalHeader.SizeOfHeaders;
        relocationDirectory = &optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        importDirectory = &optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        is32Bit = false;
    } else {
        return 0;
    }

    const IMAGE_IMPORT_DESCRIPTOR* imports = nullptr;

    if (importDirectory->VirtualAddress) {
        if (importDirectory->Size < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
            return 0;
        }

        imports = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image + importDirectory->VirtualAddress);
    }

    const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(ntHeaders);

    OsVersionHash hash;

    if (!AddHeaderEntries(hash, image, imageBase, sizeOfHeaders, 1, is32Bit)) {
        return 0;
    }

    if (!AddRelocationEntries(hash, image, imageBase, relocationDirectory, 1, is32Bit)) {
        return 0;
    }

    if (!AddImportEntries(hash, image, imports, importDirectory->VirtualAddress, importDirectory->Size, 1, is32Bit)) {
        return 0;
    }

    qsort(hash.m_entries, hash.m_count, sizeof(OsVersionHashEntry), CompareEntries);

    IMAGE_SECTION_HEADER headers = {};
    headers.Misc.VirtualSize = sizeOfHeaders;
    headers.SizeOfRawData = (sizeOfHeaders + fileAlignment - 1) & ~(fileAlignment - 1);
    headers.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    if (!HashSection(callback, hash, image, imageBase, &headers, sectionAlignment, 1, is32Bit)) {
        return 0;
    }

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (!HashSection(callback, hash, image, imageBase, &sections[i], sectionAlignment, 1, is32Bit)) {
            return 0;
        }
    }

    return 1;
}

// OFFSET: 0x00871F90
int32_t IOsVersionChecksum(IOsVersionHashCallback& callback, const char* path, int32_t loaded) {
    if (!path && !loaded) {
        return 0;
    }

    if (loaded) {
        uint16_t* widePath = nullptr;

        if (path) {
            uint32_t length = SUniConvertUTF8to16Len(path, 0x7FFFFFFF, nullptr) + 1;
            widePath = static_cast<uint16_t*>(SMemAlloc(length * sizeof(uint16_t), __FILE__, __LINE__, 0));
            if (!widePath) {
                return 0;
            }

            SUniConvertUTF8to16(widePath, length, path, 0x7FFFFFFF, nullptr, nullptr);
        }

        HMODULE module = GetModuleHandleW(reinterpret_cast<LPCWSTR>(widePath));

        if (widePath) {
            SMemFree(widePath, __FILE__, __LINE__, 0);
        }

        if (!module) {
            return 0;
        }

        __try {
            return ComputeChecksum(callback, reinterpret_cast<const uint8_t*>(module), 1);
        } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            return 0;
        }
    }

    int32_t result = 0;
    void* buffer = nullptr;

    HOSFILE file = OsCreateFile(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0x3F3F3F3F);

    if (file != reinterpret_cast<HOSFILE>(INVALID_HANDLE_VALUE)) {
        uint64_t size = OsGetFileSize(file);

        if (size == static_cast<uint32_t>(size)) {
            buffer = SMemAlloc(static_cast<uint32_t>(size), __FILE__, __LINE__, 0);

            uint32_t bytesRead;
            if (buffer && OsReadFile(file, buffer, static_cast<uint32_t>(size), &bytesRead) && bytesRead == size) {
                callback.Process(buffer, static_cast<uint32_t>(size));
                result = 1;
            }
        }
    }

    if (buffer) {
        SMemFree(buffer, __FILE__, __LINE__, 0);
    }

    if (file != reinterpret_cast<HOSFILE>(INVALID_HANDLE_VALUE)) {
        OsCloseFile(file);
    }

    return result;
}

// OFFSET: 0x00872170
OsFileVersionInfo::OsFileVersionInfo(const char* filename) : m_fileVersion(), m_productVersion() {
    char path[MAX_PATH];
    if (!filename) {
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        filename = path;
    }

    DWORD handle;
    DWORD size = GetFileVersionInfoSizeA(filename, &handle);
    if (size) {
        m_data.SetCount(size);

        if (GetFileVersionInfoA(filename, handle, m_data.Count(), m_data.Ptr())) {
            VS_FIXEDFILEINFO* info = nullptr;
            UINT length = 0;

            if (VerQueryValueA(m_data.Ptr(), "\\", reinterpret_cast<void**>(&info), &length)) {
                m_fileVersion[0] = HIWORD(info->dwFileVersionMS);
                m_fileVersion[1] = LOWORD(info->dwFileVersionMS);
                m_fileVersion[2] = HIWORD(info->dwFileVersionLS);
                m_fileVersion[3] = LOWORD(info->dwFileVersionLS);
                m_productVersion[0] = HIWORD(info->dwProductVersionMS);
                m_productVersion[1] = LOWORD(info->dwProductVersionMS);
                m_productVersion[2] = HIWORD(info->dwProductVersionLS);
                m_productVersion[3] = LOWORD(info->dwProductVersionLS);
                return;
            }
        }
    }

    m_data.Clear();
}

