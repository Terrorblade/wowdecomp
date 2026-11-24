#ifndef ENGINE_SOURCE_FRAME_CFRAMEPOINT_H
#define ENGINE_SOURCE_FRAME_CFRAMEPOINT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/CDataAllocator.h"
#include "Engine/Source/Frame/Types.h"

class CLayoutFrame;
class CRect;

class CFramePoint {
    public:

    enum POINT_FLAGS : uint32_t {
        FLAG_NONE = 0,
        FLAG_ABSOLUTE_COORDINATES = 1,
        FLAG_READING_RELATIVE_RECT = 2,
        FLAG_CHANGED_DURING_READ = 4,
        FLAG_UNUSED = 8,
        FLAG_BUSY_CHANGED_MASK = 6,
    };

    static float UNDEFINED;
    static CDataAllocator s_framePointHeap;

    static void* operator new(size_t bytes) noexcept;
    static void operator delete(void* ptr);

    /* 0000 */ float m_x;
    /* 0004 */ float m_y;
    /* 0008 */ CLayoutFrame* m_relative;
    /* 000C */ FRAMEPOINT m_framePoint : 8;
    /* 000C */ uint32_t m_flags : 24;

    CFramePoint(CLayoutFrame* relative, FRAMEPOINT framePoint, float x, float y);
    int32_t GetRelativeRect(CRect& rect);
    void MarkUnused();
    void SetRelative(CLayoutFrame* relative, FRAMEPOINT framePoint, float x, float y);
    float X(float scale);
    float Y(float scale);
};

#endif
