#include "HoT/Editor/Dbc/Store.h"
#include <cctype>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "HoT/DB/ClientDB.h"
#include "HoT/DB/Custom.h"
#include "WoW/Source/DB/DBClient/DBClient.h"

namespace HoT::Editor::Dbc {

template <class T>
struct PlainTable {
    static_assert(!T::s_generatedID, "generated id tables have no id column to edit");

    static inline WowClientDB<T>* s_db = nullptr;
    static inline T* s_block = nullptr;
    static inline int32_t s_capacity = 0;
    // Record arrays replaced by a grow, cached pointers into them still get every in-place write.
    static inline std::vector<std::pair<T*, int32_t>> s_retired;

    static void Sync() {
        if (s_db->m_records != s_block) {
            s_block = s_db->m_records;
            s_capacity = s_db->m_numRecords;
            s_retired.clear();
        }
    }

    static T* Find(int32_t id) {
        WowClientDB<T>& db = *s_db;

        if (!db.m_records || db.m_numRecords <= 0) {
            return nullptr;
        }

        if constexpr (s_indexedById<T>) {
            if (!db.m_recordsById || id < db.m_minID || id > db.m_maxID) {
                return nullptr;
            }

            return db.m_recordsById[id - db.m_minID];
        } else {
            for (int32_t i = 0; i < db.m_numRecords; i++) {
                if (db.m_records[i].m_id == id) {
                    return &db.m_records[i];
                }
            }

            return nullptr;
        }
    }

    static bool Read(int32_t id, uint8_t* out) {
        T* record = Find(id);

        if (!record) {
            return false;
        }

        memcpy(out, record, sizeof(T));
        return true;
    }

    static bool ReadIndex(int32_t index, uint8_t* out) {
        if (index < 0 || index >= s_db->m_numRecords || !s_db->m_records) {
            return false;
        }

        memcpy(out, &s_db->m_records[index], sizeof(T));
        return true;
    }

    static void RebuildIndex(int32_t minId, int32_t maxId) {
        WowClientDB<T>& db = *s_db;

        if constexpr (s_indexedById<T>) {
            uint32_t count = static_cast<uint32_t>(maxId - minId) + 1;
            T** index = new (__FILE__, __LINE__) T*[count];
            memset(index, 0, sizeof(T*) * count);

            for (int32_t i = 0; i < db.m_numRecords; i++) {
                index[db.m_records[i].m_id - minId] = &db.m_records[i];
            }

            if (db.m_recordsById) {
                HoT::DB::Retire(db.m_recordsById, sizeof(T*) * (db.m_maxID - db.m_minID + 1));
            }

            db.m_recordsById = index;
        }

        db.m_minID = minId;
        db.m_maxID = maxId;
    }

    static bool Write(int32_t id, const uint8_t* record, bool* inserted) {
        Sync();
        WowClientDB<T>& db = *s_db;

        if (T* existing = Find(id)) {
            memcpy(existing, record, sizeof(T));
            int32_t index = static_cast<int32_t>(existing - db.m_records);

            for (auto& [block, count] : s_retired) {
                if (index < count) {
                    memcpy(&block[index], record, sizeof(T));
                }
            }

            *inserted = false;
            return true;
        }

        int32_t count = db.m_numRecords > 0 && db.m_records ? db.m_numRecords : 0;
        bool moved = false;

        if (count + 1 > s_capacity) {
            int32_t capacity = count + 16 + count / 4;
            T* block = static_cast<T*>(SMemAlloc(sizeof(T) * capacity, __FILE__, __LINE__, 0));

            if (count) {
                memcpy(block, db.m_records, sizeof(T) * count);
                s_retired.emplace_back(db.m_records, count);
            }

            db.m_records = block;
            s_block = block;
            s_capacity = capacity;
            moved = true;
        }

        memcpy(&db.m_records[count], record, sizeof(T));
        db.m_numRecords = count + 1;

        int32_t minId = count && db.m_minID < id ? db.m_minID : id;
        int32_t maxId = count && db.m_maxID > id ? db.m_maxID : id;

        if (moved || minId != db.m_minID || maxId != db.m_maxID || !db.m_recordsById) {
            RebuildIndex(minId, maxId);
        } else if constexpr (s_indexedById<T>) {
            db.m_recordsById[id - minId] = &db.m_records[count];
        }

        db.m_loaded = 1;
        *inserted = true;
        return true;
    }
};

template <class T>
struct RleTable {
    struct Patch {
        T* original;
        T* patched;
        void (*apply)(T&);
    };

