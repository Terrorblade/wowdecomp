#include <cstdint>
#include "Engine/Source/Gx/CGxLight.h"

// VERIFY: 836f4142 manual
// OFFSET: 0x00683FB0
CGxLight::CGxLight() {
    this->m_flags &= ~CGxLight::F_STATE_MASK;
    this->m_dir = C3Vector(0.0f, 0.0f, 1.0f);
    this->m_ambientColor = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_dirColor = C3Vector(1.0f, 1.0f, 1.0f);
    this->m_specularColor = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_constantAttenuation = 0.0f;
    this->m_linearAttenuation = 0.7f;
    this->m_quadraticAttenuation = 0.03f;
}

// VERIFY: d4f8632f manual
// OFFSET: 0x00684620
CGxApiLight& CGxApiLight::operator=(const CGxLight& light) {
    if (light.m_constantAttenuation != this->m_constantAttenuation) {
        this->m_flags |= CGxApiLight::FIELD_CONSTANT_ATTENUATION;
        this->m_constantAttenuation = light.m_constantAttenuation;
    }

    if (light.m_linearAttenuation != this->m_linearAttenuation) {
        this->m_flags |= CGxApiLight::FIELD_LINEAR_ATTENUATION;
        this->m_linearAttenuation = light.m_linearAttenuation;
    }

    if (light.m_quadraticAttenuation != this->m_quadraticAttenuation) {
        this->m_flags |= CGxApiLight::FIELD_QUADRATIC_ATTENUATION;
        this->m_quadraticAttenuation = light.m_quadraticAttenuation;
    }

    if (light.m_dir.x != this->m_dir.x || light.m_dir.y != this->m_dir.y || light.m_dir.z != this->m_dir.z || static_cast<float>((light.m_flags >> 1) & 1) != this->m_dir.w) {
        this->m_flags |= CGxApiLight::FIELD_DIRECTION_OR_POSITION;
        this->m_dir.x = light.m_dir.x;
        this->m_dir.y = light.m_dir.y;
        this->m_dir.z = light.m_dir.z;
        this->m_dir.w = 1.0f;

        if (light.m_flags & CGxLight::F_OMNI) {
            this->m_flags |= 0xE00000;
        } else {
            this->m_flags &= ~0xE00000u;
            this->m_dir.w = 0.0f;
        }
    }

    if (light.m_dirColor.x != this->m_dirColor.x || light.m_dirColor.y != this->m_dirColor.y || light.m_dirColor.z != this->m_dirColor.z) {
        this->m_dirColor.x = light.m_dirColor.x;
        this->m_dirColor.y = light.m_dirColor.y;
        this->m_flags |= CGxApiLight::FIELD_DIFFUSE;
        this->m_dirColor.z = light.m_dirColor.z;
    }

    if (light.m_ambientColor.x != this->m_ambColor.x || light.m_ambientColor.y != this->m_ambColor.y || light.m_ambientColor.z != this->m_ambColor.z) {
        this->m_ambColor.x = light.m_ambientColor.x;
        this->m_ambColor.y = light.m_ambientColor.y;
        this->m_flags |= CGxApiLight::FIELD_AMBIENT;
        this->m_ambColor.z = light.m_ambientColor.z;
    }

    if (light.m_specularColor.x != this->m_specColor.x || light.m_specularColor.y != this->m_specColor.y || light.m_specularColor.z != this->m_specColor.z) {
        this->m_specColor.x = light.m_specularColor.x;
        this->m_specColor.y = light.m_specularColor.y;
        this->m_flags |= CGxApiLight::FIELD_SPECULAR;
        this->m_specColor.z = light.m_specularColor.z;
    }

    return *this;
}

// INLINED
CGxApiLight::CGxApiLight() : m_dir(), m_constantAttenuation(0.0f), m_linearAttenuation(0.0f), m_quadraticAttenuation(0.0f) {
}
