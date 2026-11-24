#ifndef ENGINE_SOURCE_FRAME_CSIMPLEEDITBOX_H
#define ENGINE_SOURCE_FRAME_CSIMPLEEDITBOX_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/CDataAllocator.h"
#include "Engine/Source/Frame/CSimpleFontedFrame.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/CRect.h"

enum EDITBOX_FLAGS : uint32_t {
    EDITBOX_AUTO_FOCUS = 0x1,
    EDITBOX_MULTILINE = 0x2,
    EDITBOX_NUMERIC = 0x4,
    EDITBOX_PASSWORD = 0x8,
    EDITBOX_ALT_ARROW_KEY_MODE = 0x10,
    EDITBOX_COUNT_INVISIBLE_LETTERS = 0x20,
};

class CEvent;
class CSimpleFontString;
class CSimpleMessageFrame;
class CSimpleTexture;

class CSimpleEditBox : public CSimpleFrame, public CSimpleFontedFrame {
    public:
    enum DIRTY_BITS : uint32_t {
        DIRTY_NONE = 0,
        DIRTY_TEXT = 0x1,
        DIRTY_HIGHLIGHT = 0x2,
        DIRTY_CURSOR = 0x4,
        DIRTY_USER_INPUT = 0x8,
        DIRTY_TEXT_AND_CURSOR = 0x5,
        DIRTY_HIGHLIGHT_AND_CURSOR = 0x6,
        DIRTY_RENDER = 0x7,
    };

    struct EditHistory {
        /* 0000 */ char* m_text;
        /* 0004 */ const char* m_taint;

        EditHistory();
        EditHistory(const EditHistory& source);
        ~EditHistory();
    };

    class EventHandler {
        public:
        virtual void unk_0000() = 0;
        virtual void unk_0004() = 0;
        virtual void unk_0008() = 0;
        virtual void unk_000C() = 0;
        virtual void OnEvent(const CEvent& evt) = 0;
    };

    struct EventCallback {
        /* 0000 */ uint32_t m_id;
        /* 0004 */ CSimpleEditBox::EventHandler* m_handler;
    };

    static CDataAllocator s_allocator;
    static CSimpleEditBox* s_currentFocus;
    static int32_t s_metatable;
    static int32_t s_objectType;
    static const char* s_objectTypeName;
    static const char* s_inputLanguageNames[4];

    static void operator delete(void* ptr);
    static void ClearKeyboardFocus(CSimpleEditBox* editBox, bool runScript);
    static void CreateScriptMetaTable();
    static void DestroyScriptMetaTable();
    static int32_t GetObjectType();
    static void RegisterScriptMethods(lua_State* L);
    static void SetKeyboardFocus(CSimpleEditBox* editBox);

    /* 02A0 */ uint32_t m_editBoxFlags;
    /* 02A4 */ uint32_t m_dirtyBits;
    /* 02A8 */ const char* m_textTaint;
    /* 02AC */ CSimpleFontedFrameFont* m_font;
    /* 02B0 */ CSimpleFontString* m_fontString;
    /* 02B4 */ char* m_text;
    /* 02B8 */ uint32_t* m_textInfo;
    /* 02BC */ char* m_textHidden;
    /* 02C0 */ int32_t m_textLength;
    /* 02C4 */ int32_t m_textMaxLength;
    /* 02C8 */ int32_t m_textMaxLetters;
    /* 02CC */ uint32_t m_textSize;
    /* 02D0 */ int32_t m_visiblePosition;
    /* 02D4 */ int32_t m_visibleLength;
    /* 02D8 */ CSimpleTexture* m_highlightTextures[3];
    /* 02E4 */ int32_t m_highlightLeft;
    /* 02E8 */ int32_t m_highlightRight;
    /* 02EC */ int32_t m_highlightDrag;
    /* 02F0 */ CSimpleTexture* m_cursorTexture;
    /* 02F4 */ int32_t m_cursorPos;
    /* 02F8 */ float m_cursorBlinkSpeed;
    /* 02FC */ float m_blinkElapsedTime;
    /* 0300 */ TSGrowableArray<uint32_t> m_visibleLines;
    /* 0310 */ int32_t m_historyLines;
    /* 0314 */ int32_t m_historyIndex;
    /* 0318 */ TSFixedArray<CSimpleEditBox::EditHistory> m_history;
    /* 0324 */ CSimpleEditBox::EventCallback m_enterPressedCallback;
    /* 032C */ CSimpleEditBox::EventCallback m_escapePressedCallback;
    /* 0334 */ CSimpleEditBox::EventCallback m_spacePressedCallback;
    /* 033C */ CSimpleEditBox::EventCallback m_tabPressedCallback;
    /* 0344 */ CSimpleEditBox::EventCallback m_textChangedCallback;
    /* 034C */ CSimpleEditBox::EventCallback m_textSetCallback;
    /* 0354 */ int32_t m_imeComposition;
    /* 0358 */ int32_t m_inputLanguage;
    /* 035C */ CSimpleTexture* m_clauseHighlight;
    /* 0360 */ int32_t m_clauseStart;
    /* 0364 */ int32_t m_clauseEnd;
    /* 0368 */ CSimpleMessageFrame* m_candidatesFrame;
    /* 036C */ CSimpleTexture* m_candidateHighlight;
    /* 0370 */ CRect m_textInsets;
    /* 0380 */ FrameScript_Object::ScriptIx m_onEnterPressed;
    /* 0388 */ FrameScript_Object::ScriptIx m_onEscapePressed;
    /* 0390 */ FrameScript_Object::ScriptIx m_onSpacePressed;
    /* 0398 */ FrameScript_Object::ScriptIx m_onTabPressed;
    /* 03A0 */ FrameScript_Object::ScriptIx m_onTextChanged;
    /* 03A8 */ FrameScript_Object::ScriptIx m_onTextSet;
    /* 03B0 */ FrameScript_Object::ScriptIx m_onCursorChanged;
    /* 03B8 */ FrameScript_Object::ScriptIx m_onInputLanguageChanged;
    /* 03C0 */ FrameScript_Object::ScriptIx m_onEditFocusGained;
    /* 03C8 */ FrameScript_Object::ScriptIx m_onEditFocusLost;
    /* 03D0 */ FrameScript_Object::ScriptIx m_onCharComposition;

