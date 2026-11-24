#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "HoT/Character/HD/HDCharacters.h"
#include "HoT/Character/HD/HDManifest.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "HoT/Core/Log.h"
#include "HoT/DB/Custom.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"

namespace HoT::HD {

static const int32_t GROUP_HAIR = 0;
static const int32_t HELM_GROUPS[] = { 0, 1, 2, 3, 7, 16, 17 };
static const int32_t HELM_GROUP_COUNT = 7;
static const size_t PATH_SIZE = 260;
static const char* const STOCK_HEAD_FOLDER = "Item\\ObjectComponents\\Head\\";
static const char* const HD_HEAD_FOLDER = "CharacterHD\\Item\\ObjectComponents\\Head\\";
static const char* const HD_MARKER = "_HD";

struct HelmFit {
    const char* m_name;
    float m_x;
    float m_y;
    float m_z;
    float m_scale;
};

static const HelmFit HELM_FITS[] = {
#define HELM_FIT(name, x, y, z, scale) { name, x, y, z, scale },
#include "HoT/Character/HD/HelmFit.inc"
#undef HELM_FIT
};

struct RaceFit {
    int32_t m_race;
    int32_t m_sex;
    float m_x;
    float m_y;
    float m_z;
    float m_scale;
};

static const RaceFit RACE_FITS[] = {
#define HELM_RACE_FIT(race, sex, x, y, z, scale) { race, sex, x, y, z, scale },
#include "HoT/Character/HD/HelmRaceFit.inc"
#undef HELM_RACE_FIT
};

struct HelmAttach {
    int32_t m_race;
    int32_t m_sex;
    float m_x;
    float m_y;
    float m_z;
};

static const HelmAttach HELM_ATTACH[] = {
#define HELM_ATTACH(race, sex, x, y, z) { race, sex, x, y, z },
#include "HoT/Character/HD/HelmAttach.inc"
#undef HELM_ATTACH
};

static std::unordered_map<std::string, const HelmFit*> s_helmFits;

static bool StartsWithNoCase(const char* text, const char* prefix) {
    return SStrCmpI(text, prefix, strlen(prefix)) == 0;
}

static const HelmFit* HelmFitFor(const char* stem) {
    if (!stem || !*stem) {
        return nullptr;
    }

    if (s_helmFits.empty()) {
        s_helmFits.reserve(sizeof(HELM_FITS) / sizeof(HELM_FITS[0]));

        for (const HelmFit& fit : HELM_FITS) {
            s_helmFits.emplace(fit.m_name, &fit);
        }
    }

    const char* name = stem;

    for (const char* p = stem; *p; p++) {
        if (*p == '\\' || *p == '/') {
            name = p + 1;
        }
    }

    std::string key(name);
    size_t dot = key.rfind('.');

    if (dot != std::string::npos) {
        key.erase(dot);
    }

    for (char& c : key) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    auto it = s_helmFits.find(key);
    return it == s_helmFits.end() ? nullptr : it->second;
}

static const RaceFit* RaceFitFor(int32_t race, int32_t sex) {
    for (const RaceFit& fit : RACE_FITS) {
        if (fit.m_race == race && fit.m_sex == sex) {
            return &fit;
        }
    }

    return nullptr;
}

static const HelmAttach* AttachFor(int32_t race, int32_t sex) {
    for (const HelmAttach& row : HELM_ATTACH) {
        if (row.m_race == race && row.m_sex == sex) {
            return &row;
        }
    }

    return nullptr;
}

static bool IsRedirectedHelm(const CM2Model* model) {
    if (!model || model->m_attachmentId != ATTACH_HELM) {
        return false;
    }

    const char* stem = ModelStem(model);
    return stem && StartsWithNoCase(stem, HD_HEAD_FOLDER);
}

static bool HdHelmPath(const char* modelPath, char* model) {
    if (!modelPath) {
        return false;
    }

    size_t folder = strlen(STOCK_HEAD_FOLDER);

    if (!StartsWithNoCase(modelPath, STOCK_HEAD_FOLDER)) {
        return false;
    }

    const char* name = modelPath + folder;
    const char* dot = strrchr(name, '.');
    int32_t stem = dot ? static_cast<int32_t>(dot - name) : static_cast<int32_t>(strlen(name));
    char probe[PATH_SIZE];
    snprintf(probe, sizeof(probe), "%s%.*s%s.m2", HD_HEAD_FOLDER, stem, name, HD_MARKER);

    if (!FileExists(probe)) {
        return false;
    }

    snprintf(model, PATH_SIZE, "%s%.*s%s%s", HD_HEAD_FOLDER, stem, name, HD_MARKER, dot ? dot : ".mdx");
    return true;
}

static int32_t HelmVisId(const CCharacterComponent* component) {
    int32_t displayId = static_cast<int32_t>(component->m_itemDisplayID[ITEMSLOT_HEAD]);

    if (displayId <= 0) {
        return 0;
    }

    ItemDisplayInfoRec rec;

    if (!g_itemDisplayInfoDB.GetRecord(displayId, &rec)) {
        return 0;
    }

    uint32_t sex = component->m_data.m_preferences.m_sexID;

    if (sex > 1) {
        return 0;
    }

    return rec.m_helmetGeosetVisID[sex];
}

static const DB::HDHelmHideRec* HelmHideFor(int32_t visId, int32_t race) {
    WowClientDB<DB::HDHelmHideRec>& db = DB::g_hdHelmHideDB;

    if (visId <= 0 || race < 0 || !db.m_loaded || db.m_numRecords <= 0) {
        return nullptr;
    }

    return db.GetRecord(static_cast<int32_t>(static_cast<uint32_t>(visId) * 100u + static_cast<uint32_t>(race)));
}

// HDHelmHide rows replace the stock HelmetGeosetVisData hides and can clear whole customization groups.
static void ApplyRetailHelmHides(CCharacterComponent* component, const int32_t* before) {
    int32_t race = static_cast<int32_t>(component->m_data.m_preferences.m_raceID);
    const DB::HDHelmHideRec* row = HelmHideFor(HelmVisId(component), race);

    if (!row) {
        return;
    }

    uint32_t* groups = component->m_data.m_geosets;
    int32_t added = 0;
    int32_t restored = 0;

    for (int32_t i = 0; i < HELM_GROUP_COUNT; i++) {
        int32_t group = HELM_GROUPS[i];
        uint32_t hidden = static_cast<uint32_t>(group * 100 + 1);
        bool wanted = (row->m_maskGroups & (1u << i)) != 0;

        if (wanted && groups[group] != hidden) {
            groups[group] = hidden;
            added++;
        } else if (!wanted && groups[group] == hidden && static_cast<uint32_t>(before[i]) != hidden) {
            groups[group] = static_cast<uint32_t>(before[i]);
            restored++;
        }
    }

    CM2Model* model = component->m_data.m_model;
    int32_t cleared = 0;

    if (model) {
        for (uint32_t i = 0; i < DB::HD_HELM_HIDE_EXTRA_SLOTS; i++) {
            uint32_t group = row->m_groups[i];

            if (!group) {
                continue;
            }

            model->SetGeometryVisible(group * 100, group * 100 + 99, 0);
            cleared++;
        }
    }

    if (added || restored || cleared) {
        LOG_DEBUG << "hd characters: helm vis " << row->m_visID << " race " << race << " hid " << added << " groups, put back " << restored << ", cleared " << cleared << " customisation groups";
    }
}

// The manifest picks the geoset an HD model shows in place of a hidden helm group.
static void ApplyHelmHides(CCharacterComponent* component, const int32_t* before) {
    const Manifest::Data* data = Manifest::Of(component);

    if (!data) {
        return;
    }

    uint32_t* groups = component->m_data.m_geosets;
    std::vector<int32_t> shows;
    bool resolved = false;

    for (int32_t i = 0; i < HELM_GROUP_COUNT; i++) {
        int32_t group = HELM_GROUPS[i];
        int32_t hidden = group * 100 + 1;

        if (static_cast<int32_t>(groups[group]) != hidden || before[i] == hidden) {
            continue;
        }

        int32_t value = hidden;
        auto it = data->m_helms.find(group);

        if (it != data->m_helms.end()) {
            value = it->second;
        }

        if (group == GROUP_HAIR) {
            if (!resolved) {
                Manifest::ResolveShows(*data, Manifest::LookOf(component), shows, true);
                resolved = true;
            }

            for (int32_t id : shows) {
                if (id / 100 == GROUP_HAIR && id % 100 == 0) {
                    value = 0;
                }
            }
        }

        if (value == static_cast<int32_t>(groups[group])) {
            continue;
        }

        LOG_DEBUG << "hd characters: helm hides group " << group << ", geoset " << value << " instead of " << hidden;
        groups[group] = static_cast<uint32_t>(value);
    }
}

static void ApplyHelmAttach(CCharacterComponent* component) {
    int32_t race = static_cast<int32_t>(component->m_data.m_preferences.m_raceID);
    int32_t sex = static_cast<int32_t>(component->m_data.m_preferences.m_sexID);
    CM2Model* parent = component->m_data.m_model;

    if (!parent) {
        return;
    }

    CM2Model* link = parent->m_attachList;

    if (!IsRedirectedHelm(link)) {
        return;
    }

    const HelmAttach* row = AttachFor(race, sex);

    if (!row) {
        return;
    }

    C44Matrix& placement = link->m_worldTransform;
    placement.Identity();
    placement.d0 = row->m_x;
    placement.d1 = row->m_y;
    placement.d2 = row->m_z;
    LOG_DEBUG << "hd characters: retail helm " << ModelStem(link) << " moved to the retail attachment by " << row->m_x << " " << row->m_y << " " << row->m_z;
}

HelmScope::HelmScope(CCharacterComponent* component) : m_component(component) {
    for (int32_t i = 0; i < GROUP_COUNT; i++) {
        this->m_before[i] = static_cast<int32_t>(component->m_data.m_geosets[HELM_GROUPS[i]]);
    }
}

HelmScope::~HelmScope() {
    if (!this->m_component->m_hd) {
        return;
    }

    ApplyRetailHelmHides(this->m_component, this->m_before);
    ApplyHelmHides(this->m_component, this->m_before);
    ApplyHelmAttach(this->m_component);
}

const char* HelmModelPath(const CM2Model* parent, MODEL_ATTACHMENT_ID link, const char* modelPath, char* buffer) {
    if (link != ATTACH_HELM || !IsHdModel(parent) || !HdHelmPath(modelPath, buffer)) {
        return modelPath;
    }

    LOG_DEBUG << "hd characters: helm " << modelPath << " redirected to " << buffer;
    return buffer;
}

void CopyHelmPlacement(CM2Model* copy, const CM2Model* source) {
    if (!copy || !IsRedirectedHelm(source)) {
        return;
    }

    copy->m_worldTransform = source->m_worldTransform;
}

void ApplyHelmFit(CCharacterComponent* component) {
    if (!component) {
        return;
    }

    int32_t race = static_cast<int32_t>(component->m_data.m_preferences.m_raceID);
    int32_t sex = static_cast<int32_t>(component->m_data.m_preferences.m_sexID);

    if (race < 0 || race >= RACE_COUNT || sex < 0 || sex > 1) {
        return;
    }

    CM2Model* parent = component->m_data.m_model;

    if (!parent) {
        return;
    }

    CM2Model* link = parent->m_attachList;

    if (!link || link->m_attachmentId != ATTACH_HELM) {
        return;
    }

    const char* stem = ModelStem(link);
    const HelmFit* fit = HelmFitFor(stem);
    const RaceFit* raceFit = fit ? nullptr : RaceFitFor(race, sex);
    float offset[3] = { 0.0f, 0.0f, 0.0f };
    float scale;

    if (fit) {
        offset[0] = fit->m_x;
        offset[1] = fit->m_y;
        offset[2] = fit->m_z;
        scale = fit->m_scale;
    } else if (raceFit) {
        offset[0] = raceFit->m_x;
        offset[1] = raceFit->m_y;
        offset[2] = raceFit->m_z;
        scale = raceFit->m_scale;
    } else {
        scale = BakedScale(race, sex);

        if (scale == 1.0f) {
            return;
        }
    }

    C44Matrix& placement = link->m_worldTransform;
    placement.Identity();
    placement.Scale(C3Vector(scale, scale, scale));
    placement.d0 = offset[0];
    placement.d1 = offset[1];
    placement.d2 = offset[2];
    LOG_DEBUG << "hd characters: helm " << (stem ? stem : "?") << " scaled by " << scale << " and moved by " << offset[0] << " " << offset[1] << " " << offset[2] << (fit ? " (measured)" : " (race median)");
}

}
