#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gxu/GxuFontFace.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/MSDF/MSDF.h"

// STATIC: 0x00C7D2FC
TSHashTable<FACEDATA, HASHKEY_STRI> s_faceHash;

// VERIFY: e0237d01 auto
// OFFSET: 0x006C8080
FT_Face FontFaceGetFace(HFACE handle) {
    if (!handle) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    return reinterpret_cast<FACEDATA*>(handle)->m_face;
}

// VERIFY: 5082eae0 auto
// OFFSET: 0x006C80A0
void FontFaceCloseHandle(HFACE handle) {
    if (!handle) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    FACEDATA* face = reinterpret_cast<FACEDATA*>(handle);

    HandleClose(reinterpret_cast<HOBJECT>(handle));

    if (face->m_refcount <= 1) {
        HOBJECT selfReference = face->m_selfReference;
        face->m_selfReference = nullptr;
        HandleClose(selfReference);
    }
}

// VERIFY: c0728d10 auto
// OFFSET: 0x006C80E0
const char* FontFaceGetFontName(HFACE handle) {
    if (!handle) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    return reinterpret_cast<FACEDATA*>(handle)->m_key.m_str;
}

HoT::MSDF::Font* FontFaceGetMsdf(HFACE handle) {
    return handle ? reinterpret_cast<FACEDATA*>(handle)->m_msdf : nullptr;
}

// VERIFY: 63774048 manual
// OFFSET: 0x006C8190
FACEDATA::FACEDATA() {
    this->m_data = nullptr;
    this->m_face = nullptr;
    this->m_msdf = nullptr;
}

// VERIFY: 263b0e27 manual
// OFFSET: 0x006C81C0
FACEDATA::~FACEDATA() {
    // HoT: the MSDF font reads the face data, so it goes first.
    HoT::MSDF::CloseFont(this->m_msdf);
    this->m_msdf = nullptr;

    if (this->m_face) {
        FT_Done_Face(this->m_face);
    }

    if (this->m_data) {
        SFile::Unload(this->m_data);
    }
}

// VERIFY: 45a8d799 auto
// OFFSET: 0x006C8AE0
HFACE FontFaceGetHandle(const char* fileName, FT_Library library) {
    if (!library || !fileName || !*fileName) {
        return nullptr;
    }

    FACEDATA* existing = s_faceHash.Ptr(fileName);

    if (existing) {
        return reinterpret_cast<HFACE>(HandleDuplicate(existing->m_selfReference));
    }

    HFACE handle = nullptr;
    void* data = nullptr;
    uint32_t size;

    if (SFile::ReadFile(nullptr, fileName, &data, &size, 0, 3, nullptr)) {
        if (!data) {
            return handle;
        }

        FT_Face ftFace = nullptr;

        if (!FT_New_Memory_Face(library, static_cast<FT_Byte*>(data), size, 0, &ftFace) && ftFace) {
            if (!FT_Select_Charmap(ftFace, ft_encoding_unicode)) {
                FACEDATA* face = new ("HFACE", SLOG_OBJECT) FACEDATA;
                face->m_data = data;
                face->m_face = ftFace;
                // HoT: give the face a distance field font when MSDF is on and the font passes validation.
                face->m_msdf = HoT::MSDF::OpenFont(ftFace, data, size);
                s_faceHash.Insert(face, fileName);
                data = nullptr;
                ftFace = nullptr;
                face->m_selfReference = HandleCreate(face, "HFACE");
                handle = reinterpret_cast<HFACE>(HandleDuplicate(face->m_selfReference));
            }

            // The face reads from data, so it goes before the unload below
            if (ftFace) {
                FT_Done_Face(ftFace);
            }
        }
    }

    if (data) {
        SFile::Unload(data);
    }

    return handle;
}