    static inline WowClientDB_RLE<T>* s_db = nullptr;
    static inline std::unordered_set<T*> s_owned;
    static inline std::unordered_map<int32_t, Patch> s_patches;

    static void Decode(const T* storage, uint8_t* out) {
        if (g_clientDbPerformCompress) {
            DecompressRow(reinterpret_cast<const uint8_t*>(storage), sizeof(T), out);
        } else {
            memcpy(out, storage, sizeof(T));
        }
    }

    static T* Encode(const uint8_t* record) {
        T* storage;

        if (g_clientDbPerformCompress) {
            uint32_t size = CompressRow(record, sizeof(T), nullptr);
            storage = static_cast<T*>(SMemAlloc(size, __FILE__, __LINE__, 0));
            CompressRow(record, sizeof(T), reinterpret_cast<uint8_t*>(storage));
        } else {
            storage = static_cast<T*>(SMemAlloc(sizeof(T), __FILE__, __LINE__, 0));
            memcpy(storage, record, sizeof(T));
        }

        s_owned.insert(storage);
        return storage;
    }

    static void Release(T* storage) {
        if (s_owned.erase(storage)) {
            SMemFree(storage, __FILE__, __LINE__, 0);
        }
    }

    static T** SlotFor(int32_t id) {
        WowClientDB_RLE<T>& db = *s_db;

        if (!db.m_recordsById || db.m_numRecords <= 0 || id < db.m_minID || id > db.m_maxID) {
            return nullptr;
        }

        T** slot = &db.m_recordsById[id - db.m_minID];
        return *slot ? slot : nullptr;
    }

    // A table reload drops the patched pointer, the stale patch is forgotten then.
    static Patch* LivePatch(int32_t id, T** slot) {
        auto it = s_patches.find(id);

        if (it == s_patches.end()) {
            return nullptr;
        }

        if (!slot || *slot != it->second.patched) {
            s_patches.erase(it);
            return nullptr;
        }

        return &it->second;
    }

    static void Replace(T* from, T* to, int32_t id) {
        WowClientDB_RLE<T>& db = *s_db;

        if (T** slot = SlotFor(id)) {
            *slot = to;
        }

        for (int32_t i = 0; i < db.m_numRecords; i++) {
            if (db.m_records[i] == from) {
                db.m_records[i] = to;
                break;
            }
        }
    }

    static bool Read(int32_t id, uint8_t* out) {
        T** slot = SlotFor(id);

        if (!slot) {
            return false;
        }

        Patch* patch = LivePatch(id, slot);
        Decode(patch ? patch->original : *slot, out);
        return true;
    }

    static bool ReadIndex(int32_t index, uint8_t* out) {
        WowClientDB_RLE<T>& db = *s_db;

        if (index < 0 || index >= db.m_numRecords || !db.m_records) {
            return false;
        }

        T* storage = db.m_records[index];

        for (auto& [id, patch] : s_patches) {
            if (patch.patched == storage) {
                storage = patch.original;
                break;
            }
        }

        Decode(storage, out);
        return true;
    }

    static T* EncodePatched(const uint8_t* record, void (*apply)(T&)) {
        alignas(T) uint8_t copy[sizeof(T)];
        memcpy(copy, record, sizeof(T));
        apply(*reinterpret_cast<T*>(copy));
        return Encode(copy);
    }

