#ifndef HOT_EDITOR_RESOURCES_H
#define HOT_EDITOR_RESOURCES_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The game editor and graphics profiler UIs are compiled into the exe and served when no loose file or archive has them.
namespace HoT::Editor::Resources {

struct EmbeddedFile {
    const char* path;
    const uint8_t* data;
    size_t size;
};

bool Exists(const char* name);

// The buffer comes from Blizzard::Memory::Allocate so SFileImpl frees it like its own.
bool Load(const char* name, uint8_t*& data, uint32_t& size);

// Model editor previews live under this prefix and are only served while no loose file or archive has the name.
extern const char* const kPreviewPrefix;

void AddPreviewFile(const std::string& name, std::shared_ptr<const std::vector<uint8_t>> data, const std::string& source = std::string());

// The game path a preview file was made from, or an empty string when the name is not a preview.
std::string PreviewSource(const char* name);

// A name of prefix plus a NN.skin or NNNN-NN.anim suffix loads target plus that suffix.
void AddPreviewRedirect(const std::string& prefix, const std::string& target);

// A name of prefix plus anything ending in .blp loads target plus that rest, so creature skins next to a preview model resolve.
void AddPreviewTextureRedirect(const std::string& prefix, const std::string& target);

// Drops every preview file and redirect whose name starts with prefix.
void RemovePreviews(const std::string& prefix);

}

#endif
