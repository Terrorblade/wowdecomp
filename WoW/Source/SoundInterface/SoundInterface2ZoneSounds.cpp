#include <cstddef>
#include <cstdint>
#include <new>
#include "WoW/Source/SoundInterface/SoundInterface2ZoneSounds.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRandom.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Game/GameTime.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "WoW/Source/Ui/GameUI.h"

// STATIC: 0x00AC3878
static AMBIENCE s_zoneSoundTimeOfDay = AMB_NIGHT;

// STATIC: 0x00B4AAB0
static SI2ZoneSoundInfo s_zoneSounds[SI2::NUM_SOURCE_SLOTS];

// STATIC: 0x00B4AC3C
static SE2SoundKitObject s_scriptMusic;

// STATIC: 0x00B4AC50
static SI2ZoneSoundPair s_zoneAmbience;

// STATIC: 0x00B4AC90
static SI2ZoneSoundPair s_zoneMusic;

// STATIC: 0x00B4ACD0
TSGrowableArray<uint32_t> SI2::sm_aIntroMusicNextValidPlayTime;

// STATIC: 0x00AC3864
static int32_t s_overrideChunkX = -1;

// STATIC: 0x00AC3868
static int32_t s_overrideMapID = -1;

// STATIC: 0x00AC386C
static int32_t s_overrideChunkY = -1;

// STATIC: 0x00AC3870
static int32_t s_overrideSubchunkY = -1;

// STATIC: 0x00AC3874
static int32_t s_overrideSubchunkX = -1;

// INLINED
SI2ZoneSoundInfo::SI2ZoneSoundInfo() {
    this->m_music[AMB_DAY] = -1;
    this->m_music[AMB_NIGHT] = -1;
    this->m_silenceMin[AMB_DAY] = 0;
    this->m_silenceMin[AMB_NIGHT] = 0;
    this->m_silenceMax[AMB_DAY] = 0;
    this->m_silenceMax[AMB_NIGHT] = 0;
    this->m_ambience[AMB_DAY] = -1;
    this->m_ambience[AMB_NIGHT] = -1;
    this->m_reverb = -1;
}

// INLINED
SI2ZoneSoundInfo::~SI2ZoneSoundInfo() {
}

// VERIFY: e6578ba5 manual
// OFFSET: 0x004C8590
int32_t SI2::GetZoneReverb() {
    for (int32_t i = SI2::SOURCE_OVERRIDE; i >= 0; i--) {
        if (s_zoneSounds[i].m_reverb < 0) {
            continue;
        }

        if (i == SI2::SOURCE_UNDERWATER) {
            if (!SI2::sm_bCameraIsUnderwater) {
                continue;
            }
        } else if (i == SI2::SOURCE_WEATHER && SI2::sm_bPlayerInside) {
            continue;
        }

        return s_zoneSounds[i].m_reverb;
    }

    return 0;
}

// VERIFY: 368f23da auto
// OFFSET: 0x004C85F0
void SI2::StopZoneAmbience() {
    s_zoneAmbience.Stop(1);
}

// VERIFY: 481f5672 auto
// OFFSET: 0x004C8600
void SI2::SetPlayerInside(bool inside) {
    SI2::sm_bPlayerInside = inside;
}

// VERIFY: b619fbdd auto
// OFFSET: 0x004C8610
void SI2::SetWeatherKitID(int32_t soundKitId) {
    s_zoneSounds[SI2::SOURCE_WEATHER].m_ambience[AMB_DAY] = soundKitId;
    s_zoneSounds[SI2::SOURCE_WEATHER].m_ambience[AMB_NIGHT] = soundKitId;
}

// VERIFY: 92518c26 auto
// OFFSET: 0x004C8630
void SI2::SetUnderwaterStatus(int32_t underwater) {
    int32_t unchanged = underwater == SI2::sm_bCameraIsUnderwater;
    SI2::sm_bCameraIsUnderwater = underwater;
    s_zoneSounds[SI2::SOURCE_UNDERWATER].m_ambience[AMB_DAY] = underwater ? 4209 : -1;
    s_zoneSounds[SI2::SOURCE_UNDERWATER].m_ambience[AMB_NIGHT] = s_zoneSounds[SI2::SOURCE_UNDERWATER].m_ambience[AMB_DAY];
    SI2::sm_bUnderwaterChanged = !unchanged;
    SI2::UpdateReverb();
}

