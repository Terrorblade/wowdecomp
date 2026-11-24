#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/WMOAreaTableRec.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/WorldClient/AreaList.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapEntity.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// STATIC: 0x00ADF318
static int32_t s_lastAreaID = -1;

// STATIC: 0x00ADF31C
static int32_t s_lastSubAreaID = -1;

// STATIC: 0x00ADF320
static int32_t s_lastMapID = -1;

// STATIC: 0x00ADF324
static int32_t s_lastWMOID = -1;

// STATIC: 0x00ADF328
static int32_t s_lastWMOHash = -1;

// STATIC: 0x00ADF32C
static int32_t s_lastWMOGroupNum = -1;

// STATIC: 0x00ADF330
static int32_t s_wmoGroupID = -1;

// STATIC: 0x00CD85EC
static int32_t s_inMapObj;

// VERIFY: 47f43aed auto
// OFFSET: 0x0078ED80
int32_t AreaListGetName(uint32_t areaId, char* buffer, uint32_t size, int32_t includeParent) {
    if (!buffer || !size) {
        return 0;
    }

    *buffer = '\0';
    AreaTableRec* area = g_areaTableDB.GetRecord(areaId);

    if (!area) {
        return 0;
    }

    AreaTableRec* parent;

    if (includeParent && area->m_parentAreaID && (parent = g_areaTableDB.GetRecord(area->m_parentAreaID)) != nullptr) {
        SStrPrintf(buffer, size, "%s, %s", area->m_areaName, parent->m_areaName);
        return 1;
    }

    SStrCopy(buffer, area->m_areaName, size);
    return 1;
}

// VERIFY: 374cc580 manual
// OFFSET: 0x0078EE30
static void AreaListSendZoneUpdate(int32_t zoneId) {
    if (!zoneId) {
        return;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_ZONEUPDATE));
    msg.Put(static_cast<uint32_t>(zoneId));
    msg.Finalize();
    ClientServices::Send2(&msg);
}

// VERIFY: 0b647302 manual
// OFFSET: 0x0078E9A0
void AreaListUpdateSounds(const AreaTableRec* zone, const AreaTableRec* area, const WMOAreaTableRec* rootRec, const WMOAreaTableRec* groupRec, int32_t inside) {
    int32_t introSound;
    int32_t zoneMusic;
    int32_t ambience;
    int32_t reverb;
    int32_t underwater;

    if (inside) {
        introSound = groupRec && groupRec->m_introSound ? groupRec->m_introSound : (rootRec ? rootRec->m_introSound : 0);
        zoneMusic = groupRec && groupRec->m_zoneMusic ? groupRec->m_zoneMusic : (rootRec ? rootRec->m_zoneMusic : 0);
        ambience = groupRec && groupRec->m_ambienceID ? groupRec->m_ambienceID : (rootRec ? rootRec->m_ambienceID : 0);
        reverb = groupRec && groupRec->m_soundProviderPref ? groupRec->m_soundProviderPref : (rootRec ? rootRec->m_soundProviderPref : 0);
        underwater = groupRec && groupRec->m_soundProviderPrefUnderwater ? groupRec->m_soundProviderPrefUnderwater : (rootRec ? rootRec->m_soundProviderPrefUnderwater : 0);
    } else {
        if ((groupRec && groupRec->m_introSound) || (rootRec && rootRec->m_introSound)) {
            introSound = groupRec && groupRec->m_introSound ? groupRec->m_introSound : (rootRec ? rootRec->m_introSound : 0);
        } else {
            introSound = area && area->m_introSound ? area->m_introSound : (zone ? zone->m_introSound : 0);
        }

        if ((groupRec && groupRec->m_zoneMusic) || (rootRec && rootRec->m_zoneMusic)) {
            zoneMusic = groupRec && groupRec->m_zoneMusic ? groupRec->m_zoneMusic : (rootRec ? rootRec->m_zoneMusic : 0);
        } else {
            zoneMusic = area && area->m_zoneMusic ? area->m_zoneMusic : (zone ? zone->m_zoneMusic : 0);
        }

        if ((groupRec && groupRec->m_ambienceID) || (rootRec && rootRec->m_ambienceID)) {
            ambience = groupRec && groupRec->m_ambienceID ? groupRec->m_ambienceID : (rootRec ? rootRec->m_ambienceID : 0);
        } else {
            ambience = area && area->m_ambienceID ? area->m_ambienceID : (zone ? zone->m_ambienceID : 0);
        }

        if ((groupRec && groupRec->m_soundProviderPref) || (rootRec && rootRec->m_soundProviderPref)) {
            reverb = groupRec && groupRec->m_soundProviderPref ? groupRec->m_soundProviderPref : (rootRec ? rootRec->m_soundProviderPref : 0);
        } else {
            reverb = area && area->m_soundProviderPref ? area->m_soundProviderPref : (zone ? zone->m_soundProviderPref : 0);
        }

        if ((groupRec && groupRec->m_soundProviderPrefUnderwater) || (rootRec && rootRec->m_soundProviderPrefUnderwater)) {
            underwater = groupRec && groupRec->m_soundProviderPrefUnderwater ? groupRec->m_soundProviderPrefUnderwater : (rootRec ? rootRec->m_soundProviderPrefUnderwater : 0);
        } else {
            underwater = area && area->m_soundProviderPrefUnderwater ? area->m_soundProviderPrefUnderwater : (zone ? zone->m_soundProviderPrefUnderwater : 0);
        }
    }

    SI2::UpdateZoneSoundsForWorldState(zone ? zone->m_id : 0, area ? area->m_id : 0, rootRec ? rootRec->m_id : 0, groupRec ? groupRec->m_id : 0, inside);
    SI2::SetZoneReverb(SI2::SOURCE_AREA, reverb);
    SI2::SetZoneUnderwaterReverb(underwater);
    SI2::SetZoneAmbienceID(SI2::SOURCE_AREA, ambience);
    SI2::SetZoneIntroMusicID(SI2::SI2ZONESOUNDTYPE_INTRO, introSound);
    SI2::SetZoneMusicID(SI2::SOURCE_AREA, zoneMusic);
    SI2::SetPlayerInside(inside != 0);
}

