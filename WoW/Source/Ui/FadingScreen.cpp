#include "WoW/Source/Ui/GameUI.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Scrn/Scrn.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gxu/Gxu.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CMath.h"
#include "Engine/Source/Tempest/Color.h"

// STATIC: 0x00AD1450
static const uint16_t s_fadeIndices[4] = { 0, 1, 2, 3 };

// STATIC: 0x00C24274
static CTexture* s_fadeTexture;

// STATIC: 0x00C24278
static void* s_fadeParam;

// STATIC: 0x00C2427C
static void (*s_fadeCallback)(void* param);

// STATIC: 0x00C24280
static uint8_t s_fadeFinished;

// STATIC: 0x00C24284
static uint32_t s_fadeStartTime;

// STATIC: 0x00C24288
static float s_fadeDuration;

// STATIC: 0x00C2428C
static int32_t s_fadeDirection;

// STATIC: 0x00C24290
static HLAYER s_fadeLayer;

// STATIC: 0x00C24294
static uint8_t s_fadeActive;

// STATIC: 0x00C24295
static uint8_t s_fadeStarted;

// VERIFY: 089a3404 auto
// OFFSET: 0x008E5250
static int32_t SwallowEvent(const void* data, void* param) {
    return 0;
}

// VERIFY: 49ca7f4d auto
// OFFSET: 0x005ECF60
static void RegisterHandlers() {
    EventRegisterEx(EVENT_ID_CHAR, SwallowEvent, nullptr, 8.0f);
    EventRegisterEx(EVENT_ID_KEYDOWN, SwallowEvent, nullptr, 8.0f);
    EventRegisterEx(EVENT_ID_KEYUP, SwallowEvent, nullptr, 8.0f);
    EventRegisterEx(EVENT_ID_KEYDOWN_REPEATING, SwallowEvent, nullptr, 8.0f);
    EventRegisterEx(EVENT_ID_MOUSEDOWN, SwallowEvent, nullptr, 8.0f);
    EventRegisterEx(EVENT_ID_MOUSEUP, SwallowEvent, nullptr, 8.0f);
    EventRegisterEx(EVENT_ID_MOUSEMOVE, SwallowEvent, nullptr, 8.0f);
}

// VERIFY: 55878560 auto
// OFFSET: 0x005ED020
static void FadingScreenUnregisterEvents() {
    EventUnregister(EVENT_ID_CHAR, SwallowEvent);
    EventUnregister(EVENT_ID_KEYDOWN, SwallowEvent);
    EventUnregister(EVENT_ID_KEYDOWN_REPEATING, SwallowEvent);
    EventUnregister(EVENT_ID_KEYUP, SwallowEvent);
    EventUnregister(EVENT_ID_MOUSEDOWN, SwallowEvent);
    EventUnregister(EVENT_ID_MOUSEUP, SwallowEvent);
    EventUnregister(EVENT_ID_MOUSEMOVE, SwallowEvent);
}

// VERIFY: 80851c1e auto
// OFFSET: 0x005ED080
void DisableFadingScreen(float duration, void (*callback)(void*), void* param) {
    if (!s_fadeActive) {
        return;
    }

    s_fadeDuration = duration;
    s_fadeCallback = callback;
    s_fadeStarted = 0;
    s_fadeDirection = 1;
    s_fadeFinished = 0;
    s_fadeParam = param;

    if (duration != 0.0f) {
        return;
    }

    HandleClose(reinterpret_cast<HOBJECT>(s_fadeLayer));
    HandleClose(reinterpret_cast<HOBJECT>(s_fadeTexture));
    FadingScreenUnregisterEvents();
    s_fadeActive = 0;
    s_fadeStarted = 0;
    s_fadeDirection = 0;

    if (s_fadeCallback) {
        s_fadeCallback(s_fadeParam);
    }
}