// VERIFY: 8cc2bd07 auto
// OFFSET: 0x004C8670
int32_t SI2::IsUnderwater() {
    return SI2::sm_bCameraIsUnderwater;
}

// VERIFY: 3dfce48c auto
// OFFSET: 0x004C8680
void SI2::StopLuaMusic() {
    SI2::StopSound(&s_scriptMusic, 0, 0.5f, 1);
    SI2::sm_bLuaMusicPlaying = 0;
}

// VERIFY: 527bb7b0 auto
// OFFSET: 0x004C86B0
void SI2::SetZoneReverb(int32_t type, int32_t reverb) {
    s_zoneSounds[type].m_reverb = reverb;
    SI2::ApplyReverbSettings();
}

// VERIFY: 53308ceb auto
// OFFSET: 0x004C86D0
void SI2::SetZoneUnderwaterReverb(int32_t reverb) {
    s_zoneSounds[SI2::SOURCE_UNDERWATER].m_reverb = reverb;
    SI2::ApplyReverbSettings();
}

// VERIFY: a9775166 auto
// OFFSET: 0x004C86F0
int32_t SI2::GetDesiredAmbienceKitID(int32_t* type) {
    static CVar* s_enableAllSound = CVar::Lookup("Sound_EnableAllSound");
    static CVar* s_enableAmbience = CVar::Lookup("Sound_EnableAmbience");

    if ((s_enableAllSound && !s_enableAllSound->m_intValue) || (s_enableAmbience && !s_enableAmbience->m_intValue) || CGGameUI::m_inMovie) {
        if (type) {
            *type = SI2::SOURCE_DISABLED;
        }

        return 0;
    }

    for (int32_t i = SI2::SOURCE_OVERRIDE; i >= 0; i--) {
        if (s_zoneSounds[i].m_ambience[s_zoneSoundTimeOfDay] < 0 || (i == SI2::SOURCE_WEATHER && SI2::sm_bPlayerInside)) {
            continue;
        }

        if (type) {
            *type = i;
        }

        return s_zoneSounds[i].m_ambience[s_zoneSoundTimeOfDay];
    }

    return 0;
}

// VERIFY: 40642125 auto
// OFFSET: 0x004C87D0
int32_t SI2::ZoneMusicDelayElapsed(int32_t type) {
    if (type && type != SI2::SOURCE_CHUNK_OVERRIDE && type != SI2::SOURCE_WORLD_STATE) {
        return 1;
    }

    static CVar* s_zoneMusicNoDelay = CVar::Lookup("Sound_ZoneMusicNoDelay");

    return (s_zoneMusicNoDelay && s_zoneMusicNoDelay->m_intValue) || OsGetAsyncTimeMs() >= SI2::sm_nextZoneMusicTime;
}

// VERIFY: 97d2bfe2 auto
// OFFSET: 0x004C8830
int32_t SI2::IntroMusicOkToPlay(int32_t soundKitId, int32_t type) {
    if (soundKitId == s_zoneMusic.GetPlayingSoundKitId() || (type != SI2::SI2ZONESOUNDTYPE_INTRO && type != SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE && type != SI2::SI2ZONESOUNDTYPE_INTRO_WORLDSTATEOVERRIDE)) {
        return 1;
    }

    if (s_zoneMusic.GetCurrentType() == SI2::SI2ZONESOUNDTYPE_INTRO) {
        return 0;
    }

    if (s_zoneMusic.GetCurrentType() == SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE) {
        return 0;
    }

    if (s_zoneMusic.GetCurrentType() == SI2::SI2ZONESOUNDTYPE_INTRO_WORLDSTATEOVERRIDE) {
        return 0;
    }

    uint32_t now = OsGetAsyncTimeMs();

    if (static_cast<uint32_t>(soundKitId) >= SI2::sm_aIntroMusicNextValidPlayTime.Count()) {
        return 0;
    }

    return now >= SI2::sm_aIntroMusicNextValidPlayTime[soundKitId];
}

