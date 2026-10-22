#include "HoT/Editor/Model/ModelFiles.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Platform/Files.h"
#include "HoT/Streaming/LooseFiles.h"
#include "HoT/Streaming/TextConv.h"

namespace HoT::Editor::Model::Files {

std::wstring Widen(const std::string& text) {
    return Streaming::Text::WidenUtf8(text);
}

std::string Narrow(const std::wstring& text) {
    return Streaming::Text::NarrowUtf8(text);
}

std::wstring DiskPath(const std::string& gamePath) {
    return Platform::GameFilePath(gamePath).wstring();
}

bool NormalizeGamePath(const std::string& input, std::string& output, std::string& error) {
    size_t first = input.find_first_not_of(" \t");
    size_t last = input.find_last_not_of(" \t");

    if (first == std::string::npos) {
        error = "empty path";
        return false;
    }

    std::string path;

    for (size_t i = first; i <= last; i++) {
        char c = input[i] == '/' ? '\\' : input[i];

        if (static_cast<unsigned char>(c) < 32 || strchr("<>\"|?*:", c)) {
            error = "invalid character in path";
            return false;
        }

        if (c == '\\' && (path.empty() || path.back() == '\\')) {
            if (path.empty()) {
                error = "path must be relative to the game folder";
                return false;
            }

            continue;
        }

        path += c;
    }

    if (path.empty() || path.back() == '\\') {
        error = "path names a folder";
        return false;
    }

    if (path.size() > 200) {
        error = "path is too long";
        return false;
    }

    size_t start = 0;

    while (start <= path.size()) {
        size_t end = path.find('\\', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);

        if (part == "." || part == "..") {
            error = "path must not contain . or ..";
            return false;
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    output = path;
    return true;
}

std::string Lower(const std::string& text) {
    std::string result = text;

    for (char& c : result) {
        if (c == '/') {
            c = '\\';
        } else if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return result;
}

bool EndsWith(const std::string& text, const char* suffix) {
    size_t length = strlen(suffix);
    return text.size() >= length && _stricmp(text.c_str() + text.size() - length, suffix) == 0;
}

std::string StripExtension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("\\/");

    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return path;
    }

    return path.substr(0, dot);
}

std::string FileStem(const std::string& path) {
    std::string base = StripExtension(path);
    size_t slash = base.find_last_of("\\/");
    return slash == std::string::npos ? base : base.substr(slash + 1);
}

std::string Folder(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

bool GameFileExists(const std::string& gamePath) {
    return SFile::FileExistsEx(gamePath.c_str(), 0) != 0;
}

bool IsLooseFile(const std::string& gamePath) {
    std::error_code error;
    return std::filesystem::is_regular_file(std::filesystem::path(DiskPath(gamePath)), error);
}

bool ReadGameFile(const std::string& gamePath, std::vector<uint8_t>& data, uint32_t maxSize) {
    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, gamePath.c_str(), 0, &file) || !file) {
        return false;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);

    if (size > maxSize) {
        SFileCloseFile(file);
        return false;
    }

    data.resize(size);
    bool ok = !size || SFileReadFile(file, data.data(), size, nullptr, nullptr, 0) != 0;
    SFileCloseFile(file);

    if (!ok) {
        data.clear();
    }

    return ok;
}

bool WriteLoose(const std::string& gamePath, const void* data, size_t size, std::string& error) {
    std::filesystem::path path(DiskPath(gamePath));
    std::error_code code;

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), code);
    }

    if (std::filesystem::exists(path, code)) {
        if (std::filesystem::is_directory(path, code)) {
            error = gamePath + " is a folder";
            return false;
        }

        std::filesystem::path backup = path;
        backup += ".bak";

        if (!std::filesystem::exists(backup, code) && !std::filesystem::copy_file(path, backup, code)) {
            error = "could not back up " + gamePath;
            return false;
        }
    }

    std::filesystem::path temp = path;
    temp += ".tmp";
    FILE* file = Platform::OpenStdioFile(temp, "wb");

    if (!file) {
        error = "could not write " + gamePath;
        return false;
    }

    bool ok = !size || fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok;

    if (!ok || !Platform::RenameReplacing(temp, path)) {
        Platform::RemoveFile(temp);
        error = "could not write " + gamePath;
        return false;
    }

    return true;
}

void RefreshLooseIndex() {
    Streaming::LooseFiles::Invalidate();
}

}
