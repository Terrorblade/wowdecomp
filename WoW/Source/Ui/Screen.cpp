#include "Screen.h"
#include <cstdint>
#include <cstdio>
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"

// VERIFY: a937de73 auto
// OFFSET: 0x0050F7C0
float Screen::GetAspectRatio() {
    CVar* widescreen = CVar::Lookup("widescreen");
    CVar* resolution = CVar::Lookup("gxResolution");
    int32_t width = 4;
    int32_t height = 3;
    char separator;

    if (widescreen && widescreen->m_intValue && resolution) {
        sscanf(resolution->m_stringValue.m_str, "%d%c%d", &width, &separator, &height);

        float aspect;

        if (CGxDevice::DeviceRenderAspect(aspect)) {
            return aspect;
        }
    }

    return static_cast<float>(width) / height;
}
