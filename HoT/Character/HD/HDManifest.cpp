#include <algorithm>
#include <memory>
#include "HoT/Character/HD/HDManifest.h"
#include "HoT/Core/Log.h"
#include "HoT/DB/Custom.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/Types.h"

namespace HoT::HD::Manifest {

static std::map<int32_t, std::unique_ptr<Data>> s_manifests;

static int32_t RectOf(uint32_t rect) {
    if (rect == DB::HD_MANIFEST_RECT_FACE) {
        return RECT_FACE;
    }

    if (rect == DB::HD_MANIFEST_RECT_BODY) {
        return RECT_BODY;
    }

    if (rect >= DB::HD_MANIFEST_RECT_SECTION) {
        return static_cast<int32_t>(rect - DB::HD_MANIFEST_RECT_SECTION);
    }

    return RECT_WHOLE;
}

static void CondsOf(const DB::HDCharacterManifestRec& row, Entry& entry) {
    for (const DB::HDCharacterManifestRec::Cond& cond : row.m_conds) {
        if (cond.m_option > 0 && cond.m_option < static_cast<uint32_t>(OPTION_COUNT)) {
            entry.m_conds.push_back({ static_cast<int32_t>(cond.m_option), static_cast<int32_t>(cond.m_value) });
        }
    }
}

void Build() {
    s_manifests.clear();

    WowClientDB<DB::HDCharacterManifestRec>& db = DB::g_hdCharacterManifestDB;
    int32_t count = db.m_loaded ? db.m_numRecords : 0;

    if (count <= 0 || !db.m_records) {
        LOG_INFO << "hd characters: DBFilesClient\\HDCharacterManifest.dbc has no rows, hd models cannot be dressed";
        return;
    }

    for (int32_t i = 0; i < count; i++) {
        const DB::HDCharacterManifestRec& row = db.m_records[i];
        std::unique_ptr<Data>& slot = s_manifests[static_cast<int32_t>(row.m_race) * 2 + static_cast<int32_t>(row.m_sex)];

        if (!slot) {
            slot = std::make_unique<Data>();
        }

        Data& data = *slot;
        const char* path = row.m_path ? row.m_path : "";

        switch (row.m_kind) {
            case DB::HD_MANIFEST_MODEL: {
                data.m_model = path;
                break;
            }

            case DB::HD_MANIFEST_TEXTURE: {
                TexLine entry;
                entry.m_target = static_cast<int32_t>(row.m_target);
                entry.m_path = path;
                CondsOf(row, entry);
                data.m_textures.push_back(std::move(entry));
                break;
            }

            case DB::HD_MANIFEST_SLOT: {
                SlotLine entry;
                entry.m_type = static_cast<int32_t>(row.m_target);
                entry.m_path = path;
                CondsOf(row, entry);
                data.m_slots.push_back(std::move(entry));
                break;
            }

            case DB::HD_MANIFEST_LAYER: {
                LayerLine entry;
                entry.m_order = static_cast<int32_t>(row.m_layerOrder);
                entry.m_target = static_cast<int32_t>(row.m_target);
                entry.m_rect = RectOf(row.m_rect);
                entry.m_blend = static_cast<int32_t>(row.m_blend);
                entry.m_path = path;
                CondsOf(row, entry);
                data.m_layers.push_back(std::move(entry));
                break;
            }

            case DB::HD_MANIFEST_HIDE: {
                data.m_hides.push_back({ static_cast<int32_t>(row.m_target), static_cast<int32_t>(row.m_value) });
                break;
            }

            case DB::HD_MANIFEST_SHOW: {
                ShowLine entry;
                entry.m_id = static_cast<int32_t>(row.m_target);
                CondsOf(row, entry);
                data.m_shows.push_back(std::move(entry));
                break;
            }

            case DB::HD_MANIFEST_HELM: {
                data.m_helms[static_cast<int32_t>(row.m_target)] = static_cast<int32_t>(row.m_value);
                break;
            }

            default: {
                break;
            }
        }
    }

    for (const auto& entry : s_manifests) {
        const Data& data = *entry.second;
        LOG_DEBUG << "hd characters: manifest race " << (entry.first / 2) << " sex " << (entry.first % 2)
                  << " tex=" << data.m_textures.size() << " slots=" << data.m_slots.size()
                  << " layers=" << data.m_layers.size() << " hides=" << data.m_hides.size()
                  << " shows=" << data.m_shows.size() << " helms=" << data.m_helms.size();
    }

    LOG_INFO << "hd characters: DBFilesClient\\HDCharacterManifest.dbc loaded, " << count << " lines dress " << s_manifests.size() << " race and sex pairs";
}

bool Entry::Matches(const Look& look) const {
    for (const Cond& cond : this->m_conds) {
        if (look.m_values[cond.m_option] != cond.m_value) {
            return false;
        }
    }

    return true;
}

const Data* Get(int32_t race, int32_t sex) {
    auto it = s_manifests.find(race * 2 + sex);
    return it == s_manifests.end() ? nullptr : it->second.get();
}

const Data* Of(const CCharacterComponent* component) {
    const CharacterPreferences& prefs = component->m_data.m_preferences;
    return Get(static_cast<int32_t>(prefs.m_raceID), static_cast<int32_t>(prefs.m_sexID));
}

Look LookOf(const CCharacterComponent* component) {
    const CharacterPreferences& prefs = component->m_data.m_preferences;
    Look look;
    look.m_values[OPTION_SKIN_COLOR] = static_cast<int32_t>(prefs.m_skinID);
    look.m_values[OPTION_FACE] = static_cast<int32_t>(prefs.m_faceId);
    look.m_values[OPTION_HAIR_STYLE] = static_cast<int32_t>(prefs.m_hairStyleId);
    look.m_values[OPTION_HAIR_COLOR] = static_cast<int32_t>(prefs.m_hairColorId);
    look.m_values[OPTION_FACIAL] = static_cast<int32_t>(prefs.m_facialHairId);
    look.m_values[OPTION_DEATH_KNIGHT] = prefs.m_classId == CLASS_DEATH_KNIGHT ? 1 : 0;
    return look;
}

const char* Texture(const Data& data, int32_t target, const Look& look) {
    for (const TexLine& line : data.m_textures) {
        if (line.m_target == target && line.Matches(look)) {
            return line.m_path.c_str();
        }
    }

    return nullptr;
}

const char* Slot(const Data& data, int32_t type, const Look& look) {
    for (const SlotLine& line : data.m_slots) {
        if (line.m_type == type && line.Matches(look)) {
            return line.m_path.c_str();
        }
    }

    return nullptr;
}

void ResolveLayers(const Data& data, const Look& look, std::vector<const LayerLine*>& out) {
    out.clear();

    for (const LayerLine& line : data.m_layers) {
        if (!line.Matches(look)) {
            continue;
        }

        bool taken = false;

        for (const LayerLine* chosen : out) {
            if (chosen->m_target == line.m_target) {
                taken = true;
                break;
            }
        }

        if (!taken) {
            out.push_back(&line);
        }
    }

    std::stable_sort(out.begin(), out.end(), [](const LayerLine* a, const LayerLine* b) {
        return a->m_order < b->m_order;
    });
}

void ResolveShows(const Data& data, const Look& look, std::vector<int32_t>& out, bool keepMarkers) {
    out.clear();
    std::vector<int32_t> matchedGroups;

    for (const ShowLine& line : data.m_shows) {
        if (line.m_conds.empty() || !line.Matches(look)) {
            continue;
        }

        matchedGroups.push_back(line.m_id / 100);

        if (line.m_id % 100 != 0 || keepMarkers) {
            out.push_back(line.m_id);
        }
    }

    for (const ShowLine& line : data.m_shows) {
        if (!line.m_conds.empty() || (line.m_id % 100 == 0 && !keepMarkers)) {
            continue;
        }

        if (std::find(matchedGroups.begin(), matchedGroups.end(), line.m_id / 100) == matchedGroups.end()) {
            out.push_back(line.m_id);
        }
    }
}

}
