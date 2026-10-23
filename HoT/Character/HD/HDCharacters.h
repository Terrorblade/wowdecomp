#ifndef HOT_CHARACTER_HD_HDCHARACTERS_H
#define HOT_CHARACTER_HD_HDCHARACTERS_H

#include <cstdint>
#include "WoW/Common/ComponentCore/Types.h"

class CCharacterComponent;
class CGUnit_C;
class CM2Model;
class ComponentData;
class CreatureDisplayInfoRec;
class CreatureModelDataRec;

namespace HoT {
class Module;
}

// HD character models from CharacterHD\, dressed by DBFilesClient\HDCharacterManifest.dbc while the HD patch is on.
namespace HoT::HD {

static const int32_t RACE_COUNT = 12;

bool IsEnabled();

// Turning it on only sticks while the HD art is readable.
void SetEnabled(bool enabled);

bool IsAvailable();
bool RefreshAvailability();

// Follows the launcher's HD patch setting against the art on disk, for SetHDPatchEnabled and every streamed batch.
void RefreshPatch();

// Drops every cached answer about which HD art exists, including the remembered misses.
void InvalidateArt();

bool FileExists(const char* path);

// Checks the .blp a texture path loads as before the path itself, like TextureCreate does.
bool TextureExists(const char* path);
const char* RaceFolder(int32_t raceId);
const char* SexFolder(int32_t sexId);
const char* ModelStem(const CM2Model* model);
bool IsHdModel(const CM2Model* model);
float BakedScale(int32_t race, int32_t sex);

// CGUnit_C::GetModelFileName, the HD model for a stock character model when the unit renders HD.
const char* UnitModelPath(const CGUnit_C* unit, const char* stockPath);

// CGUnit_C::GetModelScale, divides out the scale an HD model already has baked in.
float UnitModelScale(const CGUnit_C* unit, const CreatureDisplayInfoRec* displayInfo, const CreatureModelDataRec* modelData, float scale);

// FillInPartialObjectData, the HDDisplaySwap replacement for a unit's display or native display field, value for any other field.
uint32_t SwapDisplayField(const CGUnit_C* unit, uint32_t unitBlock, uint32_t value);

// Before CCharacterComponent::Init on baked NPC data, unit may be null. Uses the HD bake or drops the bake so the sheet is composited.
void ApplyNpcBake(const CGUnit_C* unit, ComponentData* data);

// Glue screens build their character from this model path.
const char* GlueModelPath(const char* stockPath);

// Scale for the glue character model's world transform, 1 for anything but an HD character.
float GlueScale(const CM2Model* model);

// First thing in CCharacterComponent::Init, marks the component HD from its model.
void OnComponentInit(CCharacterComponent* component, const ComponentData* data);

// Before every return of CCharacterComponent::Init.
void OnComponentInitDone(CCharacterComponent* component);

// End of the CCharacterComponent destructor.
void OnComponentDestroyed(CCharacterComponent* component);

// Texture path resolution for the stock path in s_path, returns stockPath unless an HD component has an HD texture for it.
const char* BaseVariationTexture(const CCharacterComponent* component, COMPONENT_VARIATIONS variation, int32_t textureIndex, int32_t variationIndex, int32_t colorIndex, const char* stockPath);
const char* NamedTexture(const CCharacterComponent* component, const char* stockPath);
const char* HairTexture(const CCharacterComponent* component, int32_t hairStyleID, const char* stockPath);
const char* ExtraSkinTexture(const CCharacterComponent* component, const char* stockPath);

// First in CCharacterComponent::ReplaceExtraSkinTexture, true when the manifest skin extra texture was applied.
bool ReplaceExtraSkinTexture(CCharacterComponent* component);

// Texture slots 11 to 13 from the manifest.
void ApplySlots(CCharacterComponent* component);

// End of CCharacterComponent::GeosRenderPrep.
void ApplyGeosets(CCharacterComponent* component);

// Lives for the whole of SetHairStyle and SetBeardStyle and applies the manifest customization geosets on the way out.
class CustomizationScope {
    public:
    explicit CustomizationScope(CCharacterComponent* component) : m_component(component) {}
    ~CustomizationScope();

    CustomizationScope(const CustomizationScope&) = delete;
    CustomizationScope& operator=(const CustomizationScope&) = delete;

    private:
    CCharacterComponent* m_component;
};

// Lives for the whole of CCharacterComponent::AddHelm and fixes the HD geosets and helm placement on the way out.
class HelmScope {
    public:
    explicit HelmScope(CCharacterComponent* component);
    ~HelmScope();

    HelmScope(const HelmScope&) = delete;
    HelmScope& operator=(const HelmScope&) = delete;

    private:
    static const int32_t GROUP_COUNT = 7;

    CCharacterComponent* m_component;
    int32_t m_before[GROUP_COUNT];
};

// CCharacterComponent::AddLink, the HD helm model next to the stock one when it exists. buffer holds 260 chars.
const char* HelmModelPath(const CM2Model* parent, GEOCOMPONENTLINKS link, const char* modelPath, char* buffer);

// CM2Scene::DuplicateModel, keeps the placement of an HD helm on the copy.
void CopyHelmPlacement(CM2Model* copy, const CM2Model* source);

// Measured per helm fit, the DLL keeps it but no longer calls it.
void ApplyHelmFit(CCharacterComponent* component);

// The HD 1024x512 sheet replaces CreateBaseTexture, PrepSections and CreateComponentRequest, true when the component is HD and was handled.
bool CreateBaseTexture(CCharacterComponent* component);
bool PrepSections(CCharacterComponent* component);
bool RequestSheet(CCharacterComponent* component);

// Applies the HD patch setting, builds the manifest and hooks the streaming refresh.
Module* GetModule();

}

#endif