// VERIFY: 17672894 auto
// OFFSET: 0x004C88B0
int32_t SI2::GetDesiredMusicKitID(int32_t* type, int32_t* silenceMin, int32_t* silenceMax) {
    static CVar* s_enableAllSound = CVar::Lookup("Sound_EnableAllSound");
    static CVar* s_enableMusic = CVar::Lookup("Sound_EnableMusic");

    if ((s_enableAllSound && !s_enableAllSound->m_intValue) || (s_enableMusic && !s_enableMusic->m_intValue) || CGGameUI::m_inMovie) {
        if (type) {
            *type = SI2::SOURCE_DISABLED;
        }

        if (silenceMin) {
            *silenceMin = 0;
        }

        if (silenceMax) {
            *silenceMax = 0;
        }

        return 0;
    }

    for (int32_t i = SI2::SOURCE_OVERRIDE; i >= 0; i--) {
        if ((i == SI2::SI2ZONESOUNDTYPE_INTRO || i == SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE || i == SI2::SI2ZONESOUNDTYPE_INTRO_WORLDSTATEOVERRIDE) && s_zoneMusic.IsPlaying() && (s_zoneMusic.GetCurrentType() == SI2::SI2ZONESOUNDTYPE_INTRO || s_zoneMusic.GetCurrentType() == SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE || s_zoneMusic.GetCurrentType() == SI2::SI2ZONESOUNDTYPE_INTRO_WORLDSTATEOVERRIDE)) {
            if (i == SI2::SOURCE_WEATHER && SI2::sm_bPlayerInside) {
                continue;
            }

            if (type) {
                *type = s_zoneMusic.GetCurrentType();
            }

            if (silenceMin) {
                *silenceMin = 0;
            }

            if (silenceMax) {
                *silenceMax = 0;
            }

            return s_zoneMusic.GetPlayingSoundKitId();
        }

        if (s_zoneSounds[i].m_music[s_zoneSoundTimeOfDay] < 0) {
            continue;
        }

        if (!SI2::ZoneMusicDelayElapsed(i)) {
            if (type) {
                *type = SI2::SOURCE_MUSIC_DELAY;
            }

            if (silenceMin) {
                *silenceMin = 0;
            }

            if (silenceMax) {
                *silenceMax = 0;
            }

            return -1111;
        }

        if (!SI2::IntroMusicOkToPlay(s_zoneSounds[i].m_music[s_zoneSoundTimeOfDay], i)) {
            continue;
        }

        if (type) {
            *type = i;
        }

        if (silenceMin) {
            *silenceMin = s_zoneSounds[i].m_silenceMin[s_zoneSoundTimeOfDay];
        }

        if (silenceMax) {
            *silenceMax = s_zoneSounds[i].m_silenceMax[s_zoneSoundTimeOfDay];
        }

        return s_zoneSounds[i].m_music[s_zoneSoundTimeOfDay];
    }

    return 0;
}

// VERIFY: e4136c86 manual
// OFFSET: 0x004C8AB0
void SI2::ZoneMusicFinished(SESound* sound, void* param) {
    SI2ZoneMusicTimer* timer = static_cast<SI2ZoneMusicTimer*>(param);
    uint32_t now = OsGetAsyncTimeMs();

    if (SI2::GetDesiredMusicKitID(nullptr, nullptr, nullptr) == timer->m_soundKitId) {
        uint32_t silenceMax = timer->m_silenceMax;
        uint32_t silenceMin = timer->m_silenceMin;

        if (silenceMax > silenceMin) {
            static CRndSeed s_seed(0);
            SI2::sm_nextZoneMusicTime = now + silenceMin + static_cast<uint32_t>((static_cast<uint64_t>(silenceMax - silenceMin) * CRandom::uint32_(s_seed)) >> 32);
        } else {
            SI2::sm_nextZoneMusicTime = now + silenceMax;
        }
    } else {
        SI2::sm_nextZoneMusicTime = 0;
    }

    delete timer;
}

// VERIFY: 4b8e2a85 manual
// OFFSET: 0x004C8C90
void SI2::SetZoneAmbienceID(int32_t type, int32_t soundAmbienceId) {
    SoundAmbienceRec* rec = g_soundAmbienceDB.GetRecord(soundAmbienceId);

    if (rec) {
        s_zoneSounds[type].m_ambience[AMB_DAY] = rec->m_ambienceID[AMB_DAY];
        s_zoneSounds[type].m_ambience[AMB_NIGHT] = rec->m_ambienceID[AMB_NIGHT];
    } else {
        s_zoneSounds[type].m_ambience[AMB_DAY] = -1;
        s_zoneSounds[type].m_ambience[AMB_NIGHT] = -1;
    }
}

