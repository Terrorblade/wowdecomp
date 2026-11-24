#include <cstdint>
#include "Engine/Source/Gx/Buffer.h"
#include "Engine/Source/Gx/Gx.h"

// STATIC: 0x00C60000
static CGxVertexAttrib s_vertexAttribsP[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_P, GxVA_Position), 12 }
};

// STATIC: 0x00C60010
static CGxVertexAttrib s_vertexAttribsPN[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PN, GxVA_Position), 24 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PN, GxVA_Normal), 24 }
};

// STATIC: 0x00C60030
static CGxVertexAttrib s_vertexAttribsPNC[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNC, GxVA_Position), 28 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNC, GxVA_Normal), 28 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PNC, GxVA_Color0), 28 }
};

// STATIC: 0x00C60060
static CGxVertexAttrib s_vertexAttribsPNT[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNT, GxVA_Position), 32 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNT, GxVA_Normal), 32 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNT, GxVA_TexCoord0), 32 }
};

// STATIC: 0x00C60090
static CGxVertexAttrib s_vertexAttribsPNCT[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNCT, GxVA_Position), 36 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNCT, GxVA_Normal), 36 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PNCT, GxVA_Color0), 36 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNCT, GxVA_TexCoord0), 36 }
};

// STATIC: 0x00C600D0
static CGxVertexAttrib s_vertexAttribsPNT2[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNT2, GxVA_Position), 40 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNT2, GxVA_Normal), 40 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNT2, GxVA_TexCoord0), 40 },
    { GxVA_TexCoord1, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNT2, GxVA_TexCoord1), 40 }
};

// STATIC: 0x00C60110
static CGxVertexAttrib s_vertexAttribsPNCT2[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNCT2, GxVA_Position), 44 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNCT2, GxVA_Normal), 44 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PNCT2, GxVA_Color0), 44 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNCT2, GxVA_TexCoord0), 44 },
    { GxVA_TexCoord1, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNCT2, GxVA_TexCoord1), 44 }
};

// STATIC: 0x00C60160
static CGxVertexAttrib s_vertexAttribsPC[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PC, GxVA_Position), 16 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PC, GxVA_Color0), 16 }
};

// STATIC: 0x00C60180
static CGxVertexAttrib s_vertexAttribsPCT[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PCT, GxVA_Position), 24 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PCT, GxVA_Color0), 24 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PCT, GxVA_TexCoord0), 24 }
};

// STATIC: 0x00C601B0
static CGxVertexAttrib s_vertexAttribsPCT2[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PCT2, GxVA_Position), 32 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PCT2, GxVA_Color0), 32 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PCT2, GxVA_TexCoord0), 32 },
    { GxVA_TexCoord1, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PCT2, GxVA_TexCoord1), 32 }
};

// STATIC: 0x00C601F0
static CGxVertexAttrib s_vertexAttribsPT[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PT, GxVA_Position), 20 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PT, GxVA_TexCoord0), 20 }
};

// STATIC: 0x00C60210
static CGxVertexAttrib s_vertexAttribsPT2[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PT2, GxVA_Position), 28 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PT2, GxVA_TexCoord0), 28 },
    { GxVA_TexCoord1, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PT2, GxVA_TexCoord1), 28 }
};

// STATIC: 0x00C60240
static CGxVertexAttrib s_vertexAttribsPBNT2[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PBNT2, GxVA_Position), 48 },
    { GxVA_BlendWeight, GxVAT_UByte4N, GxVertexAttribOffset(GxVBF_PBNT2, GxVA_BlendWeight), 48 },
    { GxVA_BlendIndices, GxVAT_UByte4, GxVertexAttribOffset(GxVBF_PBNT2, GxVA_BlendIndices), 48 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PBNT2, GxVA_Normal), 48 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PBNT2, GxVA_TexCoord0), 48 },
    { GxVA_TexCoord1, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PBNT2, GxVA_TexCoord1), 48 }
};

// STATIC: 0x00C602A0
static CGxVertexAttrib s_vertexAttribsPNC2T2[] = {
    { GxVA_Position, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNC2T2, GxVA_Position), 48 },
    { GxVA_Normal, GxVAT_Float3, GxVertexAttribOffset(GxVBF_PNC2T2, GxVA_Normal), 48 },
    { GxVA_Color0, GxVAT_Color, GxVertexAttribOffset(GxVBF_PNC2T2, GxVA_Color0), 48 },
    { GxVA_Color1, GxVAT_Color, GxVertexAttribOffset(GxVBF_PNC2T2, GxVA_Color1), 48 },
    { GxVA_TexCoord0, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNC2T2, GxVA_TexCoord0), 48 },
    { GxVA_TexCoord1, GxVAT_Float2, GxVertexAttribOffset(GxVBF_PNC2T2, GxVA_TexCoord1), 48 }
};

// STATIC: 0x00A2DA18
const VertexBufDesc Buffer::s_vertexBufDesc[GxVertexBufferFormats_Last] = {
    { s_vertexAttribsP, 1, 12, 0x1 },
    { s_vertexAttribsPN, 2, 24, 0x9 },
    { s_vertexAttribsPNC, 3, 28, 0x19 },
    { s_vertexAttribsPNT, 3, 32, 0x49 },
    { s_vertexAttribsPNCT, 4, 36, 0x59 },
    { s_vertexAttribsPNT2, 4, 40, 0xC9 },
    { s_vertexAttribsPNCT2, 5, 44, 0xD9 },
    { s_vertexAttribsPC, 2, 16, 0x11 },
    { s_vertexAttribsPCT, 3, 24, 0x51 },
    { s_vertexAttribsPCT2, 4, 32, 0xD1 },
    { s_vertexAttribsPT, 2, 20, 0x41 },
    { s_vertexAttribsPT2, 3, 28, 0xC1 },
    { s_vertexAttribsPBNT2, 6, 48, 0xCF },
    { s_vertexAttribsPNC2T2, 6, 48, 0xF9 }
};
