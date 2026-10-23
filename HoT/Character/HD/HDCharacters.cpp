#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "HoT/Character/HD/HDCharacters.h"
#include "HoT/Character/HD/HDBlpImage.h"
#include "HoT/Character/HD/HDManifest.h"
#include "HoT/Character/HD/HDNpcDisplay.h"
#include "HoT/Character/HD/HDSheet.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/DB/Custom.h"
#include "HoT/DB/Tables.h"
#include "HoT/Lua/Script.h"
#include "HoT/Runtime/LauncherSettings.h"
#include "HoT/Streaming/BackgroundDownloader.h"
#include "HoT/Streaming/LooseFiles.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoExtraRec.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace HoT::HD {

static const char* const HD_ROOT = "CharacterHD\\";
static const char* const STOCK_ROOT = "Character\\";
static const char* const BLANK_TEXTURE = "CharacterHD\\Blank.blp";
static const char* const MODEL_SUFFIX = "_HD.m2";
static const char* const PROBE_MODEL = "CharacterHD\\Human\\Male\\HumanMale_HD.m2";

static const int32_t TARGET_BLANK = 0;
static const int32_t TARGET_BY_NAME = -1;
static const uint32_t SLOT_TYPES[] = { 11, 12, 13 };
static const uint32_t TEXTURE_TYPE_SKIN_EXTRA = 8;
static const uint32_t UNIT_FLAG2_MIRROR_IMAGE = 0x10;
static const uint32_t DATA_FLAG_BAKED = 0x1;

static const int32_t GROUP_BOOTS = 5;
static const int32_t GROUP_EYE_GLOW = 17;
static const int32_t FIRST_UNMANAGED_GROUP = 19;
static const int32_t GEOSET_FEET = 2001;
static const int32_t GEOSET_FEET_BOOTED = 2002;

static const char* const RACE_FOLDERS[] = { nullptr, "Human", "Orc", "Dwarf", "NightElf", "Scourge", "Tauren", "Gnome", "Troll", nullptr, "BloodElf", "Draenei" };
static const char* const SEX_FOLDERS[] = { "Male", "Female" };

static bool s_enabled = false;
static int32_t s_available = -1;
static int32_t s_refreshed = 0;
static std::unordered_map<std::string, std::string> s_modelPaths;
static std::unordered_map<std::string, bool> s_fileExists;
static std::unordered_map<int32_t, float> s_bakedScales;

