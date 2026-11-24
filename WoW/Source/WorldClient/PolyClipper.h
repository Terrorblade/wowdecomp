#ifndef WOW_SOURCE_WORLDCLIENT_POLYCLIPPER_H
#define WOW_SOURCE_WORLDCLIENT_POLYCLIPPER_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

class C4Plane;
class PCResultsIterator;

class PolyClipper {
    public:
    enum {
        MAX_VTX_BUFFER_SIZE = 32,
    };

    class PCDataProxy {
        public:
        /* 0000 */ mutable void* m_data;
        /* 0004 */ const PCDataProxy* m_a;
        /* 0008 */ const PCDataProxy* m_b;
        /* 000C */ float m_t;

        void* GetData(void* (*lerp)(const void*, const void*, float)) const;
    };

    class PCVertex {
        public:
        /* 0000 */ C3Vector m_pos;
        /* 000C */ PCDataProxy m_proxy;
        /* 001C */ int32_t m_edges[2];
        /* 0024 */ uint32_t m_stamp;
        /* 0028 */ float m_dist;
    };

    class PCEdge {
        public:
        /* 0000 */ int32_t m_verts[2];
        /* 0008 */ bool m_clipped;
    };

    /* 0000 */ PCVertex m_verts[MAX_VTX_BUFFER_SIZE];
    /* 0580 */ PCEdge m_edges[16];
    /* 0640 */ int32_t m_vertCount;
    /* 0644 */ int32_t m_edgeCount;
    /* 0648 */ uint32_t m_stamp;
    /* 064C */ int32_t m_firstEdge;

    PolyClipper();
    ~PolyClipper();
    void AddVertex(const C3Vector& pos, const void* data);
    void Begin();
    int32_t Clip(const C4Plane& plane, int32_t side);
    void End();
    PCResultsIterator GetResults(void* (*lerp)(const void*, const void*, float)) const;
};

class PCResultsIterator {
    public:
    /* 0000 */ const PolyClipper* m_clipper;
    /* 0004 */ void* (*m_lerp)(const void*, const void*, float);
    /* 0008 */ int32_t m_front;
    /* 000C */ int32_t m_back;
    /* 0010 */ uint32_t m_step;
    /* 0014 */ bool m_finished;

    PCResultsIterator(const PolyClipper* clipper, void* (*lerp)(const void*, const void*, float));
    ~PCResultsIterator();
    void GetVertex(C3Vector& pos, const void*& data) const;
    bool HasFinished() const;
    void Next();
};

#endif