    virtual ~CSimpleEditBox();
    virtual int32_t GetScriptMetaTable();
    virtual FrameScript_Object::ScriptIx* GetScriptByName(const char* name, FrameScript_Object::ScriptData& data);
    virtual bool IsA(int32_t type);
    virtual bool IsA(const char* typeName);
    virtual const char* GetObjectTypeName();
    virtual void GetScriptTime(lua_State* L, double* time, uint32_t* count, int32_t recurse);
    virtual void OnLayerShow();
    virtual void OnLayerHide();
    virtual void OnLayerUpdate(float elapsedSec);
    virtual int32_t OnLayerTrackUpdate(const CMouseEvent& evt);
    virtual void OnFrameSizeChanged(float width, float height);
    virtual int32_t OnLayerIme(const CImeEvent& evt);
    virtual int32_t OnLayerKeyDownRepeat(const CKeyEvent& evt);
    virtual int32_t OnLayerChar(const CCharEvent& evt);
    virtual int32_t OnLayerKeyDown(const CKeyEvent& evt);
    virtual int32_t OnLayerKeyUp(const CKeyEvent& evt);
    virtual int32_t OnLayerMouseDown(const CMouseEvent& evt, const char* btn);
    virtual int32_t OnLayerMouseUp(const CMouseEvent& evt, const char* btn);
    virtual void LoadXML(XMLNode* node, CStatus* status);
    virtual void FontUpdated(CSimpleFontedFrameFont* font, int32_t flags);

    CSimpleEditBox(CSimpleFrame* parent);
    void AddHistoryLine(const char* text, const char* taint);
    void BackwardHistory();
    void ClearHistory();
    int32_t ConvertCoordinateToIndex(float x, float y, int32_t& index);
    void CopyToClipboard();
    void CreateCandidatesFrame();
    void CreateClauseHighlight();
    void Delete(int32_t count, int32_t script);
    void DeleteBackward(int32_t script);
    void DeleteBackwardWord(int32_t script);
    void DeleteForward(int32_t script);
    void DeleteForwardWord(int32_t script);
    void DeleteHighlight(int32_t script);
    void DeleteSubstring(int32_t left, int32_t right, int32_t script);
    void DeleteToEnd(int32_t script);
    void DeleteToStart(int32_t script);
    void DisableIME();
    void ExtendHighlight(int32_t count);
    void ForwardHistory();
    const char* GetInputLanguageString();
    int32_t GetLenToNum(int32_t offset, int32_t length);
    int32_t GetNumToLen(int32_t offset, int32_t count, bool skipHyperlinks);
    void GetEditTextInsets(float& left, float& right, float& top, float& bottom);
    void GrowText(int32_t length);
    void HighlightText(int32_t left, int32_t right);
    void Insert(const char* text, const char* taint, int32_t runScripts, int32_t highlight, int32_t script);
    void Insert(uint32_t ch);
    void MakeTextVisible(int32_t position, float offset, float width);
    void Move(int32_t count, int32_t highlight);
    void MoveBackward(int32_t highlight);
    void MoveBackwardWord(int32_t highlight);
    void MoveForward(int32_t highlight);
    void MoveForwardWord(int32_t highlight);
    void MoveLine(int32_t count, int32_t highlight);
    void MoveToEnd(int32_t highlight);
    void MoveToLineEnd(int32_t highlight);
    void MoveToLineStart(int32_t highlight);
    void MoveToStart(int32_t highlight);
    void OnFocusChange(bool focus, bool runScript);
    void PasteFromClipboard();
    int32_t PopulateCandidates(uint32_t listMask);
    void RunOnCharCompositionScript(const char* text);
    void RunOnCursorChangedScript(float x, float y, float width, float height);
    void RunOnEnterPressedScript();
    void RunOnEscapePressedScript();
    void RunOnSpacePressedScript();
    void RunOnTabPressedScript();
    void RunOnTextChangedScript();
    void RunOnTextSetScript(const char* taint);
    void SetAltArrowKeyMode(int32_t enable);
    void SetAutoFocus(int32_t enable);
    void SetCountInvisibleLetters(int32_t enable);
    void SetCursorPosition(int32_t position);
    void SetEditTextInsets(float left, float right, float top, float bottom);
    void SetHistoryLines(int32_t lines);
    void SetMultiLine(int32_t enable);
    void SetNumeric(int32_t enable);
    void SetPassword(int32_t enable);
    void SetText(const char* text, const char* taint);
    void ToggleInputLanguage();
    void UpdateClauseInfo();
    void UpdateDirtyBits();
    void UpdateHighlightArea(CSimpleRegion* region, int32_t left, int32_t right);
    void UpdateLanguageIndicator();
    void UpdateSizes();
    void UpdateTextInfo();
    void UpdateVisibleCursor();
    void UpdateVisibleHighlight();
    void UpdateVisibleText();
};

#endif
