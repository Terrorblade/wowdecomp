#include <cctype>
#include <string>
#include <unordered_map>
#include "HoT/Character/HandItemDisplay.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "WoW/Common/ComponentCore/Types.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"

namespace HoT::HandItemDisplay {

static const uint8_t FOUND_WEAPON = 1;
static const uint8_t FOUND_SHIELD = 2;
static const float DEFAULT_SHIELD_OFFSET[3] = { -0.015f, -0.104f, 0.132f };

static const char* const WEAPON_FOLDER = "Item\\ObjectComponents\\Weapon\\";
static const char* const SHIELD_FOLDER = "Item\\ObjectComponents\\Shield\\";

static std::string ModelFileName(const char* modelName) {
    std::string name(modelName);

    for (char& c : name) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    size_t dot = name.rfind('.');

    if (dot != std::string::npos) {
        std::string extension = name.substr(dot);

        if (extension == ".mdx" || extension == ".mdl") {
            name.replace(dot, std::string::npos, ".m2");
        }
    }

    return name;
}

static uint8_t ProbeFolders(const std::string& fileName) {
    static std::unordered_map<std::string, uint8_t> s_found;

    auto it = s_found.find(fileName);

    if (it != s_found.end()) {
        return it->second;
    }

    uint8_t found = 0;

    if (SFile::FileExistsEx((std::string(WEAPON_FOLDER) + fileName).c_str(), 0)) {
        found |= FOUND_WEAPON;
    }

    if (SFile::FileExistsEx((std::string(SHIELD_FOLDER) + fileName).c_str(), 0)) {
        found |= FOUND_SHIELD;
    }

    // Misses are not cached so a patch that adds the file later is picked up.
    if (found) {
        s_found.emplace(fileName, found);
    }

    return found;
}

bool UsesShieldFolder(const ItemDisplayInfoRec* rec, bool preferShield) {
    if (!rec || !rec->m_modelName[0] || !*rec->m_modelName[0]) {
        return preferShield;
    }

    uint8_t found = ProbeFolders(ModelFileName(rec->m_modelName[0]));

    if (found == FOUND_WEAPON) {
        return false;
    }

    if (found == FOUND_SHIELD) {
        return true;
    }

    return preferShield;
}

bool IsShieldModel(const CGUnit_C* unit, uint32_t hand, const CGUnitVirtualItem* item) {
    if (hand == 2 || !item) {
        return false;
    }

    ItemDisplayInfoRec rec;
    const ItemDisplayInfoRec* found = unit->GetVirtualItemDisplayRec(static_cast<VIRTUAL_MONSTER_SLOT>(hand), rec) ? &rec : nullptr;

    return UsesShieldFolder(found, item->m_inventoryType == INVTYPE_SHIELD);
}

static bool AttachmentBindPosition(CM2Model* model, uint32_t attachment, C3Vector& out) {
    if (!model || !(model->m_fFlags & 1) || !model->m_shared || !model->m_shared->m_data) {
        return false;
    }

    const M2Data* data = model->m_shared->m_data;

    if (attachment >= data->m_attachmentIndicesById.m_count) {
        return false;
    }

    uint16_t index = data->m_attachmentIndicesById[attachment];

    if (index == 0xFFFF || index >= data->m_attachments.m_count) {
        return false;
    }

    out = data->m_attachments[index].m_position;

    return true;
}

static void MainHandShieldOffset(CM2Model* parent, float out[3]) {
    C3Vector shield;
    C3Vector hand;

    if (AttachmentBindPosition(parent, ATTACH_SHIELD, shield) && AttachmentBindPosition(parent, ATTACH_HANDLEFT, hand)) {
        out[0] = shield.x - hand.x;
        out[1] = hand.y - shield.y;
        out[2] = shield.z - hand.z;
        return;
    }

    out[0] = DEFAULT_SHIELD_OFFSET[0];
    out[1] = DEFAULT_SHIELD_OFFSET[1];
    out[2] = DEFAULT_SHIELD_OFFSET[2];
}

void SetMainHandShieldTransform(CM2Model* child, CM2Model* parent, bool applied) {
    float mirror = applied ? -1.0f : 1.0f;
    C44Matrix& matrix = child->m_worldTransform;
    matrix = C44Matrix(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, mirror, 0.0f, 0.0f, 0.0f, 0.0f, mirror, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    if (!applied) {
        return;
    }

    float offset[3];
    MainHandShieldOffset(parent, offset);
    matrix.d0 = offset[0];
    matrix.d1 = offset[1];
    matrix.d2 = offset[2];
}

void MirrorMainHandShield(CM2Model* parent) {
    for (CM2Model* child = parent->m_attachList; child; child = child->m_attachNext) {
        if (child->m_attachmentId == static_cast<uint32_t>(ATTACH_HANDRIGHT)) {
            SetMainHandShieldTransform(child, parent, true);
            return;
        }
    }
}

}