// VERIFY: 436a05e0 manual
// OFFSET: 0x004C8CF0
void SI2::SetZoneIntroMusicID(int32_t type, int32_t zoneIntroMusicId) {
    ZoneIntroMusicTableRec* rec = g_zoneIntroMusicTableDB.GetRecord(zoneIntroMusicId);

    if (rec) {
        s_zoneSounds[type].m_music[AMB_DAY] = rec->m_soundID;
        s_zoneSounds[type].m_music[AMB_NIGHT] = rec->m_soundID;
        s_zoneSounds[type].m_silenceMin[AMB_DAY] = rec->m_minDelayMinutes;
        s_zoneSounds[type].m_silenceMin[AMB_NIGHT] = rec->m_minDelayMinutes;
    } else {
        s_zoneSounds[type].m_music[AMB_DAY] = -1;
        s_zoneSounds[type].m_music[AMB_NIGHT] = -1;
        s_zoneSounds[type].m_silenceMin[AMB_DAY] = 0;
        s_zoneSounds[type].m_silenceMin[AMB_NIGHT] = 0;
        s_zoneSounds[type].m_silenceMax[AMB_DAY] = 0;
        s_zoneSounds[type].m_silenceMax[AMB_NIGHT] = 0;
    }
}

// VERIFY: 8c58a8a2 manual
// OFFSET: 0x004C8D80
void SI2::SetZoneMusicID(int32_t type, int32_t zoneMusicId) {
    ZoneMusicRec* rec = g_zoneMusicDB.GetRecord(zoneMusicId);

    if (!rec) {
        s_zoneSounds[type].m_music[AMB_DAY] = -1;
        s_zoneSounds[type].m_music[AMB_NIGHT] = -1;
        s_zoneSounds[type].m_silenceMin[AMB_DAY] = 0;
        s_zoneSounds[type].m_silenceMin[AMB_NIGHT] = 0;
        s_zoneSounds[type].m_silenceMax[AMB_DAY] = 0;
        s_zoneSounds[type].m_silenceMax[AMB_NIGHT] = 0;

        if (zoneMusicId > 0) {
            SI2::sm_nextZoneMusicTime = 0;
        }

        return;
    }

    s_zoneSounds[type].m_music[AMB_DAY] = rec->m_sounds[AMB_DAY];
    s_zoneSounds[type].m_music[AMB_NIGHT] = rec->m_sounds[AMB_NIGHT];
    s_zoneSounds[type].m_silenceMin[AMB_DAY] = rec->m_silenceIntervalMin[AMB_DAY];
    s_zoneSounds[type].m_silenceMin[AMB_NIGHT] = rec->m_silenceIntervalMin[AMB_NIGHT];
    s_zoneSounds[type].m_silenceMax[AMB_DAY] = rec->m_silenceIntervalMax[AMB_DAY];
    s_zoneSounds[type].m_silenceMax[AMB_NIGHT] = rec->m_silenceIntervalMax[AMB_NIGHT];

    int32_t desired = SI2::GetDesiredMusicKitID(nullptr, nullptr, nullptr);

    if (desired != s_zoneMusic.GetPlayingSoundKitId() && (type > SI2::SOURCE_CHUNK_OVERRIDE || desired != -1111)) {
        SI2::sm_nextZoneMusicTime = 0;
    }
}

// VERIFY: c27c3281 manual
// OFFSET: 0x004C8FA0
void SI2::SetOverrideZoneSounds(int32_t soundAmbienceId, int32_t zoneMusicId) {
    SI2::SetZoneAmbienceID(SI2::SOURCE_OVERRIDE, soundAmbienceId);
    SI2::SetZoneMusicID(SI2::SOURCE_OVERRIDE, zoneMusicId);
}

