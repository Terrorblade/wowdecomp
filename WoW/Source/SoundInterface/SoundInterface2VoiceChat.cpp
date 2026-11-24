#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include "WoW/Source/SoundInterface/SoundInterface2VoiceChat.h"
#include "Engine/Source/Base/CmdLine.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "WoW/Source/Client.h"

// STATIC: 0x00B2D9E4
static SI2LoopbackCapture s_loopbackCapture;

// STATIC: 0x00DCEAA4
static int32_t s_microphoneSignalLevel;

// STATIC: 0x00DCEAA8
static uint8_t* s_loopbackBuffer;

// STATIC: 0x00DCEAAC
static uint8_t* s_loopbackWrite;

// STATIC: 0x00DCEAB0
static uint32_t s_loopbackSize;

// STATIC: 0x00DCEAB4
static SE2SoundKitObject s_loopbackSound;

// VERIFY: 5c71868e auto
// OFFSET: 0x00986390
void SI2::VoiceChat_ShutDownCapture() {
    SE2::ShutDownCapture();
}

// VERIFY: 92720538 manual
// OFFSET: 0x009863A0
void SI2::VoiceChat_CleanUpLoopbackSound() {
    SI2::StopSound(&s_loopbackSound, 1, -1.0f, 1);

    if (s_loopbackBuffer) {
        SMemFree(s_loopbackBuffer, __FILE__, __LINE__, 0);
        s_loopbackBuffer = nullptr;
        s_loopbackSize = 0;
    }
}

// VERIFY: 313d6efe auto
// OFFSET: 0x009863F0
void SI2::VoiceChat_StopPlayingLoopbackSound() {
    SI2::StopSound(&s_loopbackSound, 1, -1.0f, 1);
}

// VERIFY: a69f50c4 auto
// OFFSET: 0x00986410
bool SI2::VoiceChat_IsPlayingLoopbackSound() {
    return SI2::IsPlaying(&s_loopbackSound);
}

// VERIFY: 992daf2d manual
// OFFSET: 0x00986420
void SI2::VoiceChat_SetCaptureDevice(int32_t index) {
    static CVar* s_inputDriverIndex = CVar::Lookup("Sound_VoiceChatInputDriverIndex");

    if (s_inputDriverIndex) {
        char value[32];
        SStrPrintf(value, sizeof(value), "%d", index);
        s_inputDriverIndex->Set(value, true, false, false, true);
    }
}

// VERIFY: f9cebc61 manual
// OFFSET: 0x00986490
void SI2::VoiceChat_SetOutputDevice(int32_t index) {
    static CVar* s_outputDriverIndex = CVar::Lookup("Sound_VoiceChatOutputDriverIndex");

    if (s_outputDriverIndex) {
        char value[32];
        SStrPrintf(value, sizeof(value), "%d", index);
        s_outputDriverIndex->Set(value, true, false, false, true);
    }
}

// VERIFY: c0b92063 auto
// OFFSET: 0x00986500
void SI2::VoiceChat_DestroyChatSystem() {
    SE2::DestroyChatSystem();
}

// VERIFY: b52bb4f6 auto
// OFFSET: 0x00986510
int32_t SI2::VoiceChat_IsVoiceChatInitialized() {
    return SE2::IsChatSoundInitialized();
}

// VERIFY: 2b6fed5c auto
// OFFSET: 0x00986520
void SI2::VoiceChat_AmplifyMicrophoneInput(int16_t* data, uint32_t samples) {
    if (!SE2::IsChatSoundInitialized()) {
        return;
    }

    static CVar* s_outboundChatVolume = CVar::Lookup("OutboundChatVolume");

    if (!s_outboundChatVolume) {
        return;
    }

    float volume = s_outboundChatVolume->m_floatValue;

    if (1.0f == volume) {
        return;
    }

    for (uint32_t i = 0; i < samples; i++) {
        int32_t value = static_cast<int32_t>(data[i] * volume);

        if (value < -32768) {
            value = -32768;
        } else if (value > 32767) {
            value = 32767;
        }

        data[i] = static_cast<int16_t>(value);
    }
}

// VERIFY: d56d0331 manual
// OFFSET: 0x009865C0
static void ComputeAverageLevel(float* data, uint32_t count) {
    if (!SE2::IsChatSoundInitialized()) {
        return;
    }

    float sum = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        sum += fabs(data[i]);
    }

    s_microphoneSignalLevel = static_cast<int32_t>(sqrt(sum / count) * 100.0f);
}

// VERIFY: 0f8f8550 auto
// OFFSET: 0x00986670
int32_t SI2::VoiceChat_GetCurrentMicrophoneSignalLevel() {
    return SE2::IsChatSoundInitialized() != 0 ? s_microphoneSignalLevel : 0;
}