static std::string LowerKey(const char* path) {
    std::string key(path);

    for (char& c : key) {
        c = c == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return key;
}

static bool StartsWithNoCase(const char* text, const char* prefix) {
    return SStrCmpI(text, prefix, static_cast<uint32_t>(strlen(prefix))) == 0;
}

static int32_t TargetOf(int32_t variation, int32_t textureIndex) {
    switch (variation) {
        case VARIATION_SKIN: {
            return textureIndex == 0 ? Manifest::TARGET_SKIN : Manifest::TARGET_SKIN_EXTRA;
        }

        case VARIATION_HAIR: {
            return textureIndex == 0 ? Manifest::TARGET_HAIR : TARGET_BLANK;
        }

        case VARIATION_UNDERWEAR: {
            return textureIndex == 0 ? Manifest::TARGET_PELVIS : Manifest::TARGET_TORSO;
        }

        default: {
            return TARGET_BLANK;
        }
    }
}

static bool KindFromName(const std::string& lowerName, int32_t& variation, int32_t& textureIndex) {
    struct Kind {
        const char* m_key;
        int32_t m_variation;
        int32_t m_textureIndex;
    };

    static const Kind s_kinds[] = {
        { "nakedpelvis", 4, 0 },
        { "pelvis", 4, 0 },
        { "nakedtorso", 4, 1 },
        { "torso", 4, 1 },
        { "faciallowerhair", 2, 0 },
        { "facialupperhair", 2, 1 },
        { "facelower", 1, 0 },
        { "faceupper", 1, 1 },
        { "scalplowerhair", 3, 1 },
        { "scalpupperhair", 3, 2 },
        { "hair", 3, 0 },
        { "extra", 0, 1 },
        { "skin", 0, 0 },
    };

    for (const Kind& kind : s_kinds) {
        if (lowerName.find(kind.m_key) != std::string::npos) {
            variation = kind.m_variation;
            textureIndex = kind.m_textureIndex;
            return true;
        }
    }

    return false;
}

static bool IsPlayerUnit(const CGUnit_C* unit) {
    return unit && (unit->m_obj->m_type & TYPE_PLAYER);
}

static bool IsMirrorImageUnit(const CGUnit_C* unit) {
    return (unit->m_unit->m_unitFieldFlags2 & UNIT_FLAG2_MIRROR_IMAGE) != 0;
}

static uint32_t DisplayExtraIdOf(const CGUnit_C* unit) {
    return unit && unit->m_displayInfoExtra ? static_cast<uint32_t>(unit->m_displayInfoExtra->m_id) : 0;
}

static bool IsHdUnit(const CGUnit_C* unit) {
    if (!unit || !s_enabled) {
        return false;
    }

    if (IsPlayerUnit(unit) || IsMirrorImageUnit(unit)) {
        return true;
    }

    const DB::HDNpcDisplayRec* row = NpcDisplay::Find(DisplayExtraIdOf(unit));

    if (!row || !(row->m_flags & DB::HD_NPC_DISPLAY_HD_RACE)) {
        return false;
    }

    return (row->m_flags & (DB::HD_NPC_DISPLAY_NPC_LOOK | DB::HD_NPC_DISPLAY_BAKE)) != 0;
}

static int32_t FolderIndex(const char* const* folders, int32_t count, const std::string& name) {
    for (int32_t i = 0; i < count; i++) {
        if (folders[i] && SStrCmpI(folders[i], name.c_str(), 0x7FFFFFFF) == 0) {
            return i;
        }
    }

    return -1;
}

static bool RaceSexOfCharacterPath(const char* path, int32_t& race, int32_t& sex) {
    if (!path) {
        return false;
    }

    const char* rest = nullptr;

    if (StartsWithNoCase(path, HD_ROOT)) {
        rest = path + strlen(HD_ROOT);
    } else if (StartsWithNoCase(path, STOCK_ROOT)) {
        rest = path + strlen(STOCK_ROOT);
    } else {
        return false;
    }

    std::string text(rest);
    size_t first = text.find_first_of("\\/");

    if (first == std::string::npos) {
        return false;
    }

    size_t second = text.find_first_of("\\/", first + 1);

    if (second == std::string::npos) {
        return false;
    }

    race = FolderIndex(RACE_FOLDERS, RACE_COUNT, text.substr(0, first));
    sex = FolderIndex(SEX_FOLDERS, 2, text.substr(first + 1, second - first - 1));

    return race >= 0 && sex >= 0;
}

static bool RaceSexOfHdModel(const CM2Model* model, int32_t& race, int32_t& sex) {
    const char* stem = ModelStem(model);
    return stem && StartsWithNoCase(stem, HD_ROOT) && RaceSexOfCharacterPath(stem, race, sex);
}

// Caches misses too, InvalidateArt drops them once new art lands.
static const char* ResolveModelPath(const char* stockPath) {
    if (!stockPath || !StartsWithNoCase(stockPath, STOCK_ROOT)) {
        return nullptr;
    }

    std::string key = LowerKey(stockPath);
    auto it = s_modelPaths.find(key);

    if (it == s_modelPaths.end()) {
        std::string hd = std::string(HD_ROOT) + (stockPath + strlen(STOCK_ROOT));
        size_t dot = hd.find_last_of('.');
        size_t slash = hd.find_last_of('\\');

        if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
            hd.erase(dot);
        }

        hd += MODEL_SUFFIX;

        if (!FileExists(hd.c_str())) {
            hd.clear();
        }

        it = s_modelPaths.emplace(key, hd).first;
    }

    return it->second.empty() ? nullptr : it->second.c_str();
}

