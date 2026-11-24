#ifndef WOW_SOURCE_GLUE_CGLUELOADING_H
#define WOW_SOURCE_GLUE_CGLUELOADING_H

#include <cstdint>

class CCharacterComponent;

class CGlueLoading {
    public:

    enum LOAD_STATE : int32_t {
        LOAD_STATE_NONE = 0,
        LOAD_STATE_LOADING = 1,
        LOAD_STATE_FINISHING = 2,
        LOAD_STATE_DONE = 3,
    };

    static uint32_t m_loadStartTime;
    static uint32_t m_loadCompleteTime;
    static int32_t m_loadState;
    static float m_startProgress;
    static float m_loadProgress;

    static float ComputeLoadProgress(CCharacterComponent* component, int64_t& loaded, int64_t& total);
    static int32_t Idle(CCharacterComponent* component);
    static void StartLoad(CCharacterComponent* component, int32_t keepProgress);
};

#endif
