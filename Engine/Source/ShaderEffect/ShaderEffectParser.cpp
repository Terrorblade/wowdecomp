#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/ShaderEffect/ShaderEffectParser.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00A4D678
const EGxTexOp CShaderEffectParser::s_gxTexOpValues[GxTexOps_Last] = {
    GxTexOp_Mod,
    GxTexOp_Mod2x,
    GxTexOp_Add,
    GxTexOp_PassThru,
    GxTexOp_Decal,
    GxTexOp_Fade,
};

// STATIC: 0x00B1D5C8
const char* CShaderEffectParser::s_gxTexOpNames[GxTexOps_Last] = {
    "Mod",
    "Mod2x",
    "Add",
    "PassThru",
    "Decal",
    "Fade",
};

// VERIFY: 64b6e307 auto
// OFFSET: 0x00876F10
static int32_t FindScope(const char* data, uint32_t start, uint32_t end, uint32_t* open, uint32_t* close) {
    int32_t depth = 0;

    for (uint32_t i = start; i < end; i++) {
        if (data[i] == '{') {
            if (!depth) {
                *open = i;
            }

            depth++;
        } else if (data[i] == '}' && !--depth) {
            *close = i;
            return 1;
        }
    }

    return 0;
}

// VERIFY: 6bc79b07 auto
// OFFSET: 0x00876F70
static EGxTexOp LookupTexOp(const char* name) {
    for (uint32_t i = 0; i < GxTexOps_Last; i++) {
        if (!SStrCmpI(name, CShaderEffectParser::s_gxTexOpNames[i], STORM_MAX_STR)) {
            return CShaderEffectParser::s_gxTexOpValues[i];
        }
    }

    return GxTexOp_Mod;
}

// VERIFY: 361a6237 manual
// OFFSET: 0x00876E30
int32_t CShaderEffectParser::ExtractFuncKeyword(const char* text, const char* keyword, char (*args)[64], uint32_t& argCount, uint32_t& pos) {
    size_t length = strlen(keyword);

    if (strncmp(text, keyword, length) || text[length] != '(') {
        return 0;
    }

    if (text[length + 1] == ')') {
        argCount = 0;
        pos += static_cast<uint32_t>(length + 2);
        return 1;
    }

    argCount = 1;
    memset(args[0], 0, sizeof(args[0]));

    uint32_t i = static_cast<uint32_t>(length + 1);
    uint32_t j = 0;
    char c = text[i];

    while (c != ')') {
        if (c == ',') {
            argCount++;
            memset(args[argCount - 1], 0, sizeof(args[0]));
            j = 0;
            i++;
        } else {
            args[argCount - 1][j++] = c;
        }

        c = text[i + 1];
        i++;
    }

    pos += i + 1;

    return 1;
}

// VERIFY: 43a0acdb manual
// OFFSET: 0x00876FB0
void CShaderEffectParser::ParseFixedFunc(char* data, uint32_t size, uint32_t& pos, char (*args)[64], uint32_t argCount, ParsedEffect* effect) {
    uint32_t open;
    uint32_t close;

    if (!FindScope(data, pos, size, &open, &close)) {
        return;
    }

    pos = open + 1;

    while (pos < close) {
        if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "Pass", args, argCount, pos)) {
            ParsedEffect::FixedFuncPass* pass = &effect->m_fixedFuncPasses[effect->m_fixedFuncPassCount];

            char* end;
            pass->m_pass = strtol(args[0], &end, 10);

            uint32_t passOpen;
            uint32_t passClose;
            FindScope(data, pos, size, &passOpen, &passClose);

            pass->m_colorOps[0] = GxTexOp_Mod;
            pass->m_colorOps[1] = GxTexOp_Mod;
            pass->m_colorOps[2] = GxTexOp_Mod;
            pass->m_alphaOps[0] = GxTexOp_Mod;
            pass->m_alphaOps[1] = GxTexOp_Mod;
            pass->m_alphaOps[2] = GxTexOp_Mod;

            while (pos < passClose) {
                if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "ColorOp0", args, argCount, pos)) {
                    pass->m_colorOps[0] = LookupTexOp(args[0]);
                } else if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "ColorOp1", args, argCount, pos)) {
                    pass->m_colorOps[1] = LookupTexOp(args[0]);
                } else if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "AlphaOp0", args, argCount, pos)) {
                    pass->m_alphaOps[0] = LookupTexOp(args[0]);
                } else if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "AlphaOp1", args, argCount, pos)) {
                    pass->m_alphaOps[1] = LookupTexOp(args[0]);
                }

                pos++;
            }

            effect->m_fixedFuncPassCount++;
            pos = passClose + 1;
        }

        pos++;
    }

    pos = close + 1;
}

// VERIFY: 6fc19477 manual
// OFFSET: 0x00877150
void CShaderEffectParser::ParseShader(char* data, uint32_t size, uint32_t& pos, char (*args)[64], uint32_t argCount, ParsedEffect* effect) {
    uint32_t open;
    uint32_t close;

    if (!FindScope(data, pos, size, &open, &close)) {
        return;
    }

    pos = open + 1;

    while (pos < close) {
        if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "Pass", args, argCount, pos)) {
            ParsedEffect::ShaderPass* pass = &effect->m_shaderPasses[effect->m_shaderPassCount];

            uint32_t passOpen;
            uint32_t passClose;
            FindScope(data, pos, size, &passOpen, &passClose);

            while (pos < passClose) {
                if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "VertexShader", args, argCount, pos)) {
                    strcpy(pass->m_vertexShader, args[0]);
                } else if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "PixelShader", args, argCount, pos)) {
                    strcpy(pass->m_pixelShader, args[0]);
                }

                pos++;
            }

            effect->m_shaderPassCount++;
            pos = passClose + 1;
        }

        pos++;
    }

    pos = close + 1;
}

// VERIFY: 353721c3 auto
// OFFSET: 0x00877290
void CShaderEffectParser::ParseEffect(char* data, uint32_t size, uint32_t& pos, char (*args)[64], uint32_t argCount, ParsedEffect* effect) {
    SStrCopy(effect->m_name, args[0], sizeof(effect->m_name));
    effect->m_fixedFuncPassCount = 0;
    effect->m_shaderPassCount = 0;

    uint32_t open;
    uint32_t close;

    if (!FindScope(data, pos, size, &open, &close)) {
        return;
    }

    pos = open + 1;

    while (pos < close) {
        if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "FixedFunc", args, argCount, pos)) {
            CShaderEffectParser::ParseFixedFunc(data, size, pos, args, argCount, effect);
        } else if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "Shader", args, argCount, pos)) {
            CShaderEffectParser::ParseShader(data, size, pos, args, argCount, effect);
        }

        pos++;
    }
}

// VERIFY: e3f85c73 auto
// OFFSET: 0x00877360
void CShaderEffectParser::ParseEffectFile(char* data, uint32_t size, void (*callback)(const ParsedEffect&, void*), void* param) {
    char args[16][64];
    ParsedEffect effect;
    uint32_t argCount;

    for (uint32_t pos = 0; pos < size; pos++) {
        if (CShaderEffectParser::ExtractFuncKeyword(data + pos, "Effect", args, argCount, pos)) {
            CShaderEffectParser::ParseEffect(data, size, pos, args, argCount, &effect);
            callback(effect, param);
        }
    }
}
