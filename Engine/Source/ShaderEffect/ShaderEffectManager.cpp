#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "Engine/Source/ShaderEffect/ShaderEffectManager.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00D43408
TSHashTable<CShaderEffect, HASHKEY_STRI> CShaderEffectManager::m_ShaderList;

// VERIFY: 554f80ac manual
// OFFSET: 0x008764D0
void CShaderEffectManager::DestroyAllEffects() {
    CShaderEffect* effect = CShaderEffectManager::m_ShaderList.m_fulllist.Head();

    while (effect) {
        effect = CShaderEffectManager::m_ShaderList.DeleteNode(effect);
    }
}

// VERIFY: e6522edb auto
// OFFSET: 0x00876530
CShaderEffect* CShaderEffectManager::GetEffect(const char* name) {
    return CShaderEffectManager::m_ShaderList.Ptr(name);
}

// VERIFY: 82eb3c99 manual
// OFFSET: 0x00876BE0
CShaderEffect* CShaderEffectManager::CreateEffect(const char* name) {
    CShaderEffectManager::m_ShaderList.Ptr(name);

    return CShaderEffectManager::m_ShaderList.New(name, 0, 0);
}

// VERIFY: 156a1818 manual
// OFFSET: 0x00876CA0
void CShaderEffectManager::ParseEffectCallback(const CShaderEffectParser::ParsedEffect& effect, void* param) {
    CShaderEffect* shaderEffect = CShaderEffectManager::m_ShaderList.New(effect.m_name, 0, 0);

    shaderEffect->InitEffect(effect.m_shaderPasses[0].m_vertexShader, effect.m_shaderPasses[0].m_pixelShader);

    for (uint32_t i = 0; i < effect.m_fixedFuncPassCount; i++) {
        const CShaderEffectParser::ParsedEffect::FixedFuncPass& pass = effect.m_fixedFuncPasses[i];
        shaderEffect->InitFixedFuncPass(pass.m_colorOps, pass.m_alphaOps, pass.m_pass);
    }
}

// VERIFY: c6a3a3f5 manual
// OFFSET: 0x00876D90
void CShaderEffectManager::AddEffectFile(const char* fileName) {
    char path[256];
    sprintf(path, "Shaders\\Effects\\%s", fileName);

    SFileImpl* file = nullptr;
    SFileOpenFile(path, &file);

    if (!file) {
        return;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);
    char* data = static_cast<char*>(SMemAlloc(size + 1, __FILE__, __LINE__, 0));
    SFileReadFile(file, data, size, nullptr, nullptr, 0);
    SFileCloseFile(file);
    data[size] = '\0';

    CShaderEffectParser::ParseEffectFile(data, size, CShaderEffectManager::ParseEffectCallback, nullptr);

    delete[] data;
}
