#ifndef HOT_DB_CLIENTDB_H
#define HOT_DB_CLIENTDB_H

#include <cstdint>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/WowClientDB.h"

namespace HoT::DB {

// Opens a WDBC file and validates its header. Returns null on any problem instead of a fatal error.
SFileImpl* OpenTable(const char* filename, int32_t columns, int32_t rowSize, int32_t& numRecords, uint32_t& stringSize);
bool ReadStrings(SFileImpl* f, const char* filename, char* strings, uint32_t stringSize);

// Old buffers stay alive because cached record pointers into them are everywhere in the client.
void Retire(void* block, uint32_t bytes);
uint64_t RetiredBytes();

template <class DB, class T>
bool TryLoad(DB& db, const char* file, int32_t line) {
    int32_t numRecords;
    uint32_t stringSize;
    SFileImpl* f = OpenTable(T::GetFilename(), T::s_numColumns, T::s_rowSize, numRecords, stringSize);

    if (!f) {
        return false;
    }

    db.m_numRecords = numRecords;

    if (!numRecords) {
        SFileCloseFile(f);
        db.m_loaded = 1;
        return true;
    }

    char* strings = static_cast<char*>(SMemAlloc(stringSize, file, line, 0));
    db.m_strings = strings;
    db.m_maxID = 0;
    db.m_minID = 0xFFFFFFF;

    db.LoadRecords(f, file, line);

    bool ok = ReadStrings(f, T::GetFilename(), strings, stringSize);
    SFileCloseFile(f);
    db.m_loaded = 1;

    return ok;
}

template <class T>
bool TryLoad(WowClientDB<T>& db, const char* file, int32_t line) {
    return TryLoad<WowClientDB<T>, T>(db, file, line);
}

template <class T>
bool TryLoad(WowClientDB_RLE<T>& db, const char* file, int32_t line) {
    return TryLoad<WowClientDB_RLE<T>, T>(db, file, line);
}

// Replaces the live table with the winning file, rows removed from the file are gone afterwards.
template <class T>
bool Reload(WowClientDB<T>& live, const char* file, int32_t line) {
    WowClientDB<T> fresh;

    if (!TryLoad(fresh, file, line)) {
        return false;
    }

    if (live.m_loaded) {
        Retire(live.m_records, sizeof(T) * live.m_numRecords);
        Retire(live.m_recordsById, sizeof(T*) * (live.m_maxID - live.m_minID + 1));
        Retire(const_cast<char*>(live.m_strings), 0);
    }

    live.m_records = fresh.m_records;
    live.m_recordsById = fresh.m_recordsById;
    live.m_strings = fresh.m_strings;
    live.m_numRecords = fresh.m_numRecords;
    live.m_minID = fresh.m_minID;
    live.m_maxID = fresh.m_maxID;
    live.m_loaded = 1;

    fresh.Reset();
    return true;
}

template <class T>
bool Reload(WowClientDB_RLE<T>& live, const char* file, int32_t line) {
    WowClientDB_RLE<T> fresh;

    if (!TryLoad(fresh, file, line)) {
        return false;
    }

    // Rows are only ever copied out of RLE storage, so only the strings that copies point into must outlive the swap.
    if (live.m_loaded) {
        if (live.m_data) {
            SMemFree(live.m_data, __FILE__, __LINE__, 0);
        }

        if (live.m_records) {
            SMemFree(live.m_records, __FILE__, __LINE__, 0);
        }

        Retire(const_cast<char*>(live.m_strings), 0);
    }

    live.m_generation++;

    live.m_data = fresh.m_data;
    live.m_records = fresh.m_records;
    live.m_recordsById = fresh.m_recordsById;
    live.m_strings = fresh.m_strings;
    live.m_numRecords = fresh.m_numRecords;
    live.m_minID = fresh.m_minID;
    live.m_maxID = fresh.m_maxID;
    live.m_loaded = 1;

    fresh.Reset();
    return true;
}

}

#endif
