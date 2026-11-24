#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "HoT/Profiler/Profiler.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/Gxu.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Images/PngWriter.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Scrn/Layer.h"
#include "Engine/Source/Scrn/Scrn.h"
#include "Engine/Source/SoundEngine/SoundEngine.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C44Matrix.h"

// STATIC: 0x009EDCB8
static const RECTF s_defaultRect = { 0.0f, 0.0f, 1.0f, 1.0f };

// STATIC: 0x00AC1B9C
static TSExplicitList<CILayer, offsetof(CILayer, m_zorderlink)> s_zorderlist;

// STATIC: 0x00B49A10
static float s_elapsedSec;

// STATIC: 0x00B49A14
static void (*s_screenshotCallback)(int32_t success);

// STATIC: 0x00B49A18
static int32_t s_noPresent;

static std::thread* s_screenshotWriter;

static void WaitScreenshotWriter() {
    if (s_screenshotWriter) {
        s_screenshotWriter->join();
        delete s_screenshotWriter;
        s_screenshotWriter = nullptr;
    }
}

// VERIFY: cbcfc981 auto
// OFFSET: 0x004A8460
static int32_t OnIdle(const void* data, void* param) {
    s_elapsedSec += static_cast<const EVENT_DATA_IDLE*>(data)->m_elapsedSec;

    return 1;
}

// VERIFY: 8f3185ff auto
// OFFSET: 0x004A8500
void ScrnLayerSetRect(HLAYER layer, const RECTF* rect) {
    reinterpret_cast<CILayer*>(layer)->m_rect = *rect;
}

// VERIFY: e30052be auto
// OFFSET: 0x004A8530
uint32_t ScrnLayerGetFlags(HLAYER layer) {
    return reinterpret_cast<CILayer*>(layer)->m_flags;
}

// VERIFY: 4573bf37 auto
// OFFSET: 0x004A8540
void ScrnLayerSetFlags(HLAYER layer, uint32_t flags) {
    reinterpret_cast<CILayer*>(layer)->m_flags = flags;
}

// VERIFY: 2c71a8fb auto
// OFFSET: 0x004A8550
void ScrnScreenshot(void (*callback)(int32_t success)) {
    s_screenshotCallback = callback;
}

// VERIFY: 90b31936 auto
// OFFSET: 0x004A8570
static int32_t CaptureScreen() {
    uint32_t width;
    uint32_t height;
    const CImVector* pixels;

    g_theGxDevicePtr->DeviceReadScreenShot(width, height, pixels);

    if (pixels) {
        OsCreateDirectory("Screenshots", 0);

        char timeStamp[260];
        OsGetTimeStamp(timeStamp, sizeof(timeStamp));

        char fileName[260];
        SStrPrintf(fileName, sizeof(fileName), "%s\\WoWScrnShot_%s.png", "Screenshots", timeStamp);

        WaitScreenshotWriter();

        std::string path = fileName;
        std::vector<CImVector> image(pixels, pixels + static_cast<size_t>(width) * height);
        s_screenshotWriter = new std::thread([path = std::move(path), image = std::move(image), width, height]() {
            PngWriteBgra(path.c_str(), image.data(), width, height);
        });
    }

    return pixels != nullptr;
}

