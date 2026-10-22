#include <cmath>
#include <cstdint>
#include <cstring>
#include "Engine/Source/Model2/M2Light.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxLight.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CImVector.h"

// STATIC: 0x00AF59D4
static float s_lightGridScale = 0.05f;

// VERIFY: 45311b80 auto
// OFFSET: 0x008348D0
void CM2Light::Initialize(CM2Scene* scene) {
    this->m_scene = scene;
    this->m_stamp = scene ? scene->m_frameStamp - 1 : 0;
}

// VERIFY: c00e67e1 manual
// OFFSET: 0x00834900
void CM2Lighting::Initialize(CM2Scene* scene, const CAaSphere& sphere) {
    memset(this, 0, sizeof(*this));
    this->m_scene = scene;
    this->m_flags |= 0x20;
    this->m_sphere4 = sphere;
}

// VERIFY: 5a29d87b auto
// OFFSET: 0x00834940
void CM2Lighting::SetFog(const C3Vector& color, float start, float end) {
    this->m_fogStart = start;
    this->m_fogEnd = end;
    this->m_fogScale = 1.0f / (end - start);
    this->m_fogDensity = 1.0f;
    this->m_fogColor = color;
}

// VERIFY: 23d655f9 auto
// OFFSET: 0x00834990
void CM2Lighting::SetFog(const C3Vector& color, float start, float end, float density) {
    this->m_fogStart = start;
    this->m_fogEnd = end;
    this->m_fogScale = 1.0f / (end - start);
    this->m_fogDensity = density;
    this->m_fogColor = color;
}

// VERIFY: 90fc1f85 auto
// OFFSET: 0x008349E0
int32_t CM2Lighting::GetPointLight(uint32_t index, C3Vector& pos, C3Vector& color, C3Vector& attenuation) {
    if (index >= this->m_lightCount) {
        return 0;
    }

    CM2Light* light = this->m_lights[index];
    pos = light->m_pos;
    color = light->m_dirColor;
    attenuation.x = light->m_constantAttenuation;
    attenuation.y = light->m_linearAttenuation;
    attenuation.z = light->m_quadraticAttenuation;

    return 1;
}

// VERIFY: a0e7de7f auto
// OFFSET: 0x00834A40
CM2Light::CM2Light() {
    this->m_scene = nullptr;
    this->m_stamp = 0;
    this->m_type = M2LIGHT_POINT;
    this->m_constantAttenuation = 0.0f;
    this->m_linearAttenuation = 0.7f;
    this->m_quadraticAttenuation = 0.03f;
    this->m_visible = 0;
    this->m_lightPrev = nullptr;
    this->m_lightNext = nullptr;
}

// INLINED
CM2Light::~CM2Light() {
    this->Unlink();
}

// VERIFY: 42088760 manual
// OFFSET: 0x00834AB0
void CM2Light::Unlink() {
    if (this->m_lightPrev) {
        *this->m_lightPrev = this->m_lightNext;
    }

    if (this->m_lightNext) {
        this->m_lightNext->m_lightPrev = this->m_lightPrev;
    }

    this->m_lightNext = nullptr;
    this->m_lightPrev = nullptr;
}

// VERIFY: cc997e91 manual
// OFFSET: 0x00834AE0
void CM2Light::SetDirection(const C3Vector& dir) {
    this->m_dir = dir;
    this->m_dir.NormalizeSafe();
}

// VERIFY: 4eab0c16 manual
// OFFSET: 0x00834B50
void CM2Light::GxLightSet(uint32_t index) {
    CGxLight light;
    light.m_flags = (light.m_flags & ~1u) | (this->m_visible & 1);

    if (this->m_type == M2LIGHT_DIRECT) {
        light.m_flags &= ~2u;
        light.m_dir = this->m_dir;
        light.m_ambientColor = this->m_ambColor;
        light.m_specularColor = this->m_specColor;
    } else {
        light.m_flags |= 2;
        light.m_dir = this->m_pos;
        light.m_ambientColor = C3Vector();
        light.m_specularColor = C3Vector();
    }

    light.m_dirColor = this->m_dirColor;
    light.m_constantAttenuation = this->m_constantAttenuation;
    light.m_linearAttenuation = this->m_linearAttenuation;
    light.m_quadraticAttenuation = this->m_quadraticAttenuation;

    g_theGxDevicePtr->LightSet(index, light, C3Vector());
    g_theGxDevicePtr->LightEnable(index, 1);
}

