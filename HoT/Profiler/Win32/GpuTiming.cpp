#include "HoT/Profiler/GpuTiming.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/CGxDeviceD3d9Ex/CGxDeviceD3d9Ex.h"

namespace HoT::Profiler::GpuTiming {

static const size_t ZONE_COUNT = static_cast<size_t>(Zone::Count);
static const size_t GPU_RING = 5;
static const size_t GPU_MARKS = 40;

struct GpuMark {
    Zone m_zone;
    IDirect3DQuery9* m_begin;
    IDirect3DQuery9* m_end;
};

struct GpuFrame {
    IDirect3DQuery9* m_disjoint = nullptr;
    IDirect3DQuery9* m_frequency = nullptr;
    IDirect3DQuery9* m_frameBegin = nullptr;
    IDirect3DQuery9* m_frameEnd = nullptr;
    IDirect3DQuery9* m_begin[GPU_MARKS] = {};
    IDirect3DQuery9* m_end[GPU_MARKS] = {};
    Zone m_zones[GPU_MARKS] = {};
    uint32_t m_markCount = 0;
    bool m_open = false;
    bool m_pending = false;
};

static GpuFrame s_gpu[GPU_RING];
static uint32_t s_gpuSlot = 0;
static bool s_gpuCreated = false;
static IDirect3DDevice9* s_gpuDevice = nullptr;
static GpuFrame* s_gpuCurrent = nullptr;

static IDirect3DDevice9* CurrentD3dDevice() {
    CGxDevice* device = g_theGxDevicePtr;

    if (!device || !device->m_context) {
        return nullptr;
    }

    switch (device->DeviceApi()) {
        case GxApi_D3d9:
            return static_cast<CGxDeviceD3d*>(device)->m_d3dDevice;
        case GxApi_D3d9Ex:
            return static_cast<CGxDeviceD3d9Ex*>(device)->m_d3dDevice;
        default:
            return nullptr;
    }
}

static void ReleaseQuery(IDirect3DQuery9*& query) {
    if (query) {
        query->Release();
        query = nullptr;
    }
}

void Release() {
    for (GpuFrame& frame : s_gpu) {
        ReleaseQuery(frame.m_disjoint);
        ReleaseQuery(frame.m_frequency);
        ReleaseQuery(frame.m_frameBegin);
        ReleaseQuery(frame.m_frameEnd);

        for (size_t i = 0; i < GPU_MARKS; i++) {
            ReleaseQuery(frame.m_begin[i]);
            ReleaseQuery(frame.m_end[i]);
        }

        frame.m_markCount = 0;
        frame.m_open = false;
        frame.m_pending = false;
    }

    s_gpuCreated = false;
    s_gpuDevice = nullptr;
    s_gpuCurrent = nullptr;
}

static bool Create(IDirect3DDevice9* device) {
    for (GpuFrame& frame : s_gpu) {
        if (FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &frame.m_disjoint)) || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &frame.m_frequency)) || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &frame.m_frameBegin)) || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &frame.m_frameEnd))) {
            Release();
            return false;
        }

        for (size_t i = 0; i < GPU_MARKS; i++) {
            if (FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &frame.m_begin[i])) || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &frame.m_end[i]))) {
                Release();
                return false;
            }
        }
    }

    s_gpuCreated = true;
    s_gpuDevice = device;

    return true;
}

