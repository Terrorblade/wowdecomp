#ifndef WOW_SOURCE_UI_CHATBUBBLEFRAME_H
#define WOW_SOURCE_UI_CHATBUBBLEFRAME_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/CDataAllocator.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Object/WGUID.h"

class CGUnit_C;
class CGWorldFrame;
class CImVector;
class CSimpleFontString;

class CGChatBubbleFrame : public TSLinkedNode<CGChatBubbleFrame>, public CSimpleFrame {
    public:

    enum BUBBLE_FLAGS : int32_t {
        FLAG_FADING_IN = 0x1,
        FLAG_FADING_OUT = 0x2,
        FLAG_REMOVE_AFTER_FADE = 0x4,
        FLAG_NEEDS_SORT = 0x8,
        FLAG_FADE_MASK = 0x3,
        FLAG_FADE_IN_BLOCK_MASK = 0x5,
    };

    static CDataAllocator s_chatBubbleHeap;

    static void operator delete(void* ptr);
    static void ClearAll();
    static CGChatBubbleFrame* GetNewChatBubble(CSimpleFrame* parent);
    static void OnWorldRender(CGWorldFrame* worldFrame);
    static void OnWorldLayerUpdate();
    static void InitializeGame();
    static void Remove(CGChatBubbleFrame* bubble);
    static void ShutdownGame();

    /* 02A4 */ CSimpleFontString* m_fontString;
    /* 02A8 */ uint32_t m_bubbleFlags;
    /* 02AC */ float m_screenX;
    /* 02B0 */ float m_screenY;
    /* 02B4 */ float m_screenDepth;
    /* 02B8 */ C3Vector m_worldPosition;
    /* 02C4 */ uint32_t m_showTime;
    /* 02C8 */ uint32_t m_fadeStart;
    /* 02CC */ int32_t m_duration;
    /* 02D0 */ WOWGUID m_unit;
    /* 02D8 */ float m_height;
    /* 02DC */ uint8_t m_positionValid;

    ~CGChatBubbleFrame() override;

    CGChatBubbleFrame(CSimpleFrame* parent);
    void FadeIn();
    void FadeOut(bool remove);
    void Initialize(const WOWGUID& unit, const char* text, const CImVector& color);
    void Render(CGWorldFrame* worldFrame);
    bool Update(CGUnit_C* player, CGUnit_C* unit);
};

#endif