// VERIFY: 124ffd5f auto
// OFFSET: 0x00986680
int32_t SI2::VoiceChat_CreateChatSound(SE2SoundKitObject* object, SEStreamCallbacks* callbacks, int32_t chatSystem) {
    if (!SE2::IsChatSoundInitialized()) {
        return SI2_ERR_SOUND_DISABLED;
    }

    object->m_sound.CreateChatSound(callbacks, chatSystem);
    return SI2_OK;
}

// VERIFY: 73c312f2 auto
// OFFSET: 0x009866B0
int32_t SI2::VoiceChat_Play(SE2SoundKitObject* object) {
    if (!SE2::IsChatSoundInitialized()) {
        return SI2_ERR_SOUND_DISABLED;
    }

    object->m_sound.Play();
    return SI2_OK;
}

// VERIFY: eec71328 auto
// OFFSET: 0x009866E0
static void MonitorCallback(float* data, uint32_t count) {
    if (SE2::IsChatSoundInitialized()) {
        ComputeAverageLevel(data, count);
    }
}

// VERIFY: e6a66d96 auto
// OFFSET: 0x00986700
void SI2::VoiceChat_InitCapture(float sampleRate) {
    if (CmdLineGetBool(static_cast<CMDOPT>(CMD_NO_SOUND))) {
        return;
    }

    int32_t outputDriverIndex = 0;
    static CVar* s_outputDriverIndex = CVar::Lookup("Sound_VoiceChatOutputDriverIndex");

    if (s_outputDriverIndex) {
        outputDriverIndex = s_outputDriverIndex->m_intValue;
    }

    const char* outputDriverName = "";
    static CVar* s_outputDriverName = CVar::Lookup("Sound_VoiceChatOutputDriverName");

    if (s_outputDriverName) {
        outputDriverName = s_outputDriverName->m_stringValue.m_str;
    }

    int32_t inputDriverIndex = 0;
    static CVar* s_inputDriverIndex = CVar::Lookup("Sound_VoiceChatInputDriverIndex");

    if (s_inputDriverIndex) {
        inputDriverIndex = s_inputDriverIndex->m_intValue;
    }

    const char* inputDriverName = "";
    static CVar* s_inputDriverName = CVar::Lookup("Sound_VoiceChatInputDriverName");

    if (s_inputDriverName) {
        inputDriverName = s_inputDriverName->m_stringValue.m_str;
    }

    SE2::InitCapture(&outputDriverIndex, outputDriverName, &inputDriverIndex, inputDriverName, sampleRate);

    char value[32];

    if (s_outputDriverIndex) {
        SStrPrintf(value, sizeof(value), "%d", outputDriverIndex);
        s_outputDriverIndex->Set(value, true, false, false, true);
    }

    if (s_inputDriverName) {
        SStrPrintf(value, sizeof(value), "%d", inputDriverIndex);
        s_inputDriverIndex->Set(value, true, false, false, true);
    }

    SE2::SetUserMonitorDSPCallback(MonitorCallback);
}

// VERIFY: d0c59c8d auto
// OFFSET: 0x009868A0
void SI2::VoiceChat_SetPrimaryCaptureCallback(SECaptureCallbacks* callbacks) {
    if (SE2::IsChatSoundInitialized()) {
        SE2::SetPrimaryCaptureCallback(callbacks);
    }
}

// VERIFY: 57b84183 manual
// OFFSET: 0x009868C0
void SI2::VoiceChat_StartCapture() {
    if (SE2::IsChatSoundInitialized()) {
        SE2::StartCapture();
    }
}

// VERIFY: 77221e3b manual
// OFFSET: 0x009868D0
void SI2::VoiceChat_StopCapture() {
    if (SE2::IsChatSoundInitialized()) {
        SE2::StopCapture();
    }
}

// VERIFY: 67c97745 manual
// OFFSET: 0x009868E0
static void ComputeMicrophoneLevel(int16_t* data, uint32_t samples) {
    if (!SE2::IsChatSoundInitialized()) {
        return;
    }

    uint32_t sum = 0;

    for (uint32_t i = 0; i < samples; i++) {
        sum += abs(data[i]);
    }

    s_microphoneSignalLevel = static_cast<int32_t>(sqrt(static_cast<float>(sum / samples) * 0.000030517578f) * 100.0f);
}

// VERIFY: a6e75762 manual
// OFFSET: 0x00986960
SI2LoopbackCapture::~SI2LoopbackCapture() {
}