static const char* RedirectTexture(const CCharacterComponent* component, int32_t target, const Manifest::Look& look, const char* stockPath) {
    if (!stockPath || !StartsWithNoCase(stockPath, STOCK_ROOT)) {
        return stockPath;
    }

    if (target == TARGET_BY_NAME) {
        const char* fileName = strrchr(stockPath, '\\');
        fileName = fileName ? fileName + 1 : stockPath;
        int32_t variation = 0;
        int32_t textureIndex = 0;

        if (!KindFromName(LowerKey(fileName), variation, textureIndex)) {
            return stockPath;
        }

        target = TargetOf(variation, textureIndex);
    }

    if (target == TARGET_BLANK) {
        return BLANK_TEXTURE;
    }

    const Manifest::Data* data = Manifest::Of(component);

    if (!data) {
        return stockPath;
    }

    const char* path = Manifest::Texture(*data, target, look);

    if (!path) {
        LOG_DEBUG << "hd characters: no hd texture for target " << target << ", keeping " << stockPath;
        return stockPath;
    }

    return path;
}

static int32_t RefreshUnitCallback(WOWGUID guid, void* param) {
    CGObject_C* unit = ClntObjMgrObjectPtr(guid, static_cast<OBJECT_TYPE>(TYPE_UNIT | TYPE_PLAYER), __FILE__, __LINE__);

    if (unit) {
        unit->UpdateDisplayInfo(1);
        s_refreshed++;
    }

    return 1;
}

static void RefreshUnits() {
    if (!ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    s_refreshed = 0;
    ClntObjMgrEnumVisibleObjects(RefreshUnitCallback, nullptr);
    Lua::SignalEvent("UNIT_MODEL_CHANGED", "%s", "player");
    LOG_INFO << "hd characters: rebuilt " << s_refreshed << " unit models, hd=" << (s_enabled ? 1 : 0);
}

static void InvalidateContent() {
    Streaming::LooseFiles::Invalidate();
    s_fileExists.clear();
    s_modelPaths.clear();
    ClearBlpCache();
    Sheet::ClearCache();
}

static bool ProbeAvailable() {
    bool blank = FileExists(BLANK_TEXTURE);
    bool model = FileExists(PROBE_MODEL);

    if (blank && model) {
        return true;
    }

    LOG_INFO << "hd characters: the hd art is not readable, " << BLANK_TEXTURE << "=" << (blank ? 1 : 0) << " " << PROBE_MODEL << "=" << (model ? 1 : 0);
    return false;
}

void InvalidateArt() {
    InvalidateContent();
}

void SetEnabled(bool enabled) {
    if (enabled && !IsAvailable()) {
        LOG_INFO << "hd characters: the hd patch is not installed, staying on the standard models";
        enabled = false;
    }

    if (s_enabled == enabled) {
        return;
    }

    s_enabled = enabled;

    if (enabled) {
        InvalidateContent();
        NpcDisplay::Report();
    }

    RefreshUnits();
}

bool IsEnabled() {
    return s_enabled;
}

bool IsAvailable() {
    if (s_available < 0) {
        s_available = ProbeAvailable() ? 1 : 0;
    }

    return s_available == 1;
}

bool RefreshAvailability() {
    InvalidateContent();
    int32_t now = ProbeAvailable() ? 1 : 0;

    if (now != s_available) {
        LOG_INFO << "hd characters: the hd art is now " << (now ? "available" : "unavailable");
    }

    s_available = now;
    return s_available == 1;
}

void RefreshPatch() {
    bool wanted = LauncherSettings::HdPatch();
    bool available = RefreshAvailability();

    if (wanted && !available) {
        LOG_INFO << "hd characters: the hd patch is on but its art is not here yet, the models switch over as soon as it lands";
    }

    SetEnabled(wanted);
}

bool FileExists(const char* path) {
    std::string key = LowerKey(path);
    auto it = s_fileExists.find(key);

    if (it != s_fileExists.end()) {
        return it->second;
    }

    SFileImpl* file = nullptr;
    bool exists = SFile::OpenEx(nullptr, path, 0, &file) != 0 && file;

    if (exists) {
        SFileCloseFile(file);
    }

    s_fileExists.emplace(key, exists);
    return exists;
}

bool TextureExists(const char* path) {
    if (!path || !*path) {
        return false;
    }

    std::string blp = path;
    size_t dot = blp.find_last_of('.');
    size_t slash = blp.find_last_of('\\');

    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        blp.erase(dot);
    }

    blp += ".blp";
    return FileExists(blp.c_str()) || FileExists(path);
}

