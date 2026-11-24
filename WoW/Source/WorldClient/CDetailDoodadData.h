#ifndef WOW_SOURCE_WORLDCLIENT_CDETAILDOODADDATA_H
#define WOW_SOURCE_WORLDCLIENT_CDETAILDOODADDATA_H

#include <cstddef>
#include <cstdint>

class CM2Model;
class CTexture;
class GroundEffectDoodadRec;

enum GROUND_EFFECT_DOODAD_FLAGS : int32_t {
    GROUND_EFFECT_DOODAD_ALIGN_TO_TERRAIN = 0x1,
    GROUND_EFFECT_DOODAD_NO_VERTEX_SHADING = 0x2,
};

class CDetailDoodadData {
    public:
    static void ModelLoadedCallback(CM2Model* model, void* arg);

    /* 0000 */ GroundEffectDoodadRec* m_record;
    /* 0004 */ CM2Model* m_model;
    /* 0008 */ CTexture* m_texture;

    int32_t Load();
};

#endif