// VERIFY: 039f2ed0 auto
// OFFSET: 0x004C8FC0
int32_t SI2::PlayUISound(int32_t soundKitId) {
    static int32_t s_voiceChatOn = SI2::LookupSoundKitId("VoiceChatOn");
    static int32_t s_voiceChatOff = SI2::LookupSoundKitId("VoiceChatOff");

    SE2SoundKitProperties properties;
    properties.ResetToDefaults();

    if (soundKitId == s_voiceChatOn || soundKitId == s_voiceChatOff) {
        properties.m_group = SI2_SOUND_GROUP_CINEMATIC;
    }

    properties.m_priority = SE2_PRIORITY_DEFAULT;
    properties.m_suppressorMode = SE2_SUPPRESSOR_MODE_HONOR;
    properties.m_reverb = 0;

    return SI2::PlaySoundKit(soundKitId, nullptr, nullptr, &properties, 0, nullptr, 1, 0);
}

// VERIFY: cf1057e2 manual
// OFFSET: 0x004C9080
void SI2::IntroMusicFinished(SESound* sound, void* param) {
    SI2ZoneMusicTimer* timer = static_cast<SI2ZoneMusicTimer*>(param);
    int32_t minutes = timer->m_silenceMin;
    uint32_t soundKitId = timer->m_soundKitId;
    uint32_t now = OsGetAsyncTimeMs();

    if (soundKitId < SI2::sm_aIntroMusicNextValidPlayTime.Count()) {
        SI2::sm_aIntroMusicNextValidPlayTime[soundKitId] = now + 60000 * minutes;
    }

    delete timer;
}

// VERIFY: a0e10314 manual
// OFFSET: 0x004C90C0
void SI2::AIMusicFinished(SESound* sound, void* param) {
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_reverb = -1;

    if (param) {
        delete static_cast<SI2ZoneMusicTimer*>(param);
    }
}

// VERIFY: 26708b3b manual
// OFFSET: 0x004C9110
int32_t SI2::PlaySoundFile(const char* filename, int32_t group) {
    static CVar* s_enableAllSound = CVar::Lookup("Sound_EnableAllSound");

    if (!s_enableAllSound || !s_enableAllSound->m_intValue || CGGameUI::m_inMovie) {
        return SI2_ERR_SOUND_DISABLED;
    }

    if (group == SI2_SOUND_GROUP_SCRIPT_MUSIC) {
        static CVar* s_enableMusic = CVar::Lookup("Sound_EnableMusic");

        if (!s_enableMusic || !s_enableMusic->m_intValue) {
            return SI2_ERR_MUSIC_DISABLED;
        }
    }

    SESound localSound;
    SESound* sound = group == SI2_SOUND_GROUP_SCRIPT_MUSIC ? &s_scriptMusic.m_sound : &localSound;

    sound->Create2D(filename, group != SI2_SOUND_GROUP_SCRIPT_SOUND, nullptr, nullptr, false, false, 0, group != SI2_SOUND_GROUP_SCRIPT_MUSIC ? SE2_PRIORITY_DEFAULT : SI2_PRIORITY_ZONE, 0xFFFFFFFF);
    sound->SetChannelGroup(SI2::s_channelGroupNames[group], true);

    SI2SoundUserData* userData = new (SMemAlloc(sizeof(SI2SoundUserData), __FILE__, __LINE__, 0)) SI2SoundUserData();

    if (userData) {
        userData->m_name = group != SI2_SOUND_GROUP_SCRIPT_SOUND ? "<SCRIPT MUSIC>" : "<SCRIPT SOUND>";
        userData->m_soundKitId = 0;
        sound->SetUserData(userData);
    }

    sound->SetVolume(1.0f);
    sound->CompleteNonBlockingLoad();
    sound->Play();

    if (sound->IsPlaying() && group == SI2_SOUND_GROUP_SCRIPT_MUSIC) {
        SI2::sm_bLuaMusicPlaying = 1;
        s_zoneMusic.Stop(0);
    }

    return SI2_OK;
}

// VERIFY: 14578d5d auto
// OFFSET: 0x004C92B0
void SI2::PlayAIMusic(int32_t soundKitId) {
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_reverb = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_DAY] = soundKitId;
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_NIGHT] = soundKitId;
}

// VERIFY: 759593c5 auto
// OFFSET: 0x004C92F0
void SI2::StopAIMusic() {
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_reverb = -1;
}

// VERIFY: 373e56ae manual
// OFFSET: 0x004C9330
void SI2::InitZoneIntros() {
    uint32_t count = g_soundEntriesDB.m_maxID + 1;
    SI2::sm_aIntroMusicNextValidPlayTime.SetCount(count);

    for (uint32_t i = 0; i < count; i++) {
        SI2::sm_aIntroMusicNextValidPlayTime[i] = 0;
    }
}

