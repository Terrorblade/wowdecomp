#include "HoT/Editor/Resources.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <cstdlib>
#include <filesystem>
#include <strings.h>
#endif
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"

namespace HoT::Editor::Resources {

extern const EmbeddedFile g_embeddedFiles[];
extern const size_t g_embeddedFileCount;
extern const EmbeddedFile g_profilerFiles[];
extern const size_t g_profilerFileCount;

struct EmbeddedTable {
    const char* prefix;
    const EmbeddedFile* files;
    const size_t* count;
};

static const EmbeddedTable s_tables[] = {
    { "Interface\\GameEditor\\", g_embeddedFiles, &g_embeddedFileCount },
    { "Interface\\GraphicsProfiler\\", g_profilerFiles, &g_profilerFileCount },
};

// Segoe UI ships with Windows, so the editor reads it from there instead of embedding it.
struct SystemFont {
    const char* name;
    const char* file;
};

static const SystemFont s_systemFonts[] = {
    { "Interface\\GameEditor\\Fonts\\segoeui.ttf", "segoeui.ttf" },
    { "Interface\\GameEditor\\Fonts\\segoeuisb.ttf", "seguisb.ttf" },
};

static bool SamePath(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        char ca = *a == '/' ? '\\' : *a;
        char cb = *b == '/' ? '\\' : *b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }

        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }

        if (ca != cb) {
            return false;
        }
    }

    return *a == *b;
}

static bool StartsWith(const char* name, const char* prefix) {
    for (size_t i = 0; prefix[i]; i++) {
        char c = name[i] == '/' ? '\\' : name[i];
        char p = prefix[i];

        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }

        if (p >= 'A' && p <= 'Z') {
            p = static_cast<char>(p - 'A' + 'a');
        }

        if (c != p) {
            return false;
        }
    }

    return true;
}

static const EmbeddedTable* TableFor(const char* name) {
    if (!name) {
        return nullptr;
    }

    for (const EmbeddedTable& table : s_tables) {
        if (StartsWith(name, table.prefix)) {
            return &table;
        }
    }

    return nullptr;
}

static bool HasPrefix(const char* name) {
    return TableFor(name) != nullptr;
}

static const EmbeddedFile* FindEmbedded(const char* name) {
    const EmbeddedTable* table = TableFor(name);

    if (!table) {
        return nullptr;
    }

    for (size_t i = 0; i < *table->count; i++) {
        if (SamePath(table->files[i].path, name)) {
            return &table->files[i];
        }
    }

    return nullptr;
}

static const SystemFont* FindSystemFont(const char* name) {
    for (const SystemFont& font : s_systemFonts) {
        if (SamePath(font.name, name)) {
            return &font;
        }
    }

    return nullptr;
}

#if !defined(_WIN32)
// No Windows font directory here: look for the Segoe file in the usual font directories (it may have been copied
// from a Windows install), else use a common sans font of similar weight.
static std::string FindFontFile(const char* file) {
    std::vector<std::filesystem::path> roots;
    const char* dataHome = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");

    if (dataHome && *dataHome) {
        roots.emplace_back(std::filesystem::path(dataHome) / "fonts");
    }

    if (home && *home) {
        roots.emplace_back(std::filesystem::path(home) / ".local/share/fonts");
        roots.emplace_back(std::filesystem::path(home) / ".fonts");
    }

    roots.emplace_back("/usr/local/share/fonts");
    roots.emplace_back("/usr/share/fonts");

    for (const std::filesystem::path& root : roots) {
        std::error_code error;
        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error);

        for (; !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
            if (strcasecmp(it->path().filename().c_str(), file) == 0 && it->is_regular_file(error)) {
                return it->path().string();
            }
        }
    }

    return std::string();
}

