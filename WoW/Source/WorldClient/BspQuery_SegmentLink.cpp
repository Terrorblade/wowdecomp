#include "BspQuery_SegmentLink.h"
#include <cmath>
#include "Engine/Source/Tempest/NTempest.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Common/AaBsp.h"

// VERIFY: d9cfa43d auto
// OFFSET: 0x007C77D0
BspQuery_SegmentLink::BspQuery_SegmentLink(SMOPoly* faces, C3Vector* vertexList, uint16_t* indices, const C3Segment& seg, float interiorMaxT, float groundMaxT) {
    this->m_faces = faces;
    this->m_overflowFlags = nullptr;
    this->m_vertexList = vertexList;
    this->m_indices = indices;
    this->m_ray.origin = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_ray.dir = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_seg.b = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_seg.t = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_ray.origin = seg.b;
    this->m_ray.dir = C3Vector(seg.t.x - seg.b.x, seg.t.y - seg.b.y, seg.t.z - seg.b.z);
    this->m_seg = seg;

    float length = std::sqrt(this->m_ray.dir.z * this->m_ray.dir.z + this->m_ray.dir.y * this->m_ray.dir.y + this->m_ray.dir.x * this->m_ray.dir.x);
    this->m_oosegMag = 1.0f / length;
    this->m_ray.dir.x = 1.0f / length * this->m_ray.dir.x;
    this->m_ray.dir.y = this->m_ray.dir.y * (1.0f / length);
    this->m_ray.dir.z = 1.0f / length * this->m_ray.dir.z;
    this->m_interiorMaxT = interiorMaxT;
    this->m_groundMaxT = groundMaxT;
    this->m_interiorFace = -1;
    this->m_groundFace = -1;
    this->m_faceIgnoreFlags = SMOPoly::F_NOCAMCOLLIDE | SMOPoly::F_COLLIDE_HIT;
    this->m_interiorT = interiorMaxT * length;
    this->m_groundT = length * groundMaxT;
}

// OFFSET: 0x007C6600
void BspQuery_SegmentLink::operator()(uint16_t face) {
    uint8_t flags = this->m_faces[face].m_flags;

    if (flags & this->m_faceIgnoreFlags) {
        return;
    }

    if (BspQuery::testFaceSub >= BspQuery::MAX_FACES) {
        return;
    }

    BspQuery::testFaces[BspQuery::testFaceSub++] = face;
    this->m_faces[face].m_flags |= SMOPoly::F_COLLIDE_HIT;

    float t = 0.0f;

    if (!NTempest::Intersect(this->m_ray, this->m_vertexList, &this->m_indices[3 * face], &t, nullptr, 0.0020000001f)) {
        return;
    }

    if (flags & SMOPoly::F_RENDER) {
        if (t < 0.0f) {
            return;
        }

        if (t <= this->m_interiorT) {
            this->m_interiorT = t;
            this->m_interiorFace = face;
        }

        if (t <= this->m_groundT) {
            this->m_groundFace = face;
            this->m_groundT = t;
        }
    } else if (flags & SMOPoly::F_COLLISION) {
        if (t >= 0.0f && t <= this->m_interiorT) {
            this->m_interiorFace = face;
            this->m_interiorT = t;
        }
    } else if (flags & SMOPoly::F_DETAIL) {
        if (t >= 0.0f && t <= this->m_groundT) {
            this->m_groundFace = face;
            this->m_groundT = t;
        }
    }
}

// VERIFY: bd10af2f manual
// OFFSET: 0x007C6710
bool BspQuery_SegmentLink::GetHits(float* interiorT, int32_t* interiorFace, float* groundT, int32_t* groundFace) {
    *interiorFace = -1;
    *groundFace = -1;

    if (this->m_interiorFace == -1 && this->m_groundFace == -1) {
        return false;
    }

    if (this->m_groundFace != -1) {
        float t = this->m_groundT * this->m_oosegMag;
        *groundT = t;

        if (t >= this->m_groundMaxT) {
            t = this->m_groundMaxT;
        }

        *groundT = t;
        *groundFace = this->m_groundFace;
    }

    if (this->m_interiorFace != -1) {
        float t = this->m_interiorT * this->m_oosegMag;
        *interiorT = t;

        if (t >= this->m_interiorMaxT) {
            t = this->m_interiorMaxT;
        }

        *interiorT = t;
        *interiorFace = this->m_interiorFace;
    }

    return true;
}

