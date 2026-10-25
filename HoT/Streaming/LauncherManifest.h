#ifndef HOT_STREAMING_LAUNCHERMANIFEST_H
#define HOT_STREAMING_LAUNCHERMANIFEST_H

#include <string>
#include <vector>

namespace HoT::Streaming {

constexpr int kManifestLevel = 2;

struct ManifestFile {
    // Forward slash and client relative, like "Data/patch-4.MPQ".
    std::string m_path;
    long long m_size = 0;
    std::string m_sha256;
    // "q1:<hex>" sampled digest for MPQs, empty means check m_sha256.
    std::string m_quick;
    bool m_hd = false;
    // "linux" or "macos" for a native build only that OS takes, empty for every client.
    std::string m_platform;
    int m_requiredLevel = 0;
};

// An MPQ that carries a (hotmanifest), so it can be brought up to date file by file.
struct ManifestArchive {
    std::string m_path;
    std::string m_contentId;
};

struct Manifest {
    std::string m_baseUrl;
    std::vector<ManifestFile> m_files;
    std::vector<ManifestArchive> m_archives;
};

struct LauncherConfig {
    std::wstring m_manifestUrl;
    std::string m_patchBaseUrl;
    std::string m_launcherExe;
};

// Reads <installDir>/launcher.json, or keeps the built in defaults when it is missing.
void LoadLauncherConfig(const std::wstring& installDir, LauncherConfig& out);

bool ParseManifest(const std::string& json, Manifest& out);

}

#endif