// VERIFY: a73679ed manual
// OFFSET: 0x00834C70
void CM2Light::Link() {
    if (!this->m_visible || !this->m_scene) {
        return;
    }

    if (this->m_type == M2LIGHT_POINT) {
        if (!this->m_scene->m_lightGrid) {
            this->m_scene->m_lightGrid = static_cast<CM2Light**>(SMemAlloc(sizeof(CM2Light*) * 64 * 64, __FILE__, __LINE__, 0));
            memset(this->m_scene->m_lightGrid, 0, sizeof(CM2Light*) * 64 * 64);
        }

        int32_t y = static_cast<int32_t>(floor(this->m_pos.y * s_lightGridScale)) & 63;
        int32_t x = static_cast<int32_t>(floor(this->m_pos.x * s_lightGridScale)) & 63;
        CM2Light** head = &this->m_scene->m_lightGrid[y * 64 + x];

        this->m_lightPrev = head;
        this->m_lightNext = *head;
        *this->m_lightPrev = this;

        if (this->m_lightNext) {
            this->m_lightNext->m_lightPrev = &this->m_lightNext;
        }
    } else if (!(this->m_scene->m_flags & 1)) {
        CM2Light** head = &this->m_scene->m_lightList;

        this->m_lightPrev = head;
        this->m_lightNext = *head;
        *head = this;

        if (this->m_lightNext) {
            this->m_lightNext->m_lightPrev = &this->m_lightNext;
        }
    }
}

// VERIFY: bb2e385b manual
// OFFSET: 0x00834D90
void CM2Lighting::AddAmbient(const C3Vector& color) {
    this->m_sunAmbient += color;
}

// VERIFY: b0a37a1c manual
// OFFSET: 0x00834DC0
void CM2Lighting::AddDiffuse(const C3Vector& color, const C3Vector& dir) {
    C3Vector viewDir = dir;

    if (this->m_scene) {
        const C44Matrix& view = this->m_scene->m_view;
        viewDir.x = view.b0 * dir.y + view.c0 * dir.z + dir.x * view.a0;
        viewDir.y = view.b1 * dir.y + view.a1 * dir.x + view.c1 * dir.z;
        viewDir.z = view.b2 * dir.y + view.a2 * dir.x + view.c2 * dir.z;
    }

    this->m_vector18.x = viewDir.x * color.x + this->m_vector18.x;
    this->m_vector18.y = viewDir.y * color.x + this->m_vector18.y;
    this->m_vector18.z = color.x * viewDir.z + this->m_vector18.z;

    this->m_vector24.x = viewDir.x * color.y + this->m_vector24.x;
    this->m_vector24.y = viewDir.y * color.y + this->m_vector24.y;
    this->m_vector24.z = color.y * viewDir.z + this->m_vector24.z;

    this->m_vector30.x = color.z * viewDir.x + this->m_vector30.x;
    this->m_vector30.y = viewDir.y * color.z + this->m_vector30.y;
    this->m_vector30.z = color.z * viewDir.z + this->m_vector30.z;

    float luminance = color.y * 0.71516f + color.x * 0.212671f + color.z * 0.072169f;
    this->m_vector3C.x = viewDir.x * luminance + this->m_vector3C.x;
    this->m_vector3C.y = viewDir.y * luminance + this->m_vector3C.y;
    this->m_vector3C.z = viewDir.z * luminance + this->m_vector3C.z;

    this->m_vector48 += color;
    this->m_sunDir = dir;
    this->m_sunDiffuse = color;
}