// VERIFY: 9fa225c6 auto
// OFFSET: 0x00986990
bool SI2::VoiceChat_IsRecordingLoopbackSound() {
    if (!SE2::IsChatSoundInitialized()) {
        return false;
    }

    return SE2::IsCapturing() && SE2::AlternateCaptureCallbackIsActive();
}

// VERIFY: eec9034f manual
// OFFSET: 0x009869C0
void SI2::VoiceChat_ActivatePrimaryCaptureCallback() {
    if (SE2::IsChatSoundInitialized()) {
        SE2::ActivatePrimaryCaptureCallback();
    }
}

// VERIFY: 887239cf auto
// OFFSET: 0x009869D0
void SI2::VoiceChat_StopRecordingLoopbackSound() {
    if (SE2::IsChatSoundInitialized()) {
        SI2::VoiceChat_ActivatePrimaryCaptureCallback();
    }
}

// VERIFY: 075fee4d manual
// OFFSET: 0x009869F0
void SI2LoopbackCapture::ProcessData(int16_t* data, uint32_t samples) {
    if (!s_loopbackBuffer) {
        return;
    }

    SI2::VoiceChat_AmplifyMicrophoneInput(data, samples);
    ComputeMicrophoneLevel(data, samples);

    uint8_t* write = s_loopbackWrite;
    uint32_t remaining = static_cast<uint32_t>(s_loopbackBuffer + s_loopbackSize - write);
    uint32_t bytes = 2 * samples;

    if (bytes >= remaining) {
        bytes = remaining;
    }

    memcpy(write, data, 2 * (bytes >> 1));
    s_loopbackWrite = write + bytes;

    if (!remaining && SE2::IsChatSoundInitialized()) {
        SI2::VoiceChat_ActivatePrimaryCaptureCallback();
    }
}

// VERIFY: 0dc037b8 auto
// OFFSET: 0x00986A70
static int32_t DetectRecordingFinished(const void* data, void* param) {
    if (!SE2::IsChatSoundInitialized() || !SE2::IsCapturing() || !SE2::AlternateCaptureCallbackIsActive()) {
        EventUnregister(EVENT_ID_POLL, DetectRecordingFinished);
    }

    return 1;
}

// VERIFY: 0741bfdd manual
// OFFSET: 0x00986AA0
void SI2::VoiceChat_RecordLoopbackSound(int32_t seconds) {
    if (!SE2::IsChatSoundInitialized()) {
        return;
    }

    SI2::VoiceChat_CleanUpLoopbackSound();

    s_loopbackSize = 16000 * seconds + 44;

    uint32_t* header = static_cast<uint32_t*>(SMemAlloc(16000 * seconds + 44, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY));
    header[0] = 'FFIR';
    header[1] = 16000 * seconds + 32;
    header[2] = 'EVAW';
    header[3] = ' tmf';
    header[4] = 16;
    header[5] = 0x10001;
    s_loopbackBuffer = reinterpret_cast<uint8_t*>(header);
    header[6] = 8000;
    header[7] = 16000;
    header[8] = 0x100002;
    header[9] = 'atad';
    header[10] = 16000 * seconds;
    s_loopbackWrite = reinterpret_cast<uint8_t*>(&header[11]);

    SE2::SetAlternateCaptureCallback(&s_loopbackCapture);
    SE2::ActivateAlternateCaptureCallback();
    EventRegister(EVENT_ID_POLL, DetectRecordingFinished);
}

// VERIFY: ef5b70e1 manual
// OFFSET: 0x00986BC0
void SI2::VoiceChat_PlayLoopbackSound() {
    if (!SE2::IsChatSoundInitialized() || !s_loopbackBuffer) {
        return;
    }

    s_loopbackSound.m_sound.PlaySoundFromMemory(reinterpret_cast<const char*>(s_loopbackBuffer), s_loopbackSize, 1);

    SI2SoundUserData* userData = new (SMemAlloc(sizeof(SI2SoundUserData), __FILE__, __LINE__, 0)) SI2SoundUserData();
    userData->m_name = "<Voice Chat Loopback Sound>";
    userData->m_soundKitId = 0;
    userData->m_def = nullptr;
    userData->m_group = SI2_SOUND_GROUP_CINEMATIC;

    s_loopbackSound.m_sound.SetUserData(userData);
    s_loopbackSound.m_sound.SetFinishedCallback(SI2::SoundFinished, nullptr);
    s_loopbackSound.m_sound.SetChannelGroup(SI2::s_channelGroupNames[userData->m_group], userData->m_group != SI2_SOUND_GROUP_CINEMATIC);
    SE2::AttachMonitorDSP(&s_loopbackSound.m_sound);
    s_loopbackSound.m_sound.Play();
}