    static bool Write(int32_t id, const uint8_t* record, bool* inserted) {
        WowClientDB_RLE<T>& db = *s_db;

        if (T** slot = SlotFor(id)) {
            T* fresh = Encode(record);

            if (Patch* patch = LivePatch(id, slot)) {
                Release(patch->original);
                patch->original = fresh;

                T* patched = EncodePatched(record, patch->apply);
                T* old = patch->patched;
                Replace(old, patched, id);
                Release(old);
                patch->patched = patched;
            } else {
                T* old = *slot;
                Replace(old, fresh, id);
                Release(old);
            }

            *inserted = false;
            return true;
        }

        int32_t count = db.m_numRecords > 0 && db.m_records ? db.m_numRecords : 0;
        int32_t minId = count && db.m_minID < id ? db.m_minID : id;
        int32_t maxId = count && db.m_maxID > id ? db.m_maxID : id;
        uint32_t range = static_cast<uint32_t>(maxId - minId) + 1;

        // Same layout as LoadRecords, the index by position then the index by id in one block.
        T** block = static_cast<T**>(SMemAlloc(sizeof(T*) * (count + 1 + range), __FILE__, __LINE__, 0));
        T** byId = &block[count + 1];
        memset(byId, 0, sizeof(T*) * range);

        if (count) {
            memcpy(block, db.m_records, sizeof(T*) * count);

            if (db.m_recordsById) {
                memcpy(&byId[db.m_minID - minId], db.m_recordsById, sizeof(T*) * (db.m_maxID - db.m_minID + 1));
            }
        }

        T* fresh = Encode(record);
        block[count] = fresh;
        byId[id - minId] = fresh;

        if (db.m_records) {
            HoT::DB::Retire(db.m_records, count ? static_cast<uint32_t>(sizeof(T*) * (count + db.m_maxID - db.m_minID + 1)) : 0);
        }

        db.m_records = block;
        db.m_recordsById = byId;
        db.m_numRecords = count + 1;
        db.m_minID = minId;
        db.m_maxID = maxId;
        db.m_loaded = 1;

        *inserted = true;
        return true;
    }

    static bool ApplyPatch(int32_t id, void (*apply)(T&)) {
        T** slot = SlotFor(id);

        if (!slot || LivePatch(id, slot)) {
            return false;
        }

        alignas(T) uint8_t record[sizeof(T)];
        Decode(*slot, record);

        T* original = *slot;
        T* patched = EncodePatched(record, apply);
        Replace(original, patched, id);
        s_patches[id] = { original, patched, apply };

        return true;
    }

