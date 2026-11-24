#ifndef WOW_SOURCE_CONSOLE_CONSOLECLIENT_H
#define WOW_SOURCE_CONSOLE_CONSOLECLIENT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/ConsoleBase/Types.h"
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Services/Types.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/CImVector.h"

struct CGxString;
class CONSOLELINE;

class CONSOLELINE : public TSLinkedNode<CONSOLELINE> {
    public:
    /* 0008 */ uint8_t* m_buffer;
    /* 000C */ uint32_t m_chars;
    /* 0010 */ uint32_t m_charsalloc;
    /* 0014 */ uint32_t m_inputpos;
    /* 0018 */ uint32_t m_inputstart;
    /* 001C */ COLOR_T m_colorType;
    /* 0020 */ CGxString* m_fontPointer;

    ~CONSOLELINE();
};

extern int32_t s_active;
extern uint32_t s_baseTextFlags;
extern float s_charSpacing;
extern uint32_t s_colorArray[NUM_COLORTYPES];
extern float s_consoleHeight;
extern float s_consoleLines;
extern float s_fontHeight;
extern char s_fontName[260];
extern TSList<CONSOLELINE, TSGetLink<CONSOLELINE> > s_linelist;
extern uint32_t s_NumLines;
extern char s_repeatBuffer[64];
extern uint32_t s_repeatCount;
extern HTEXTFONT s_textFont;

int32_t ConsoleGetActive();
int32_t ConsoleCommand_AppendLogToFile(const char* command, const char* arguments);
int32_t ConsoleCommand_BackGroundColor(const char* command, const char* arguments);
int32_t ConsoleCommand_BufferSize(const char* command, const char* arguments);
int32_t ConsoleCommand_CharSpacing(const char* command, const char* arguments);
int32_t ConsoleCommand_ClearConsole(const char* command, const char* arguments);
int32_t ConsoleCommand_CloseConsole(const char* command, const char* arguments);
int32_t ConsoleCommand_CurrentSettings(const char* command, const char* arguments);
int32_t ConsoleCommand_DefaultSettings(const char* command, const char* arguments);
int32_t ConsoleCommand_Font(const char* command, const char* arguments);
int32_t ConsoleCommand_FontColor(const char* command, const char* arguments);
int32_t ConsoleCommand_FontSize(const char* command, const char* arguments);
int32_t ConsoleCommand_HighLightColor(const char* command, const char* arguments);
int32_t ConsoleCommand_Proportional(const char* command, const char* arguments);
int32_t ConsoleCommand_RepeatHandler(const char* command, const char* arguments);
void ConsoleScreenDestroy();
void ConsoleScreenInitialize(const char* title);
void GenerateNodeString(CONSOLELINE* node);
const char* GetConsoleMessage();
void SetConsoleMessage(const char* message);

void SetConsoleKey(KEY key);

#endif