// VERIFY: 65b668bf auto
// OFFSET: 0x004C93B0
void SI2::StopAllMusic() {
    s_zoneMusic.Stop(1);
    SI2::StopSound(&s_scriptMusic, 0, 0.5f, 1);
    SI2::sm_bLuaMusicPlaying = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMin[AMB_NIGHT] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_DAY] = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_silenceMax[AMB_NIGHT] = 0;
    SI2::sm_nextZoneMusicTime = 0;
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_music[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_DAY] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_ambience[AMB_NIGHT] = -1;
    s_zoneSounds[SI2::SOURCE_AI].m_reverb = -1;
}

// VERIFY: 7a4f127b auto
// OFFSET: 0x004C9420
void SI2::FreeZoneIntros() {
    SI2::sm_aIntroMusicNextValidPlayTime.Clear();
}

// VERIFY: c8ca0148 manual
// OFFSET: 0x004C9460
SI2ZoneSoundPair::SI2ZoneSoundPair() {
    this->m_current = 0;
    this->m_next = 1;
}

// VERIFY: 28bfbf51 manual
// OFFSET: 0x004C94B0
SI2ZoneSoundPair::~SI2ZoneSoundPair() {
}

// INLINED
SI2ZoneSound::SI2ZoneSound() {
    this->m_soundKitId = 0;
    this->m_type = SI2::SOURCE_NONE;
}

// VERIFY: d03f2e4b manual
// OFFSET: 0x004C9910
int32_t SI2::ZoneSoundsUpdate(const void* data, void* param) {
    AMBIENCE timeOfDay = g_clientGameTime.IsDaytime() ? AMB_DAY : AMB_NIGHT;

    if (s_zoneSoundTimeOfDay != timeOfDay) {
        s_zoneSoundTimeOfDay = timeOfDay;
    }

    SE2SoundKitProperties properties;
    int32_t type = SI2::SOURCE_NONE;
    int32_t ambienceKitId = SI2::GetDesiredAmbienceKitID(&type);

    if (!s_zoneAmbience.IsPlaying() || ambienceKitId != s_zoneAmbience.GetPlayingSoundKitId()) {
        properties.ResetToDefaults();
        properties.m_group = SI2_SOUND_GROUP_AMBIENCE;
        properties.m_loopMode = SE2_LOOP_MODE_LOOPING;
        properties.m_priority = SI2_PRIORITY_ZONE;
        properties.m_reverb = 0;

        if (SFile::IsStreamingMode()) {
            properties.m_checkStreaming = 1;
        }

        int32_t immediate = 0;

        if (SI2::sm_bUnderwaterChanged) {
            immediate = 1;
            properties.m_fadeInTime = 0.0f;
        } else {
            properties.m_fadeInTime = 5.0f;
        }

        s_zoneAmbience.Transition(ambienceKitId, type, &properties, immediate, 5.0f);
    }

    type = SI2::SOURCE_NONE;
    int32_t silenceMin = 0;
    int32_t silenceMax = 0;
    int32_t musicKitId = SI2::GetDesiredMusicKitID(&type, &silenceMin, &silenceMax);

    if (SI2::sm_bLuaMusicPlaying) {
        return 1;
    }

    if (s_zoneMusic.IsPlaying() && s_zoneMusic.GetPlayingSoundKitId() == musicKitId) {
        return 1;
    }

    if (musicKitId == -1111 || CGGameUI::CameraMusicIsPlaying() || CGGameUI::SequenceMusicIsPlaying()) {
        return 1;
    }

    properties.ResetToDefaults();
    properties.m_group = SI2_SOUND_GROUP_MUSIC;
    properties.m_selectMode = SE2_FILE_SELECT_WEIGHTED_CYCLE;
    properties.m_fadeInTime = s_zoneMusic.IsPlaying() ? 4.0f : 0.01f;
    properties.m_fadeOutTime = 4.0f;
    properties.m_priority = SI2_PRIORITY_ZONE;
    properties.m_reverb = 0;

    if (SFile::IsStreamingMode()) {
        properties.m_checkStreaming = 1;
    }

    if (s_zoneMusic.Transition(musicKitId, type, &properties, 0, -1.0f)) {
        return 1;
    }

    SI2ZoneMusicTimer* timer = static_cast<SI2ZoneMusicTimer*>(SMemAlloc(sizeof(SI2ZoneMusicTimer), __FILE__, __LINE__, 0));
    timer->m_soundKitId = musicKitId;

    switch (type) {
        case SI2::SOURCE_AREA:
        case SI2::SOURCE_WORLD_STATE:
        case SI2::SOURCE_CHUNK_OVERRIDE:
            timer->m_silenceMax = silenceMax;
            timer->m_silenceMin = silenceMin;
            s_zoneMusic.SetFinishedCallback(SI2::ZoneMusicFinished, timer);
            break;

        case SI2::SOURCE_AI:
            timer->m_silenceMax = silenceMax;
            timer->m_silenceMin = silenceMin;
            s_zoneMusic.SetFinishedCallback(SI2::AIMusicFinished, timer);
            break;

        case SI2::SI2ZONESOUNDTYPE_INTRO:
        case SI2::SI2ZONESOUNDTYPE_INTRO_WORLDSTATEOVERRIDE:
        case SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE:
            timer->m_silenceMin = silenceMin;
            timer->m_silenceMax = 0;
            s_zoneMusic.SetFinishedCallback(SI2::IntroMusicFinished, timer);
            break;

        default:
            delete timer;
            break;
    }

    return 1;
}

