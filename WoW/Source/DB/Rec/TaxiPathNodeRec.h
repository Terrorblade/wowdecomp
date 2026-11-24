#ifndef WOW_SOURCE_DB_REC_TAXIPATHNODEREC_H
#define WOW_SOURCE_DB_REC_TAXIPATHNODEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum TAXI_PATH_NODE_FLAGS : int32_t {
    TAXI_PATH_NODE_FLAG_NONE     = 0x00000000,
    TAXI_PATH_NODE_FLAG_TELEPORT = 0x00000001,
    TAXI_PATH_NODE_FLAG_STOP     = 0x00000002,
};

class TaxiPathNodeRec {
    public:
    static const int32_t s_numColumns = 11;
    static const int32_t s_rowSize = 44;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_pathID;
    /* 0008 */ int32_t m_nodeIndex;
    /* 000C */ int32_t m_continentID;
    /* 0010 */ float m_loc [3];
    /* 001C */ int32_t m_flags;
    /* 0020 */ int32_t m_delay;
    /* 0024 */ int32_t m_arrivalEventID;
    /* 0028 */ int32_t m_departureEventID;

    TaxiPathNodeRec();
    ~TaxiPathNodeRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