// VERIFY: 52c6418c manual
// OFFSET: 0x004A8720
static int32_t OnPaint(const void* data, void* param) {
    if (GxHasContext() && GxIsWindowVisible()) {
        HSRGN rgn;
        SRgnCreate(&rgn, 0);

        RECTF screen = { 0.0f, 0.0f, 1.0f, 1.0f };
        SRgnCombineRectf(rgn, &screen, nullptr, 2);

        for (CILayer* layer = s_zorderlist.Tail(); layer; layer = s_zorderlist.Prev(layer)) {
            SRgnGetBoundingRectf(rgn, &layer->m_visible);

            layer->m_visible.left = layer->m_rect.left >= layer->m_visible.left ? layer->m_rect.left : layer->m_visible.left;
            layer->m_visible.bottom = layer->m_rect.bottom >= layer->m_visible.bottom ? layer->m_rect.bottom : layer->m_visible.bottom;
            layer->m_visible.right = layer->m_rect.right <= layer->m_visible.right ? layer->m_rect.right : layer->m_visible.right;
            layer->m_visible.top = layer->m_rect.top <= layer->m_visible.top ? layer->m_rect.top : layer->m_visible.top;

            if (!(layer->m_flags & SCRN_LAYER_TRANSPARENT)) {
                SRgnCombineRectf(rgn, &layer->m_rect, nullptr, 4);
            }
        }

        SRgnDelete(rgn);

        float minX;
        float maxX;
        float minY;
        float maxY;
        float minZ;
        float maxZ;
        GxXformViewport(minX, maxX, minY, maxY, minZ, maxZ);

        CILayer* next;

        for (CILayer* layer = s_zorderlist.Head(); !s_zorderlist.IsEnd(layer); layer = next) {
            next = s_zorderlist.RawNext(layer);

            if (layer->m_visible.left < layer->m_visible.right && layer->m_visible.bottom < layer->m_visible.top) {
                if (layer->m_flags & SCRN_LAYER_FULLSCREEN_VIEWPORT) {
                    GxXformSetViewport(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
                } else {
                    GxXformSetViewport(layer->m_visible.left, layer->m_visible.right, layer->m_visible.bottom, layer->m_visible.top, 0.0f, 1.0f);
                }

                if (layer->m_flags & SCRN_LAYER_ORTHO) {
                    C44Matrix view;
                    C44Matrix proj;
                    GxuXformCreateOrtho(layer->m_visible.left, layer->m_visible.right, layer->m_visible.bottom, layer->m_visible.top, 0.0f, 500.0f, proj);
                    GxXformSetView(view);
                    GxXformSetProjection(proj);
                }

                layer->m_paintFunc(layer->m_param, &layer->m_rect, &layer->m_visible, s_elapsedSec);
            }
        }

        GxXformSetViewport(minX, maxX, minY, maxY, minZ, maxZ);

        {
            HoT::Profiler::Scope profilerScope(HoT::Profiler::Zone::FontUpdate);
            GxuFontUpdate();
        }

        if (!s_noPresent) {
            if (s_screenshotCallback) {
                g_theGxDevicePtr->DeviceTakeScreenShot();
                GxScenePresent();
                s_screenshotCallback(CaptureScreen());
                g_theGxDevicePtr->DeviceClearScreenShot();
                s_elapsedSec = 0.0f;
                s_screenshotCallback = nullptr;

                return 1;
            }

            GxScenePresent();
        }

        s_elapsedSec = 0.0f;
    } else {
        GxScenePresent();

        if (OsSleepInBackground()) {
            uint32_t count = OsGetBackgroundSleepMs() / 25;

            for (uint32_t i = 0; i < count; i++) {
                OsSleep(25);
                SE2::HeartBeat(nullptr, nullptr);
            }

            uint32_t remainder = OsGetBackgroundSleepMs() - count * 25;

            if (remainder) {
                OsSleep(remainder);
            }
        }
    }

    return 1;
}

// VERIFY: 04c6e74c auto
// OFFSET: 0x004A8B80
void ILayerInitialize() {
    EventRegister(EVENT_ID_IDLE, OnIdle);
    EventRegister(EVENT_ID_PAINT, OnPaint);
}

// VERIFY: 7c08fb34 auto
// OFFSET: 0x004A8BA0
static void ILayerPaint() {
    OnPaint(nullptr, nullptr);
}

// VERIFY: 38fdd351 manual
// OFFSET: 0x004A8BB0
void ScrnLayerCreate(const RECTF* rect, float zorder, uint32_t flags, void* param, void (*paintFunc)(void* param, const RECTF* rect, const RECTF* visible, float elapsedSec), HLAYER* layer) {
    const RECTF* layerRect = rect ? rect : &s_defaultRect;

    void* mem = SMemAlloc(sizeof(CILayer), "HLAYER", SLOG_OBJECT, 0);
    CILayer* ilayer = mem ? new (mem) CILayer : nullptr;

    ilayer->m_rect = *layerRect;
    ilayer->m_zorder = zorder;
    ilayer->m_flags = flags;
    ilayer->m_param = param;
    ilayer->m_paintFunc = paintFunc;

    CILayer* existing;

    for (existing = s_zorderlist.Tail(); existing && zorder < existing->m_zorder; existing = s_zorderlist.Prev(existing)) {
    }

    s_zorderlist.LinkNode(ilayer, TSLIST_LINK_AFTER, existing);

    *layer = reinterpret_cast<HLAYER>(HandleCreate(ilayer, "HLAYER"));
}

// VERIFY: fc689bb0 manual
// OFFSET: 0x004A8C90
CILayer::~CILayer() {
}

// VERIFY: 393d0327 auto
// OFFSET: 0x004A8B60
void ILayerDestroy() {
    EventUnregister(EVENT_ID_IDLE, OnIdle);
    EventUnregister(EVENT_ID_PAINT, OnPaint);
    WaitScreenshotWriter();
}

// VERIFY: 867daeb1 auto
// OFFSET: 0x004A8D00
void ScrnLayerPaint() {
    ILayerPaint();
}
