#ifndef ENGINE_SOURCE_FRAME_CSIMPLEMOVIEFRAME_H
#define ENGINE_SOURCE_FRAME_CSIMPLEMOVIEFRAME_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/CDataAllocator.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/SoundEngine/SoundEngine.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C2iVector.h"

class CGxTex;
class CRenderBatch;
struct lua_State;

enum MOVIE_LAYOUT : uint32_t {
    MOVIE_LAYOUT_800 = 0,
    MOVIE_LAYOUT_1024 = 1,
    MOVIE_LAYOUT_800X448 = 2,
    MOVIE_LAYOUT_1024X576 = 3,
    MOVIE_LAYOUT_800X342 = 4,
    MOVIE_LAYOUT_1024X436 = 5,
    NUM_MOVIE_LAYOUTS = 6,
};

struct MOVIEKEYFRAME {
    int32_t m_frame;
    uint32_t m_offset;
};

struct MOVIECAPTION {
    /* 0000 */ uint32_t m_startTime;
    /* 0004 */ uint32_t m_endTime;
    /* 0008 */ char* m_text;
};

struct MOVIETEXTUREINFO {
    /* 0000 */ uint32_t m_width;
    /* 0004 */ uint32_t m_height;
    /* 0008 */ uint32_t m_offset;
    /* 000C */ uint32_t m_stride;
};

struct MOVIETEXTUREDATA {
    /* 0000 */ const MOVIETEXTUREINFO* m_info;
    /* 0004 */ uint8_t* m_buffer;
};

struct MOVIETEXTURERECT {
    /* 0000 */ float m_left;
    /* 0004 */ float m_right;
    /* 0008 */ float m_top;
    /* 000C */ float m_bottom;
};

struct MOVIEUPDATERECT {
    /* 0000 */ int32_t m_minX;
    /* 0004 */ int32_t m_minY;
    /* 0008 */ int32_t m_maxX;
    /* 000C */ int32_t m_maxY;
};

class CSimpleMovieFrame : public CSimpleFrame {
    public:
    static CDataAllocator s_allocator;
    static CSimpleMovieFrame* s_currentMovieFrame;
    static int32_t s_decoderCount;
    static int32_t s_metatable;
    static int32_t s_objectType;
    static const char* s_objectTypeName;

    static void operator delete(void* ptr);
    static void AudioDeviceChanged_Restart();
    static void AudioDeviceChanged_ShutDown();
    static void CreateScriptMetaTable();
    static void DestroyScriptMetaTable();
    static int32_t GetObjectType();
    static void RegisterScriptMethods(lua_State* L);
    static void RenderMovie(void* param);

    /* 029C */ SESound m_sound;
    /* 02A0 */ int32_t m_isPlaying;
    /* 02A4 */ int32_t m_finished;
    /* 02A8 */ int32_t m_subtitlesEnabled;
    /* 02AC */ int32_t m_audioRestart;
    /* 02B0 */ char m_filename[256];
    /* 03B0 */ int32_t m_volume;
    /* 03B4 */ FrameScript_Object::ScriptIx m_onMovieFinished;
    /* 03BC */ FrameScript_Object::ScriptIx m_onMovieShowSubtitle;
    /* 03C4 */ FrameScript_Object::ScriptIx m_onMovieHideSubtitle;
    /* 03CC */ int32_t m_decoder;
    /* 03D0 */ float m_fps;
    /* 03D4 */ int32_t m_width;
    /* 03D8 */ int32_t m_height;
    /* 03DC */ int32_t m_frameCount;
    /* 03E0 */ uint32_t m_layout;
    /* 03E4 */ CGxTex* m_textures[6];
    /* 03FC */ MOVIETEXTUREDATA m_textureData[6];
    /* 042C */ uint8_t* m_frameBuffer;
    /* 0430 */ uint8_t* m_videoData;
    /* 0434 */ uint8_t* m_videoPos;
    /* 0438 */ uint32_t m_videoDataSize;
    /* 043C */ uint8_t* m_audioData;
    /* 0440 */ uint32_t m_audioDataSize;
    /* 0444 */ uint32_t m_startTime;
    /* 0448 */ int32_t m_currentFrame;
    /* 044C */ int32_t m_targetFrame;
    /* 0450 */ int32_t m_droppedFrames;
    /* 0454 */ int32_t m_frameOffset;
    /* 0458 */ uint32_t m_elapsedMs;
    /* 045C */ TSGrowableArray<MOVIECAPTION> m_captions;
    /* 046C */ int32_t m_currentCaption;
    TSGrowableArray<MOVIEKEYFRAME> m_keyframes;

    virtual ~CSimpleMovieFrame();
    virtual int32_t GetScriptMetaTable();
    virtual FrameScript_Object::ScriptIx* GetScriptByName(const char* name, FrameScript_Object::ScriptData& data);
    virtual bool IsA(int32_t type);
    virtual bool IsA(const char* typeName);
    virtual const char* GetObjectTypeName();
    virtual void GetScriptTime(lua_State* L, double* time, uint32_t* count, int32_t recurse);
    virtual void OnFrameRender(CRenderBatch* batch, uint32_t layer);

    CSimpleMovieFrame(CSimpleFrame* parent);
    void CloseAudio();
    void CloseCaptions();
    void CloseVideo();
    bool DecodeFrame(bool render);
    int32_t OpenAudio(const char* fileName, int32_t volume, uint32_t position);
    int32_t OpenCaptions(const char* fileName);
    int32_t OpenVideo(const char* fileName);
    int32_t ParseAVIFile(const char* fileName);
    void Render();
    void RunOnMovieShowSubtitleScript(const char* text);
    int32_t StartMovie(const char* fileName, int32_t volume);
    void StopMovie();
    bool UpdateTiming();
};

#endif
