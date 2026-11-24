#include <filesystem>
#include <fstream>
#include <system_error>
#include <unordered_set>
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Net/Packet.h"
#include "HoT/Streaming/Paths.h"
#include "WoW/Source/Net/Types.h"

namespace HoT::CinematicSkip {

enum Kind : char {
    KIND_CINEMATIC = 'c',
    KIND_MOVIE = 'm',
};

static bool s_enabled = false;
static bool s_loaded = false;
static std::unordered_set<uint64_t> s_viewed;

static void OnEnabledChanged(const char* value) {
    s_enabled = CVars::Bool(value);
}

static std::filesystem::path FilePath() {
    return Streaming::Paths::GameDir() / "Cache" / "ViewedCinematics.txt";
}

static uint64_t Key(Kind kind, uint32_t id) {
    return (static_cast<uint64_t>(static_cast<uint8_t>(kind)) << 32) | id;
}

static void Load() {
    if (s_loaded) {
        return;
    }

    s_loaded = true;

    std::ifstream in(FilePath());
    char kind = 0;
    uint32_t id = 0;

    while (in >> kind >> id) {
        if (kind == KIND_CINEMATIC || kind == KIND_MOVIE) {
            s_viewed.insert(Key(static_cast<Kind>(kind), id));
        }
    }
}

static void Append(Kind kind, uint32_t id) {
    std::filesystem::path path = FilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::app);
    out << static_cast<char>(kind) << ' ' << id << '\n';
}

static bool ShouldSkip(Kind kind, uint32_t id) {
    if (!id) {
        return false;
    }

    Load();

    if (s_viewed.insert(Key(kind, id)).second) {
        Append(kind, id);
        return false;
    }

    return s_enabled;
}

bool SkipCinematic(int32_t cinematicId) {
    if (!ShouldSkip(KIND_CINEMATIC, static_cast<uint32_t>(cinematicId))) {
        return false;
    }

    Net::Packet(CMSG_COMPLETE_CINEMATIC).Send();
    return true;
}

bool SkipMovie(uint32_t movieId) {
    if (!ShouldSkip(KIND_MOVIE, movieId)) {
        return false;
    }

    Net::Packet(CMSG_COMPLETE_MOVIE).Send();
    return true;
}

class CinematicSkipModule : public Module {
    public:
    const char* Name() const override {
        return "CinematicSkip";
    }

    void OnInitializeGlobal() override {
        CVars::Register("skipViewedCinematics", "Skip cinematics and movies that have already been watched once.", "0", OnEnabledChanged);
    }
};

Module* GetModule() {
    static CinematicSkipModule module;
    return &module;
}

}
