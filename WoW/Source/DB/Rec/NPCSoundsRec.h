#ifndef WOW_SOURCE_DB_REC_NPCSOUNDSREC_H
#define WOW_SOURCE_DB_REC_NPCSOUNDSREC_H

#include "WoW/Source/Object/ObjectClient/Types.h"

#include <cstddef>
#include <cstdint>

class SFileImpl;

class NPCSoundsRec {
    public:
    static const int32_t s_numColumns = 5;
    static const int32_t s_rowSize = 20;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_soundID [NUM_NPCSOUNDS];

    NPCSoundsRec();
    ~NPCSoundsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