// VERIFY: 7d1db3b8 manual
// OFFSET: 0x00834F60
void CM2Lighting::AddLight(CM2Light* light) {
    if (!light->m_visible) {
        return;
    }

    if (light->m_type != M2LIGHT_POINT) {
        this->AddAmbient(light->m_ambColor);
        this->AddDiffuse(light->m_dirColor, light->m_dir);
        this->m_sunSpecular += light->m_specColor;
        return;
    }

    float dz = light->m_pos.z - this->m_sphere4.c.z;
    float dy = light->m_pos.y - this->m_sphere4.c.y;
    float dx = light->m_pos.x - this->m_sphere4.c.x;
    float dist = dx * dx + (dy * dy + dz * dz);

    uint32_t count = this->m_lightCount;
    const uint32_t capacity = sizeof(this->m_lights) / sizeof(this->m_lights[0]);

    if (count >= capacity) {
        if (dist >= this->m_lightDist[capacity - 1]) {
            return;
        }

        count--;
    }

    for (; count && dist <= this->m_lightDist[count - 1]; count--) {
        this->m_lights[count] = this->m_lights[count - 1];
        this->m_lightDist[count] = this->m_lightDist[count - 1];
    }

    this->m_lightDist[count] = dist;
    this->m_lights[count] = light;

    if (this->m_lightCount < capacity) {
        this->m_lightCount++;
    }
}

// VERIFY: 58f9ffa3 manual
// OFFSET: 0x008350A0
void CM2Lighting::CameraSpace() {
    if (this->m_flags & 1) {
        return;
    }

    if (this->m_scene) {
        for (uint32_t i = 0; i < this->m_lightCount; i++) {
            this->m_lights[i]->m_viewPos = this->m_lights[i]->m_pos * this->m_scene->m_view;
        }

        if ((this->m_flags & 0x60) == 0x60) {
            float d = -this->m_liquidPlane.d;
            C3Vector point(d * this->m_liquidPlane.n.x, this->m_liquidPlane.n.y * d, d * this->m_liquidPlane.n.z);

            const C44Matrix& view = this->m_scene->m_view;
            C3Vector normal;
            normal.x = view.c0 * this->m_liquidPlane.n.z + view.b0 * this->m_liquidPlane.n.y + this->m_liquidPlane.n.x * view.a0;
            normal.y = view.a1 * this->m_liquidPlane.n.x + view.c1 * this->m_liquidPlane.n.z + view.b1 * this->m_liquidPlane.n.y;
            normal.z = view.a2 * this->m_liquidPlane.n.x + view.c2 * this->m_liquidPlane.n.z + view.b2 * this->m_liquidPlane.n.y;
            this->m_liquidPlane.n = normal;
            this->m_liquidPlane.n.NormalizeSafe();

            C3Vector viewPoint = point * this->m_scene->m_view;
            this->m_liquidPlane.d = -(viewPoint.z * this->m_liquidPlane.n.z + viewPoint.y * this->m_liquidPlane.n.y + this->m_liquidPlane.n.x * viewPoint.x);
        }
    }

    this->m_flags |= 1;
}

// VERIFY: 0203843d manual
// OFFSET: 0x00835280
void CM2Lighting::SetupSunlight() {
    if (this->m_flags & 2) {
        return;
    }

    this->m_sunDir = this->m_vector3C;

    if (this->m_sunDir.z * this->m_sunDir.z + this->m_sunDir.y * this->m_sunDir.y + this->m_sunDir.x * this->m_sunDir.x <= 0.00001f) {
        this->m_sunDir = C3Vector(0.0f, 0.0f, -1.0f);
    } else {
        this->m_sunDir.NormalizeSafe();
    }

    float red = this->m_sunDir.z * this->m_vector18.z + this->m_sunDir.y * this->m_vector18.y + this->m_sunDir.x * this->m_vector18.x;
    float green = this->m_sunDir.z * this->m_vector24.z + this->m_sunDir.y * this->m_vector24.y + this->m_sunDir.x * this->m_vector24.x;
    float blue = this->m_vector30.z * this->m_sunDir.z + this->m_vector30.y * this->m_sunDir.y + this->m_vector30.x * this->m_sunDir.x;

    this->m_sunDiffuse = C3Vector(red * 1.25f - this->m_vector48.x * 0.25f, green * 1.25f - this->m_vector48.y * 0.25f, 1.25f * blue - this->m_vector48.z * 0.25f);
    this->m_sunAmbient += C3Vector((this->m_vector48.x - red) * 0.25f, (this->m_vector48.y - green) * 0.25f, (this->m_vector48.z - blue) * 0.25f);

    this->m_flags |= 2;
}