static std::string FindLinuxFont(const char* file, size_t slot) {
    static const char* const s_fallbacks[][3] = {
        { "DejaVuSans.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf" },
        { "DejaVuSans-Bold.ttf", "LiberationSans-Bold.ttf", "NotoSans-SemiBold.ttf" },
    };

    std::string path = FindFontFile(file);

    for (size_t i = 0; path.empty() && slot < sizeof(s_fallbacks) / sizeof(s_fallbacks[0]) && i < 3; i++) {
        path = FindFontFile(s_fallbacks[slot][i]);
    }

    return path;
}
#endif

static const std::vector<uint8_t>* LoadSystemFont(const SystemFont& font) {
    static std::vector<uint8_t> s_cache[sizeof(s_systemFonts) / sizeof(s_systemFonts[0])];
    static bool s_tried[sizeof(s_systemFonts) / sizeof(s_systemFonts[0])] = {};

    size_t slot = &font - s_systemFonts;

    if (s_tried[slot]) {
        return s_cache[slot].empty() ? nullptr : &s_cache[slot];
    }

    s_tried[slot] = true;

#if defined(_WIN32)
    char directory[MAX_PATH];
    UINT length = GetWindowsDirectoryA(directory, sizeof(directory));

    if (!length || length >= sizeof(directory)) {
        return nullptr;
    }

    std::string path = std::string(directory) + "\\Fonts\\" + font.file;
#else
    std::string path = FindLinuxFont(font.file, slot);

    if (path.empty()) {
        return nullptr;
    }
#endif
    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size > 0) {
        s_cache[slot].resize(static_cast<size_t>(size));

        if (fread(s_cache[slot].data(), 1, s_cache[slot].size(), file) != s_cache[slot].size()) {
            s_cache[slot].clear();
        }
    }

    fclose(file);

    return s_cache[slot].empty() ? nullptr : &s_cache[slot];
}

const char* const kPreviewPrefix = "HoTEditor\\Preview\\";

struct PreviewFile {
    std::string name;
    std::shared_ptr<const std::vector<uint8_t>> data;
    std::string source;
};

struct PreviewRedirect {
    std::string prefix;
    std::string target;
};

static std::mutex s_previewLock;
static std::vector<PreviewFile> s_previewFiles;
static std::vector<PreviewRedirect> s_previewRedirects;
static thread_local int32_t s_redirectDepth = 0;

static bool IsDigits(const char* text, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }

    return true;
}

// Only the skin and anim names Model2 builds from the model name are redirected.
static bool IsSideFileSuffix(const char* rest) {
    size_t length = strlen(rest);

    if (length == 7 && IsDigits(rest, 2) && SamePath(rest + 2, ".skin")) {
        return true;
    }

    return length == 12 && IsDigits(rest, 4) && rest[4] == '-' && IsDigits(rest + 5, 2) && SamePath(rest + 7, ".anim");
}

