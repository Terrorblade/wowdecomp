#ifndef HOT_EDITOR_MODEL_MODELFILES_H
#define HOT_EDITOR_MODEL_MODELFILES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Game file reads through SFile and loose writes under the game folder for the model editor.
namespace HoT::Editor::Model::Files {

std::wstring Widen(const std::string& text);
std::string Narrow(const std::wstring& text);

// Where the loose file for a game path lives on disk.
std::wstring DiskPath(const std::string& gamePath);

// Backslashes, no drive, no leading slash, no . or .. parts, false with error otherwise.
bool NormalizeGamePath(const std::string& input, std::string& output, std::string& error);

// Lower case copy with backslashes.
std::string Lower(const std::string& text);

bool EndsWith(const std::string& text, const char* suffix);

// The path without its extension and the file name without folder or extension.
std::string StripExtension(const std::string& path);
std::string FileStem(const std::string& path);
std::string Folder(const std::string& path);

bool GameFileExists(const std::string& gamePath);
bool IsLooseFile(const std::string& gamePath);
bool ReadGameFile(const std::string& gamePath, std::vector<uint8_t>& data, uint32_t maxSize = 0x20000000);

// Writes the file under the game folder through a temp file, an existing file is copied to .bak once.
bool WriteLoose(const std::string& gamePath, const void* data, size_t size, std::string& error);

// New loose files are only seen once the loose file index is rebuilt.
void RefreshLooseIndex();

}

#endif