// VERIFY: f82642c3 manual
// OFFSET: 0x005ED120
static void FadingScreenPaint(void* param, const RECTF* rect, const RECTF* visible, float elapsedSec) {
    static C3Vector s_positions[4] = { C3Vector(0.0f, 0.0f, 0.0f), C3Vector(1.0f, 0.0f, 0.0f), C3Vector(0.0f, 1.0f, 0.0f), C3Vector(1.0f, 1.0f, 0.0f) };
    static C2Vector s_texCoords[4] = { C2Vector(0.0f, 1.0f), C2Vector(1.0f, 1.0f), C2Vector(0.0f, 0.0f), C2Vector(1.0f, 0.0f) };
    static C3Vector s_normal = C3Vector(0.0f, 0.0f, 1.0f);
    uint8_t finished = 0;
    CImVector color = { COLOR_WHITE_BGRA };

    if (s_fadeDirection) {
        float elapsed;

        if (s_fadeStarted) {
            elapsed = static_cast<float>(OsGetAsyncTimeMs() - s_fadeStartTime) * 0.001f;
        } else {
            elapsed = 0.0f;
            s_fadeStartTime = OsGetAsyncTimeMs();
        }

        if (s_fadeDuration > elapsed) {
            uint8_t alpha = CMath::fint_(elapsed / s_fadeDuration * 255.0f);
            color.a = s_fadeDirection == 2 ? alpha : 0xFF - alpha;
        } else {
            finished = 1;
            color.a = s_fadeDirection == 2 ? 0xFF : 0x00;
        }
    }

    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);
    GxUpdateAlphaRef(GxRs_AlphaRef);
    GxPrimLockVertexPtrs(4, s_positions, sizeof(C3Vector), &s_normal, 0, &color, 0, nullptr, 0, s_texCoords, sizeof(C2Vector), nullptr, 0);
    g_theGxDevicePtr->RsSet(GxRs_Texture0, TextureGetGxTex(s_fadeTexture, 1, nullptr));
    GxDrawLockedElements(GxPrim_TriangleStrip, 4, s_fadeIndices);
    GxPrimUnlockVertexPtrs();
    g_theGxDevicePtr->RsPop();
    s_fadeStarted = 1;

    if (s_fadeFinished) {
        if (s_fadeDirection == 1) {
            HandleClose(reinterpret_cast<HOBJECT>(s_fadeLayer));
            HandleClose(reinterpret_cast<HOBJECT>(s_fadeTexture));
            FadingScreenUnregisterEvents();
            s_fadeActive = 0;
            s_fadeStarted = 0;
        }

        s_fadeDirection = 0;
        s_fadeFinished = 0;

        if (s_fadeCallback) {
            s_fadeCallback(s_fadeParam);
        }
    } else if (finished) {
        s_fadeFinished = 1;
    }
}

// VERIFY: 2c6b34d8 manual
// OFFSET: 0x005ED480
void EnableFadingScreen(float duration, void (*callback)(void*), void* param) {
    if (s_fadeActive) {
        HandleClose(reinterpret_cast<HOBJECT>(s_fadeLayer));
        HandleClose(reinterpret_cast<HOBJECT>(s_fadeTexture));
        FadingScreenUnregisterEvents();
        s_fadeActive = 0;
        s_fadeStarted = 0;
    }

    s_fadeTexture = TextureCreate(8, 8, GxTex_Argb8888, GxTex_Argb8888, CGxTexFlags(GxTex_Linear, 0, 0, 0, 0, 0, 1), reinterpret_cast<void*>(static_cast<uintptr_t>(0xFF000000)), GxuUpdateSingleColorTexture, nullptr, 0);
    RECTF rect = { 0.0f, 0.0f, 1.0f, 1.0f };
    ScrnLayerCreate(&rect, 10.0f, duration != 0.0f ? SCRN_LAYER_TRANSPARENT | SCRN_LAYER_ORTHO | SCRN_LAYER_FULLSCREEN_VIEWPORT : SCRN_LAYER_ORTHO | SCRN_LAYER_FULLSCREEN_VIEWPORT, nullptr, FadingScreenPaint, &s_fadeLayer);
    RegisterHandlers();
    s_fadeDuration = duration;
    s_fadeStarted = 0;
    s_fadeFinished = 0;
    s_fadeActive = 1;
    s_fadeDirection = 2;
    s_fadeCallback = callback;
    s_fadeParam = param;
}