// VERIFY: 2d5d59f9 manual
// OFFSET: 0x0078EC70
static bool AreaListUpdateCurrentZone(CMapEntity* entity, const char** zoneName, const char** subzoneName, int32_t wmoId, int32_t wmoHash, int32_t wmoGroupNum, int32_t areaId, const AreaTableRec* zone, const AreaTableRec* area) {
    const char* wmoSubzoneName = nullptr;
    const char* wmoZoneName = nullptr;
    int32_t hasZoneName = World::QueryMapObjZoneName(entity, &wmoZoneName);
    int32_t hasSubzoneName = World::QueryMapObjSubzoneName(entity, &wmoSubzoneName, &areaId);

    if (areaId == s_lastAreaID && wmoId == s_lastWMOID && wmoHash == s_lastWMOHash && wmoGroupNum == s_lastWMOGroupNum) {
        return false;
    }

    s_lastAreaID = areaId;
    s_lastWMOID = wmoId;
    s_lastWMOHash = wmoHash;
    s_lastWMOGroupNum = wmoGroupNum;

    if (hasZoneName && wmoZoneName && *wmoZoneName) {
        if (!*subzoneName || SStrCmpUTF8I(wmoZoneName, *subzoneName, STORM_MAX_STR)) {
            *zoneName = wmoZoneName;
            *subzoneName = nullptr;
        }
    }

    if (hasSubzoneName && wmoSubzoneName && *wmoSubzoneName) {
        *subzoneName = wmoSubzoneName;
    }

    WMOAreaTableRec* groupRec = nullptr;
    WMOAreaTableRec* rootRec = nullptr;
    int32_t inside = World::QueryObjectInside(entity);
    World::QueryMapObjAreaTable(entity, &groupRec, &rootRec, &s_wmoGroupID);
    AreaListUpdateSounds(zone, area, rootRec, groupRec, inside);
    return true;
}

// VERIFY: d03ecf6d manual
// OFFSET: 0x0078EEB0
static bool AreaListUpdateReverbForLocation(int32_t mapId, int32_t areaId, int32_t zoneId) {
    if (!s_inMapObj && zoneId == s_lastAreaID && areaId == s_lastSubAreaID && mapId == s_lastMapID) {
        return false;
    }

    s_lastMapID = mapId;
    s_lastAreaID = zoneId;
    s_lastSubAreaID = areaId;
    s_wmoGroupID = 0;
    AreaTableRec* area = g_areaTableDB.GetRecord(areaId);
    AreaTableRec* zone = g_areaTableDB.GetRecord(zoneId);
    int32_t introSound = area && area->m_introSound ? area->m_introSound : (zone ? zone->m_introSound : 0);
    int32_t zoneMusic = area && area->m_zoneMusic ? area->m_zoneMusic : (zone ? zone->m_zoneMusic : 0);
    int32_t ambience = area && area->m_ambienceID ? area->m_ambienceID : (zone ? zone->m_ambienceID : 0);
    int32_t reverb = area && area->m_soundProviderPref ? area->m_soundProviderPref : (zone ? zone->m_soundProviderPref : 0);
    int32_t underwater = area && area->m_soundProviderPrefUnderwater ? area->m_soundProviderPrefUnderwater : (zone ? zone->m_soundProviderPrefUnderwater : 0);
    SI2::UpdateZoneSoundsForWorldState(zone ? zone->m_id : 0, area ? area->m_id : 0, 0, 0, 0);
    SI2::SetZoneReverb(SI2::SOURCE_AREA, reverb);
    SI2::SetZoneUnderwaterReverb(underwater);
    SI2::SetZoneAmbienceID(SI2::SOURCE_AREA, ambience);
    SI2::SetZoneIntroMusicID(SI2::SI2ZONESOUNDTYPE_INTRO, introSound);
    SI2::SetZoneMusicID(SI2::SOURCE_AREA, zoneMusic);
    SI2::SetPlayerInside(false);
    AreaListSendZoneUpdate(zoneId);
    return true;
}