// VERIFY: 62c67e6e manual
// OFFSET: 0x007C6790
bool BspQuery_SegmentLink::GetFaceIndicesUsingCache(const CAaBsp& aaBsp, const CAaBspNode* node) {
    if (!g_BspDigestCache) {
        return false;
    }

    CAaBspNodeDigest* digest = g_BspDigestCache->GetDigest(aaBsp, node, this->m_faces, this->m_vertexList, this->m_indices);

    if (!digest) {
        return false;
    }

    float maxX = this->m_seg.t.x;
    float maxY = this->m_seg.t.y;
    float minX = this->m_seg.b.x;
    float maxZ = this->m_seg.t.z;

    if (minX > maxX) {
        minX = this->m_seg.t.x;
        maxX = this->m_seg.b.x;
    }

    float minY = this->m_seg.b.y;

    if (minY > maxY) {
        minY = this->m_seg.t.y;
        maxY = this->m_seg.b.y;
    }

    float minZ = this->m_seg.b.z;

    if (minZ > maxZ) {
        minZ = this->m_seg.t.z;
        maxZ = this->m_seg.b.z;
    }

    minX -= 0.0099999998f;
    maxX += 0.0099999998f;
    minY -= 0.0099999998f;
    maxY += 0.0099999998f;
    minZ -= 0.0099999998f;
    maxZ += 0.0099999998f;

    uint8_t outcodes[450];

    for (uint32_t i = 0; i < digest->m_nVertices; i++) {
        const C3Vector& vertex = digest->m_vertices[i];
        uint8_t code = 0;

        if (minX > vertex.x) {
            code = 0x20;
        }

        if (maxX < vertex.x) {
            code |= 0x10;
        }

        if (minY > vertex.y) {
            code |= 0x8;
        }

        if (maxY < vertex.y) {
            code |= 0x4;
        }

        if (minZ > vertex.z) {
            code |= 0x2;
        }

        if (maxZ < vertex.z) {
            code |= 0x1;
        }

        outcodes[i] = code;
    }

    for (uint32_t i = 0; i < digest->m_nFaces; i++) {
        uint16_t face = digest->m_faceIndices[i];
        uint32_t flags = this->m_faces[face].m_flags;

        if ((this->m_faceIgnoreFlags & digest->m_faceFlags[i]) || (flags & this->m_faceIgnoreFlags)) {
            continue;
        }

        if (BspQuery::testFaceSub >= BspQuery::MAX_FACES) {
            continue;
        }

        BspQuery::testFaces[BspQuery::testFaceSub++] = face;
        this->m_faces[face].m_flags |= SMOPoly::F_COLLIDE_HIT;

        const uint16_t* verts = &digest->m_faceVertexIndices[3 * i];

        if (outcodes[verts[2]] & outcodes[verts[1]] & outcodes[verts[0]] & 0x3F) {
            continue;
        }

        float t = 0.0f;

        if (!NTempest::Intersect(this->m_ray, digest->m_vertices, verts, &t, nullptr, 0.0020000001f)) {
            continue;
        }

        if (flags & SMOPoly::F_RENDER) {
            if (t < 0.0f) {
                continue;
            }

            if (t <= this->m_interiorT) {
                this->m_interiorT = t;
                this->m_interiorFace = face;
            }

            if (t <= this->m_groundT) {
                this->m_groundT = t;
                this->m_groundFace = face;
            }
        } else if (flags & SMOPoly::F_COLLISION) {
            if (t >= 0.0f && t <= this->m_interiorT) {
                this->m_interiorT = t;
                this->m_interiorFace = face;
            }
        } else if (flags & SMOPoly::F_DETAIL) {
            if (t >= 0.0f && t <= this->m_groundT) {
                this->m_groundT = t;
                this->m_groundFace = face;
            }
        }
    }

    return true;
}
