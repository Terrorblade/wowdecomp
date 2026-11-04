#ifndef HOT_STREAMING_CHANNELS_H
#define HOT_STREAMING_CHANNELS_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace HoT {
class Module;
}

namespace HoT::Streaming::Channels {

constexpr const char* s_release = "release";
constexpr const char* s_dev = "dev";

std::string Normalize(const std::string& name);

std::string LastChannel();
std::string Override();
bool WantsDev();
std::string BinaryChannel();
bool BinaryChannelChanged();

std::wstring DevManifestUrl(const std::wstring& releaseUrl);
std::filesystem::path DevRoot();
std::wstring DevPath(const std::string& relPath);
std::wstring DevPartPath(const std::string& relPath);

struct LayoutEntry {
    std::string m_relPath;
    bool m_inRelease = false;
    bool m_inDev = false;
    bool m_differs = false;
    std::wstring m_releasePath;
    std::wstring m_devPath;
};

void PublishLayout(std::vector<LayoutEntry> layout, bool devCurrent);
void NoteDevQueued(int32_t delta);
void NoteDevFailed();

std::string Mounted();
bool IsReady();
bool DeferMount(const std::string& openName);

bool ReportedPath(const std::wstring& dataPath, std::wstring& out);
std::vector<std::wstring> ExtraMounts();

void OnBatchFinished();
void OnCharacterEnum();
void Pump();
void OnCloseGame();

Module* GetModule();

}

#endif