// VERIFY: 64227ed1 manual
// OFFSET: 0x0078F020
void AreaListRegisterLocation(int32_t mapId, CMapEntity* entity) {
    if (!entity) {
        return;
    }

    int32_t areaId = 0;

    if (!World::QueryAreaIdAtPosition(&entity->m_position, &areaId)) {
        return;
    }

    AreaTableRec* area = g_areaTableDB.GetRecord(areaId);

    if (!area) {
        return;
    }

    AreaTableRec* zone;
    int32_t zoneId;

    zone = area->m_parentAreaID ? g_areaTableDB.GetRecord(area->m_parentAreaID) : nullptr;

    if (zone) {
        zoneId = zone->m_id;
    } else {
        zone = area;
        zoneId = areaId;
        areaId = 0;
        area = nullptr;
    }

    if (!zoneId) {
        return;
    }

    const char* subzoneName = area ? area->m_areaName : nullptr;
    const char* realZoneName = zone->m_areaName;
    const char* zoneName = realZoneName;
    int32_t wmoId = -1;
    int32_t wmoHash = -1;
    int32_t wmoGroupNum = -1;
    int32_t inMapObj = World::QueryMapObjIDs(entity, &wmoId, &wmoHash, &wmoGroupNum);
    int32_t changed = inMapObj != s_inMapObj;
    s_inMapObj = inMapObj;
    bool updated;

    if (!inMapObj && !changed) {
        updated = AreaListUpdateReverbForLocation(mapId, area ? area->m_id : 0, zone->m_id);
    } else {
        updated = AreaListUpdateCurrentZone(entity, &zoneName, &subzoneName, wmoId, wmoHash, wmoGroupNum, areaId ? areaId : zoneId, zone, area);
    }

    SI2::OverrideZoneSounds();

    if (!updated) {
        return;
    }

    CGGameUI::SetMinimapZoneText(subzoneName ? subzoneName : zoneName);
    const char* realZoneText = realZoneName && *realZoneName ? realZoneName : nullptr;
    const char* subZoneText = subzoneName && *subzoneName ? subzoneName : nullptr;
    const char* zoneText = zoneName && *zoneName ? zoneName : nullptr;
    int32_t subzoneId = area ? area->m_id : 0;
    CGGameUI::NewZoneFeedback(zone->m_id, subzoneId, s_wmoGroupID, zoneText, subZoneText, realZoneText, World::QueryObjectInside(entity));
}

// VERIFY: cc53ae16 auto
// OFFSET: 0x0078E960
void AreaListShutdown() {
    s_lastAreaID = -1;
    s_lastSubAreaID = -1;
    s_lastMapID = -1;
    s_inMapObj = 0;
    s_lastWMOID = -1;
    s_lastWMOHash = -1;
    s_lastWMOGroupNum = -1;
    s_wmoGroupID = -1;
}

// VERIFY: f319121e manual
// OFFSET: 0x0078F1F0
bool AreaListZoneHasBreathParticles(CMapEntity* entity) {
    AreaTableRec* area;

    if (!World::QueryObjectInside(entity)) {
        uint32_t areaId = 0;

        if (!CMap::QueryAreaId(entity, areaId)) {
            return false;
        }

        area = g_areaTableDB.GetRecord(areaId);

        if (!area) {
            return false;
        }
    } else {
        WMOAreaTableRec* groupRec = nullptr;
        WMOAreaTableRec* rootRec = nullptr;
        int32_t groupId = 0;

        if (!World::QueryMapObjAreaTable(entity, &groupRec, &rootRec, &groupId)) {
            return false;
        }

        area = groupRec ? g_areaTableDB.GetRecord(groupRec->m_areaTableID) : nullptr;

        if (!area) {
            if (!rootRec) {
                return false;
            }

            area = g_areaTableDB.GetRecord(rootRec->m_areaTableID);

            if (!area) {
                return false;
            }
        }
    }

    AreaTableRec* parent = g_areaTableDB.GetRecord(area->m_parentAreaID);
    int32_t flags = area->m_flags;

    if (!(flags & AREA_FLAG_BREATH_PARTICLES_OVERRIDE_PARENT) && parent) {
        flags = parent->m_flags;
    }

    return flags & AREA_FLAG_EMIT_BREATH_PARTICLES;
}