// VERIFY: 52fc9b8b manual
// OFFSET: 0x008353D0
void CM2Lighting::SetupGxLights(const C3Vector* origin) {
    CGxLight light;
    light.m_flags |= 1;

    this->SetupSunlight();

    light.m_flags &= ~2u;
    light.m_dir = this->m_sunDir;
    light.m_ambientColor = this->m_sunAmbient;
    light.m_dirColor = this->m_sunDiffuse;
    light.m_specularColor = this->m_sunSpecular;

    g_theGxDevicePtr->LightSet(0, light, C3Vector());
    g_theGxDevicePtr->LightEnable(0, 1);

    light.m_flags |= 2;
    light.m_ambientColor = C3Vector();
    light.m_specularColor = C3Vector();

    uint32_t index = 1;

    for (uint32_t i = this->m_lightCount; i > 0 && index < 4; i--) {
        CM2Light* point = this->m_lights[i - 1];

        if (origin) {
            light.m_dir = point->m_pos - *origin;
        } else {
            light.m_dir = point->m_viewPos;
        }

        light.m_dirColor = point->m_dirColor;
        light.m_constantAttenuation = point->m_constantAttenuation;
        light.m_linearAttenuation = point->m_linearAttenuation;
        light.m_quadraticAttenuation = point->m_quadraticAttenuation;

        g_theGxDevicePtr->LightSet(index, light, C3Vector());
        g_theGxDevicePtr->LightEnable(index, 1);
        index++;
    }

    for (; index < 4; index++) {
        g_theGxDevicePtr->LightEnable(index, 0);
    }
}

// VERIFY: c72eeeeb auto
// OFFSET: 0x008355D0
int32_t CM2Lighting::GetSunLight(C3Vector& dir, C3Vector& ambient, C3Vector& diffuse, C3Vector& specular) {
    this->SetupSunlight();

    dir = this->m_sunDir;
    ambient = this->m_sunAmbient;
    diffuse = this->m_sunDiffuse;
    specular = this->m_sunSpecular;

    return 1;
}

// VERIFY: 0a2d28ed manual
// OFFSET: 0x00835640
void CM2Light::SetLightType(M2LIGHTTYPE type) {
    if (this->m_type == type) {
        return;
    }

    this->m_type = type;

    if (this->m_visible && this->m_scene) {
        this->Unlink();
        this->Link();
    }
}

// VERIFY: 1b918590 manual
// OFFSET: 0x00835690
void CM2Light::SetPosition(const C3Vector& pos) {
    this->m_pos = pos;

    if (this->m_visible && this->m_scene && this->m_type == M2LIGHT_POINT) {
        this->Unlink();
        this->Link();
    }
}

// VERIFY: 33e205ca auto
// OFFSET: 0x008356F0
void CM2Light::SetVisible(int32_t visible) {
    if (!this->m_visible == !visible) {
        return;
    }

    this->m_visible = visible;

    if (!this->m_scene) {
        return;
    }

    if (visible) {
        this->Link();
    } else {
        this->Unlink();
    }
}

// VERIFY: 530853de manual
// OFFSET: 0x00835750
void CM2Lighting::SetupGxFog() {
    if (fabs(this->m_fogScale) < 0.00000023841858f) {
        GxRsSet(GxRs_Fog, 0);
        return;
    }

    if (!g_theGxDevicePtr->Caps().m_shaderTargets[0]) {
        GxRsSet(GxRs_FogStart, this->m_fogStart);
        GxRsSet(GxRs_FogEnd, this->m_fogEnd);
    }

    float blue = this->m_fogColor.z > 0.0f ? (this->m_fogColor.z < 1.0f ? this->m_fogColor.z * 255.0f + 0.5f : 255.0f) : 0.0f;
    float green = this->m_fogColor.y > 0.0f ? (this->m_fogColor.y < 1.0f ? this->m_fogColor.y * 255.0f + 0.5f : 255.0f) : 0.0f;
    float red = this->m_fogColor.x > 0.0f ? (this->m_fogColor.x < 1.0f ? this->m_fogColor.x * 255.0f + 0.5f : 255.0f) : 0.0f;

    CImVector color;
    color.b = static_cast<uint8_t>(static_cast<int64_t>(blue));
    color.a = 0xFF;
    color.g = static_cast<uint8_t>(static_cast<int64_t>(green));
    color.r = static_cast<uint8_t>(static_cast<int64_t>(red));

    GxRsSet(GxRs_FogColor, *reinterpret_cast<int32_t*>(&color));
    GxRsSet(GxRs_Fog, 1);
}