    static void RemovePatch(int32_t id) {
        auto it = s_patches.find(id);

        if (it == s_patches.end()) {
            return;
        }

        Patch patch = it->second;
        s_patches.erase(it);

        T** slot = SlotFor(id);

        if (slot && *slot == patch.patched) {
            Replace(patch.patched, patch.original, id);
        }

        Release(patch.patched);
    }
};

template <class T>
static Store Make(WowClientDB<T>& db, const char* stem) {
    PlainTable<T>::s_db = &db;
    return { stem, &db, static_cast<uint32_t>(sizeof(T)), PlainTable<T>::Read, PlainTable<T>::ReadIndex, PlainTable<T>::Write };
}

template <class T>
static Store Make(WowClientDB_RLE<T>& db, const char* stem) {
    RleTable<T>::s_db = &db;
    return { stem, &db, static_cast<uint32_t>(sizeof(T)), RleTable<T>::Read, RleTable<T>::ReadIndex, RleTable<T>::Write };
}

const std::vector<Store>& Stores() {
    static const std::vector<Store> stores = {
        Make(g_animationDataDB, "AnimationData"),
        Make(g_creatureDisplayInfoDB, "CreatureDisplayInfo"),
        Make(g_creatureDisplayInfoExtraDB, "CreatureDisplayInfoExtra"),
        Make(g_creatureModelDataDB, "CreatureModelData"),
        Make(g_gameObjectDisplayInfoDB, "GameObjectDisplayInfo"),
        Make(g_lightDB, "Light"),
        Make(g_lightFloatBandDB, "LightFloatBand"),
        Make(g_lightIntBandDB, "LightIntBand"),
        Make(g_lightParamsDB, "LightParams"),
        Make(g_lightSkyboxDB, "LightSkybox"),
        Make(g_skillLineAbilityDB, "SkillLineAbility"),
        Make(g_soundEntriesDB, "SoundEntries"),
        Make(g_spellDB, "Spell"),
        Make(g_spellCastTimesDB, "SpellCastTimes"),
        Make(g_spellCategoryDB, "SpellCategory"),
        Make(g_spellChainEffectsDB, "SpellChainEffects"),
        Make(HoT::DB::g_spellClassMaskExtensionDB, "SpellClassMaskExtension"),
        Make(HoT::DB::g_spellCustomAttrDB, "SpellCustomAttr"),
        Make(g_spellDescriptionVariablesDB, "SpellDescriptionVariables"),
        Make(g_spellDifficultyDB, "SpellDifficulty"),
        Make(g_spellDispelTypeDB, "SpellDispelType"),
        Make(g_spellDurationDB, "SpellDuration"),
        Make(g_spellEffectCameraShakesDB, "SpellEffectCameraShakes"),
        Make(g_spellFocusObjectDB, "SpellFocusObject"),
        Make(g_spellIconDB, "SpellIcon"),
        Make(g_spellItemEnchantmentDB, "SpellItemEnchantment"),
        Make(g_spellItemEnchantmentConditionDB, "SpellItemEnchantmentCondition"),
        Make(g_spellMechanicDB, "SpellMechanic"),
        Make(g_spellMissileDB, "SpellMissile"),
        Make(g_spellMissileMotionDB, "SpellMissileMotion"),
        Make(g_spellRadiusDB, "SpellRadius"),
        Make(g_spellRangeDB, "SpellRange"),
        Make(g_spellRuneCostDB, "SpellRuneCost"),
        Make(g_spellShapeshiftFormDB, "SpellShapeshiftForm"),
        Make(g_spellVisualDB, "SpellVisual"),
        Make(g_spellVisualEffectNameDB, "SpellVisualEffectName"),
        Make(g_spellVisualKitDB, "SpellVisualKit"),
        Make(g_spellVisualKitAreaModelDB, "SpellVisualKitAreaModel"),
        Make(g_spellVisualKitModelAttachDB, "SpellVisualKitModelAttach"),
        Make(g_areaGroupDB, "AreaGroup"),
        Make(g_areaTableDB, "AreaTable"),
        Make(g_auctionHouseDB, "AuctionHouse"),
        Make(g_charTitlesDB, "CharTitles"),
        Make(g_cinematicCameraDB, "CinematicCamera"),
        Make(g_creatureSpellDataDB, "CreatureSpellData"),
        Make(g_factionDB, "Faction"),
        Make(g_factionTemplateDB, "FactionTemplate"),
        Make(g_gameObjectArtKitDB, "GameObjectArtKit"),
        Make(g_gemPropertiesDB, "GemProperties"),
        Make(g_holidayNamesDB, "HolidayNames"),
        Make(g_holidaysDB, "Holidays"),
        Make(g_itemDisplayInfoDB, "ItemDisplayInfo"),
        Make(g_itemLimitCategoryDB, "ItemLimitCategory"),
        Make(g_itemPetFoodDB, "ItemPetFood"),
        Make(g_itemRandomPropertiesDB, "ItemRandomProperties"),
        Make(g_itemRandomSuffixDB, "ItemRandomSuffix"),
        Make(g_itemSetDB, "ItemSet"),
        Make(g_languagesDB, "Languages"),
        Make(g_lockDB, "Lock"),
        Make(g_mailTemplateDB, "MailTemplate"),
        Make(g_mapDB, "Map"),
        Make(g_pageTextMaterialDB, "PageTextMaterial"),
        Make(g_powerDisplayDB, "PowerDisplay"),
        Make(g_skillLineDB, "SkillLine"),
        Make(g_taxiPathDB, "TaxiPath"),
        Make(g_totemCategoryDB, "TotemCategory"),
        Make(g_vehicleDB, "Vehicle"),
        Make(g_vehicleSeatDB, "VehicleSeat"),
    };

    return stores;
}

static std::string Stem(const char* name) {
    std::string s(name ? name : "");
    size_t slash = s.find_last_of("\\/");

    if (slash != std::string::npos) {
        s = s.substr(slash + 1);
    }

    size_t dot = s.find_last_of('.');

    if (dot != std::string::npos) {
        s = s.substr(0, dot);
    }

    return s;
}

const Store* FindStore(const char* table) {
    std::string want = Stem(table);

    for (const Store& store : Stores()) {
        if (_stricmp(store.stem, want.c_str()) == 0) {
            return &store;
        }
    }

    return nullptr;
}

bool PatchSpell(int32_t id, SpellPatch patch) {
    Stores();
    return patch && RleTable<SpellRec>::ApplyPatch(id, patch);
}

void UnpatchSpell(int32_t id) {
    Stores();
    RleTable<SpellRec>::RemovePatch(id);
}

}