// Finds the preview bytes or the redirect target for a name, false when the name is not a preview.
static bool FindPreview(const char* name, std::shared_ptr<const std::vector<uint8_t>>& data, std::string& target) {
    if (!name || !StartsWith(name, kPreviewPrefix)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(s_previewLock);

    for (const PreviewFile& file : s_previewFiles) {
        if (SamePath(file.name.c_str(), name)) {
            data = file.data;
            return true;
        }
    }

    size_t length = strlen(name);

    for (const PreviewRedirect& redirect : s_previewRedirects) {
        if (length > redirect.prefix.size() && StartsWith(name, redirect.prefix.c_str()) && IsSideFileSuffix(name + redirect.prefix.size())) {
            target = redirect.target + (name + redirect.prefix.size());
            return true;
        }
    }

    return false;
}

void AddPreviewFile(const std::string& name, std::shared_ptr<const std::vector<uint8_t>> data, const std::string& source) {
    std::lock_guard<std::mutex> lock(s_previewLock);
    s_previewFiles.push_back({ name, std::move(data), source });
}

std::string PreviewSource(const char* name) {
    if (!name || !StartsWith(name, kPreviewPrefix)) {
        return std::string();
    }

    std::lock_guard<std::mutex> lock(s_previewLock);

    for (const PreviewFile& file : s_previewFiles) {
        if (SamePath(file.name.c_str(), name)) {
            return file.source;
        }
    }

    return std::string();
}

void AddPreviewRedirect(const std::string& prefix, const std::string& target) {
    std::lock_guard<std::mutex> lock(s_previewLock);
    s_previewRedirects.push_back({ prefix, target });
}

void RemovePreviews(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(s_previewLock);

    for (size_t i = s_previewFiles.size(); i-- > 0;) {
        if (StartsWith(s_previewFiles[i].name.c_str(), prefix.c_str())) {
            s_previewFiles.erase(s_previewFiles.begin() + i);
        }
    }

    for (size_t i = s_previewRedirects.size(); i-- > 0;) {
        if (StartsWith(s_previewRedirects[i].prefix.c_str(), prefix.c_str())) {
            s_previewRedirects.erase(s_previewRedirects.begin() + i);
        }
    }
}

static bool PreviewExists(const char* name) {
    std::shared_ptr<const std::vector<uint8_t>> data;
    std::string target;

    if (!FindPreview(name, data, target)) {
        return false;
    }

    if (data) {
        return true;
    }

    if (s_redirectDepth) {
        return false;
    }

    s_redirectDepth++;
    bool exists = SFile::FileExistsEx(target.c_str(), 0) != 0;
    s_redirectDepth--;
    return exists;
}

static bool LoadPreview(const char* name, uint8_t*& out, uint32_t& size) {
    std::shared_ptr<const std::vector<uint8_t>> data;
    std::string target;

    if (!FindPreview(name, data, target)) {
        return false;
    }

    if (data) {
        out = static_cast<uint8_t*>(Blizzard::Memory::Allocate(static_cast<uint32_t>(data->size() ? data->size() : 1), 0, __FILE__, __LINE__));
        memcpy(out, data->data(), data->size());
        size = static_cast<uint32_t>(data->size());
        return true;
    }

    if (s_redirectDepth) {
        return false;
    }

    s_redirectDepth++;
    SFileImpl* file = nullptr;
    bool ok = SFile::OpenEx(nullptr, target.c_str(), 0, &file) && file;

    if (ok) {
        uint32_t length = SFileGetFileSize(file, nullptr);
        uint8_t* buffer = static_cast<uint8_t*>(Blizzard::Memory::Allocate(length ? length : 1, 0, __FILE__, __LINE__));
        ok = !length || SFileReadFile(file, buffer, length, nullptr, nullptr, 0);
        SFileCloseFile(file);

        if (ok) {
            out = buffer;
            size = length;
        } else {
            Blizzard::Memory::Free(buffer);
        }
    }

    s_redirectDepth--;
    return ok;
}

bool Exists(const char* name) {
    if (!HasPrefix(name)) {
        return PreviewExists(name);
    }

    if (FindEmbedded(name)) {
        return true;
    }

    const SystemFont* font = FindSystemFont(name);

    return font && LoadSystemFont(*font);
}

bool Load(const char* name, uint8_t*& data, uint32_t& size) {
    if (!HasPrefix(name)) {
        return LoadPreview(name, data, size);
    }

    const uint8_t* source = nullptr;
    size_t length = 0;

    if (const EmbeddedFile* file = FindEmbedded(name)) {
        source = file->data;
        length = file->size;
    } else if (const SystemFont* font = FindSystemFont(name)) {
        if (const std::vector<uint8_t>* bytes = LoadSystemFont(*font)) {
            source = bytes->data();
            length = bytes->size();
        }
    }

    if (!source) {
        return false;
    }

    data = static_cast<uint8_t*>(Blizzard::Memory::Allocate(static_cast<uint32_t>(length), 0, __FILE__, __LINE__));
    memcpy(data, source, length);
    size = static_cast<uint32_t>(length);

    return true;
}

}