const char* RaceFolder(int32_t raceId) {
    if (raceId < 0 || raceId >= RACE_COUNT) {
        return nullptr;
    }

    return RACE_FOLDERS[raceId];
}

const char* SexFolder(int32_t sexId) {
    return sexId == 0 || sexId == 1 ? SEX_FOLDERS[sexId] : nullptr;
}

const char* ModelStem(const CM2Model* model) {
    if (!model || !model->m_shared) {
        return nullptr;
    }

    return model->m_shared->m_fileName;
}

bool IsHdModel(const CM2Model* model) {
    const char* stem = ModelStem(model);
    return stem && StartsWithNoCase(stem, HD_ROOT);
}

float BakedScale(int32_t race, int32_t sex) {
    int32_t key = race * 2 + sex;
    auto it = s_bakedScales.find(key);

    if (it != s_bakedScales.end()) {
        return it->second;
    }

    float baked = 1.0f;
    uint32_t displayId = GetDisplayIdFromRaceAndGender(static_cast<uint32_t>(race), static_cast<uint32_t>(sex));
    CreatureDisplayInfoRec* display = g_creatureDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId));

    if (display) {
        CreatureModelDataRec* modelData = g_creatureModelDataDB.GetRecord(display->m_modelID);

        if (modelData) {
            baked = display->m_creatureModelScale * modelData->m_modelScale;
        }
    }

    if (baked <= 0.0f) {
        baked = 1.0f;
    }

    s_bakedScales.emplace(key, baked);
    return baked;
}

const char* UnitModelPath(const CGUnit_C* unit, const char* stockPath) {
    if (!stockPath || !*stockPath || !s_enabled || !IsHdUnit(unit)) {
        return stockPath;
    }

    const char* hd = ResolveModelPath(stockPath);

    if (!hd) {
        LOG_DEBUG << "hd characters: no hd model for " << stockPath;
        return stockPath;
    }

    LOG_DEBUG << "hd characters: " << stockPath << " -> " << hd;
    return hd;
}

float UnitModelScale(const CGUnit_C* unit, const CreatureDisplayInfoRec* displayInfo, const CreatureModelDataRec* modelData, float scale) {
    if (!s_enabled || !displayInfo || !modelData || !IsHdUnit(unit)) {
        return scale;
    }

    const char* modelName = modelData->m_modelName;

    if (!ResolveModelPath(modelName)) {
        return scale;
    }

    int32_t race = 0;
    int32_t sex = 0;
    float baked = RaceSexOfCharacterPath(modelName, race, sex) ? BakedScale(race, sex) : displayInfo->m_creatureModelScale * modelData->m_modelScale;

    if (baked <= 0.0f) {
        return scale;
    }

    return scale / baked;
}

void SwapDisplayIds(const CGUnit_C* unit, uint32_t& displayId, uint32_t& nativeDisplayId) {
    if (!s_enabled || !NpcDisplay::HasSwaps()) {
        return;
    }

    uint32_t swap = NpcDisplay::Swap(displayId, IsPlayerUnit(unit));

    if (!swap || swap == displayId) {
        return;
    }

    LOG_DEBUG << "hd characters: display " << displayId << " rendered as " << swap;

    if (nativeDisplayId == displayId) {
        nativeDisplayId = swap;
    }

    displayId = swap;
}