// Returns false while the results are not ready yet.
static bool CollectGpu(GpuFrame& frame) {
    BOOL disjoint = TRUE;
    UINT64 frequency = 0;
    UINT64 begin = 0;
    UINT64 end = 0;

    if (frame.m_disjoint->GetData(&disjoint, sizeof(disjoint), 0) != S_OK || frame.m_frequency->GetData(&frequency, sizeof(frequency), 0) != S_OK || frame.m_frameEnd->GetData(&end, sizeof(end), 0) != S_OK || frame.m_frameBegin->GetData(&begin, sizeof(begin), 0) != S_OK) {
        return false;
    }

    for (uint32_t i = 0; i < frame.m_markCount; i++) {
        UINT64 value = 0;

        if (frame.m_end[i]->GetData(&value, sizeof(value), 0) != S_OK || frame.m_begin[i]->GetData(&value, sizeof(value), 0) != S_OK) {
            return false;
        }
    }

    frame.m_pending = false;

    if (disjoint || !frequency) {
        OnFrameDropped();
        return true;
    }

    double toMs = 1000.0 / static_cast<double>(frequency);
    double frameMs = end > begin ? (end - begin) * toMs : 0.0;

    double zoneMs[ZONE_COUNT] = {};

    for (uint32_t i = 0; i < frame.m_markCount; i++) {
        UINT64 markBegin = 0;
        UINT64 markEnd = 0;
        frame.m_begin[i]->GetData(&markBegin, sizeof(markBegin), 0);
        frame.m_end[i]->GetData(&markEnd, sizeof(markEnd), 0);

        if (markEnd > markBegin) {
            zoneMs[static_cast<size_t>(frame.m_zones[i])] += (markEnd - markBegin) * toMs;
        }
    }

    OnFrameTimed(frameMs, zoneMs);

    return true;
}

bool BeginFrame(bool enabled) {
    s_gpuCurrent = nullptr;

    if (!enabled) {
        return true;
    }

    IDirect3DDevice9* device = CurrentD3dDevice();

    if (!device) {
        return true;
    }

    if (s_gpuCreated && s_gpuDevice != device) {
        Release();
    }

    if (!s_gpuCreated && !Create(device)) {
        return false;
    }

    s_gpuSlot = (s_gpuSlot + 1) % GPU_RING;
    GpuFrame& frame = s_gpu[s_gpuSlot];

    // The ring came all the way around without the GPU finishing, drop that frame.
    if (frame.m_pending && !CollectGpu(frame)) {
        OnFrameDropped();
        frame.m_pending = false;
    }

    frame.m_markCount = 0;
    frame.m_disjoint->Issue(D3DISSUE_BEGIN);
    frame.m_frameBegin->Issue(D3DISSUE_END);
    frame.m_open = true;
    s_gpuCurrent = &frame;
    return true;
}

void EndFrame() {
    GpuFrame* frame = s_gpuCurrent;
    s_gpuCurrent = nullptr;

    if (!frame || !frame->m_open || !CurrentD3dDevice()) {
        return;
    }

    frame->m_frameEnd->Issue(D3DISSUE_END);
    frame->m_frequency->Issue(D3DISSUE_END);
    frame->m_disjoint->Issue(D3DISSUE_END);
    frame->m_open = false;
    frame->m_pending = true;

    for (GpuFrame& other : s_gpu) {
        if (other.m_pending && &other != frame) {
            CollectGpu(other);
        }
    }
}

int32_t BeginMark(Zone zone) {
    GpuFrame* frame = s_gpuCurrent;

    if (!frame || frame->m_markCount >= GPU_MARKS) {
        return -1;
    }

    uint32_t mark = frame->m_markCount++;
    frame->m_zones[mark] = zone;
    frame->m_begin[mark]->Issue(D3DISSUE_END);

    return static_cast<int32_t>(mark);
}

void EndMark(int32_t mark) {
    GpuFrame* frame = s_gpuCurrent;

    if (frame && mark >= 0 && static_cast<uint32_t>(mark) < frame->m_markCount) {
        frame->m_end[mark]->Issue(D3DISSUE_END);
    }
}

bool Available() {
    return CurrentD3dDevice() != nullptr;
}

void DeviceMemory(double& availableTexMB, double& texCacheMB, double& texCacheLimitMB) {
    if (IDirect3DDevice9* d3d = CurrentD3dDevice()) {
        availableTexMB = d3d->GetAvailableTextureMem() / (1024.0 * 1024.0);
    }

    CGxDevice* device = g_theGxDevicePtr;

    if (device && device->DeviceApi() == GxApi_D3d9Ex) {
        CGxDeviceD3d9Ex* ex = static_cast<CGxDeviceD3d9Ex*>(device);
        texCacheMB = ex->m_texCacheUsed / (1024.0 * 1024.0);
        texCacheLimitMB = ex->m_texCacheSize / (1024.0 * 1024.0);
    }
}

}