// VERIFY: cf894244 auto
// OFFSET: 0x004C9BD0
void SI2::InitZoneSoundsHandler() {
    EventRegister(EVENT_ID_IDLE, SI2::ZoneSoundsUpdate);
    EventRegister(EVENT_ID_IDLE, SI2::HandleWaterSounds);
}

// VERIFY: 3ebdde4f manual
// OFFSET: 0x004C9BF0
void SI2::ShutdownZoneSoundsHandler() {
    EventUnregister(EVENT_ID_IDLE, SI2::ZoneSoundsUpdate);
    s_zoneAmbience.Stop(1);
    SI2::StopAllMusic();
    SI2::WaterShutdown();
    SE2::SetReverb(nullptr);
}

// VERIFY: a49403c9 manual
// OFFSET: 0x004C68E0
void SI2::OverrideZoneSounds() {
    int32_t mapId;
    int32_t chunkX;
    int32_t chunkY;
    int32_t subchunkX;
    int32_t subchunkY;

    if (!SI2::GetChunkLocation(&mapId, &chunkX, &chunkY, &subchunkX, &subchunkY)) {
        return;
    }

    int32_t lastChunkY = chunkY;
    int32_t lastSubchunkX = subchunkX;
    int32_t lastSubchunkY = subchunkY;

    if (subchunkX != s_overrideSubchunkX || subchunkY != s_overrideSubchunkY) {
        int32_t zoneMusicId;

        if (SI2::GetZoneSoundOverride(mapId, chunkX, chunkY, subchunkX, subchunkY, &subchunkX, &zoneMusicId, &chunkY, &subchunkY)) {
            if (subchunkY >= 0) {
                SI2::SetZoneReverb(SI2::SOURCE_CHUNK_OVERRIDE, subchunkY);
            }

            if (chunkY >= 0) {
                SI2::SetZoneAmbienceID(SI2::SOURCE_CHUNK_OVERRIDE, chunkY);
            }

            if (subchunkX >= 0) {
                SI2::SetZoneIntroMusicID(SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE, subchunkX);
            }

            if (zoneMusicId >= 0) {
                SI2::SetZoneMusicID(SI2::SOURCE_CHUNK_OVERRIDE, zoneMusicId);
            }
        } else {
            SI2::SetZoneReverb(SI2::SOURCE_CHUNK_OVERRIDE, -1);
            SI2::SetZoneAmbienceID(SI2::SOURCE_CHUNK_OVERRIDE, -1);
            SI2::SetZoneIntroMusicID(SI2::SI2ZONESOUNDTYPE_INTRO_OVERRIDE, -1);
            SI2::SetZoneMusicID(SI2::SOURCE_CHUNK_OVERRIDE, -1);
        }
    }

    s_overrideSubchunkY = lastSubchunkY;
    s_overrideSubchunkX = lastSubchunkX;
    s_overrideChunkY = lastChunkY;
    s_overrideMapID = mapId;
    s_overrideChunkX = chunkX;
}