void ApplyNpcBake(const CGUnit_C* unit, ComponentData* data) {
    if (!data || !IsHdModel(data->m_model) || !(data->m_flags & DATA_FLAG_BAKED)) {
        return;
    }

    uint32_t extraId = DisplayExtraIdOf(unit);
    const char* bake = NpcDisplay::BakePath(NpcDisplay::Find(extraId));

    if (bake && strlen(bake) < sizeof(data->m_npcSkinTexture) && FileExists(bake)) {
        strcpy(data->m_npcSkinTexture, bake);
        LOG_DEBUG << "hd characters: display extra " << extraId << " uses the baked hd texture " << bake;
        return;
    }

    data->m_flags &= ~DATA_FLAG_BAKED;
    data->m_npcSkinTexture[0] = 0;
    LOG_DEBUG << "hd characters: display extra " << extraId << " has no baked hd texture, the sheet is composited from the manifest and the npc items";
}

const char* GlueModelPath(const char* stockPath) {
    if (!stockPath || !s_enabled) {
        return stockPath;
    }

    const char* hd = ResolveModelPath(stockPath);

    if (!hd) {
        return stockPath;
    }

    LOG_DEBUG << "hd characters: glue " << stockPath << " -> " << hd;
    return hd;
}

float GlueScale(const CM2Model* model) {
    int32_t race = 0;
    int32_t sex = 0;

    if (!RaceSexOfHdModel(model, race, sex)) {
        return 1.0f;
    }

    return 1.0f / BakedScale(race, sex);
}

void OnComponentInit(CCharacterComponent* component, const ComponentData* data) {
    component->m_hd = data && IsHdModel(data->m_model);
}

void OnComponentInitDone(CCharacterComponent* component) {
    ApplySlots(component);
}

void OnComponentDestroyed(CCharacterComponent* component) {
    component->m_hd = false;
    Sheet::Destroy(component);
}

const char* BaseVariationTexture(const CCharacterComponent* component, COMPONENT_VARIATIONS variation, int32_t textureIndex, int32_t variationIndex, int32_t colorIndex, const char* stockPath) {
    if (!component->m_hd) {
        return stockPath;
    }

    Manifest::Look look = Manifest::LookOf(component);

    switch (variation) {
        case VARIATION_SKIN:
        case VARIATION_UNDERWEAR: {
            look.m_values[Manifest::OPTION_SKIN_COLOR] = colorIndex;
            break;
        }

        case VARIATION_FACE: {
            look.m_values[Manifest::OPTION_FACE] = variationIndex;
            look.m_values[Manifest::OPTION_SKIN_COLOR] = colorIndex;
            break;
        }

        case VARIATION_FACIAL_HAIR: {
            look.m_values[Manifest::OPTION_FACIAL] = variationIndex;
            look.m_values[Manifest::OPTION_HAIR_COLOR] = colorIndex;
            break;
        }

        case VARIATION_HAIR: {
            look.m_values[Manifest::OPTION_HAIR_STYLE] = variationIndex;
            look.m_values[Manifest::OPTION_HAIR_COLOR] = colorIndex;
            break;
        }

        default: {
            break;
        }
    }

    return RedirectTexture(component, TargetOf(variation, textureIndex), look, stockPath);
}

const char* NamedTexture(const CCharacterComponent* component, const char* stockPath) {
    if (!component->m_hd) {
        return stockPath;
    }

    return RedirectTexture(component, TARGET_BY_NAME, Manifest::LookOf(component), stockPath);
}

const char* HairTexture(const CCharacterComponent* component, int32_t hairStyleID, const char* stockPath) {
    if (!component->m_hd) {
        return stockPath;
    }

    Manifest::Look look = Manifest::LookOf(component);
    look.m_values[Manifest::OPTION_HAIR_STYLE] = hairStyleID;

    return RedirectTexture(component, Manifest::TARGET_HAIR, look, stockPath);
}

const char* ExtraSkinTexture(const CCharacterComponent* component, const char* stockPath) {
    if (!component->m_hd) {
        return stockPath;
    }

    return RedirectTexture(component, Manifest::TARGET_SKIN_EXTRA, Manifest::LookOf(component), stockPath);
}

bool ReplaceExtraSkinTexture(CCharacterComponent* component) {
    if (!component->m_hd) {
        return false;
    }

    CM2Model* model = component->m_data.m_model;
    const Manifest::Data* data = Manifest::Of(component);

    if (!model || !data) {
        return false;
    }

    const char* path = Manifest::Texture(*data, Manifest::TARGET_SKIN_EXTRA, Manifest::LookOf(component));

    if (!path) {
        return false;
    }

    CStatus status;
    CTexture* texture = CreateTexture(path, &status);

    if (!texture) {
        LOG_WARN << "hd characters: skin extra texture " << path << " failed to load";
        return false;
    }

    model->ReplaceTexture(TEXTURE_TYPE_SKIN_EXTRA, texture);
    HandleClose(reinterpret_cast<HOBJECT>(texture));

    return true;
}

void ApplySlots(CCharacterComponent* component) {
    if (!component->m_hd) {
        return;
    }

    CM2Model* model = component->m_data.m_model;
    const Manifest::Data* data = Manifest::Of(component);

    if (!model || !data) {
        return;
    }

    Manifest::Look look = Manifest::LookOf(component);

    for (uint32_t type : SLOT_TYPES) {
        const char* path = Manifest::Slot(*data, static_cast<int32_t>(type), look);

        if (!path) {
            continue;
        }

        CStatus status;
        CTexture* texture = CreateTexture(path, &status);

        if (!texture) {
            LOG_WARN << "hd characters: slot " << type << " texture " << path << " failed to load";
            continue;
        }

        model->ReplaceTexture(type, texture);
        HandleClose(reinterpret_cast<HOBJECT>(texture));
    }
}

static bool IsCustomizationGroup(int32_t group) {
    return group == 0 || group == 1 || group == 2 || group == 3 || group == 16;
}

void ApplyGeosets(CCharacterComponent* component) {
    if (!component->m_hd) {
        return;
    }

    CM2Model* model = component->m_data.m_model;
    const Manifest::Data* data = Manifest::Of(component);

    if (!model || !data) {
        return;
    }

    for (const Manifest::Range& range : data->m_hides) {
        model->SetGeometryVisible(static_cast<uint32_t>(range.m_from), static_cast<uint32_t>(range.m_to), 0);
    }

    std::vector<int32_t> shows;
    Manifest::ResolveShows(*data, Manifest::LookOf(component), shows);
    uint32_t boots = component->m_data.m_geosets[GROUP_BOOTS];

    for (int32_t id : shows) {
        int32_t group = id / 100;

        if (group < FIRST_UNMANAGED_GROUP && group != GROUP_EYE_GLOW) {
            continue;
        }

        if (id == GEOSET_FEET && boots != 0) {
            id = GEOSET_FEET_BOOTED;
        }

        model->SetGeometryVisible(static_cast<uint32_t>(id), static_cast<uint32_t>(id), 1);
    }
}

CustomizationScope::~CustomizationScope() {
    if (!this->m_component->m_hd) {
        return;
    }

    const Manifest::Data* data = Manifest::Of(this->m_component);

    if (!data) {
        return;
    }

    std::vector<int32_t> shows;
    Manifest::ResolveShows(*data, Manifest::LookOf(this->m_component), shows, true);
    uint32_t* groups = this->m_component->m_data.m_geosets;

    for (int32_t id : shows) {
        int32_t group = id / 100;

        if (!IsCustomizationGroup(group)) {
            continue;
        }

        groups[group] = id % 100 == 0 ? 0 : static_cast<uint32_t>(id);
    }
}

static void OnManifestReloaded(const char* stem) {
    Manifest::Build();
}

class HDCharactersModule : public Module {
    public:
    const char* Name() const override {
        return "HDCharacters";
    }

    void OnInitializeGlobal() override {
        Streaming::OnArtStreamed(InvalidateArt);
        Streaming::OnBatchApplied(RefreshPatch);
        DB::OnTableReloaded("HDCharacterManifest", OnManifestReloaded);
        LOG_INFO << "hd characters: module ready";
    }

    void OnClientDBInitialized() override {
        Manifest::Build();
        SetEnabled(LauncherSettings::HdPatch());
    }
};

Module* GetModule() {
    static HDCharactersModule module;
    return &module;
}

}
