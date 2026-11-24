#include "NameGen.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRandom.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/NameGenRec.h"

// VERIFY: 671b4278 auto
// OFFSET: 0x004E6360
static int32_t QSortDictionary(const void* l, const void* r) {
    DictionaryRecord left = *static_cast<const DictionaryRecord*>(l);
    DictionaryRecord right = *static_cast<const DictionaryRecord*>(r);
    return SStrCmp(left.m_str, right.m_str, STORM_MAX_STR);
}

// VERIFY: 99831e05 manual
// OFFSET: 0x004E63A0
void CNameGen::GenerateName(char* buffer, uint32_t size) {
    int32_t choice = 0;

    memset(buffer, 0, size);

    if (!this->m_dictionaryCount) {
        return;
    }

    while (true) {
        int32_t first = this->m_dictionaryCount;
        int32_t last = 0;
        uint32_t total = 0;

        if (!*buffer) {
            for (int32_t i = 0; i < this->m_dictionaryCount; i++) {
                DictionaryRecord& record = this->m_dictionary.m_data[i];

                if (record.m_str[0] == '_') {
                    if (i < first) {
                        first = i;
                    }

                    if (i > last) {
                        last = i;
                    }

                    total += record.m_count;
                } else if (last > first) {
                    break;
                }
            }
        } else {
            for (int32_t i = 0; i < this->m_dictionaryCount; i++) {
                if (!SStrCmp(this->m_dictionary.m_data[i].m_str, &buffer[SStrLen(buffer) - 2], 2)) {
                    if (i < first) {
                        first = i;
                    }

                    if (i > last) {
                        last = i;
                    }

                    total += this->m_dictionary.m_data[i].m_count;
                } else if (last > first) {
                    break;
                }
            }
        }

        if (last < first) {
            break;
        }

        int32_t roll = static_cast<int32_t>((static_cast<uint64_t>(total) * CRandom::uint32_(g_rndSeed)) >> 32);

        for (int32_t i = first; i <= last; i++) {
            if (roll < this->m_dictionary.m_data[i].m_count) {
                choice = i;
                break;
            }

            roll -= this->m_dictionary.m_data[i].m_count;
        }

        if (!*buffer) {
            SStrCopy(buffer, this->m_dictionary.m_data[choice].m_str, size);
            continue;
        }

        if (!this->m_dictionary.m_data[choice].m_str[2]) {
            break;
        }

        SStrPack(buffer, &this->m_dictionary.m_data[choice].m_str[2], size);

        if (SStrLen(buffer) >= size - 1) {
            break;
        }
    }

    memmove(buffer, buffer + 1, size - 1);
    *buffer = static_cast<char>(toupper(*buffer));
}

// VERIFY: a1cebea0 manual
// OFFSET: 0x004E6560
void CNameGen::CreateDictionary() {
    for (int32_t i = 0; i < static_cast<int32_t>(this->m_names.Count()); i++) {
        char name[64];
        SStrPrintf(name, sizeof(name), "_%s", this->m_names.m_data[i]);
        SStrLower(name);

        int32_t length = static_cast<int32_t>(SStrLen(name) - 1);

        for (int32_t j = 0; j < length; j++) {
            DictionaryRecord record = { { 0 }, 0 };
            SStrCopy(record.m_str, &name[j], sizeof(record.m_str));
            record.m_count = 1;

            int32_t k;

            for (k = 0; k < this->m_dictionaryCount; k++) {
                if (!SStrCmp(this->m_dictionary.m_data[k].m_str, record.m_str, STORM_MAX_STR)) {
                    this->m_dictionary.m_data[k].m_count++;
                    break;
                }
            }

            if (k >= this->m_dictionaryCount && static_cast<uint32_t>(this->m_dictionaryCount) < this->m_dictionary.Count()) {
                this->m_dictionary.m_data[this->m_dictionaryCount++] = record;
            }
        }
    }

    qsort(this->m_dictionary.m_data, this->m_dictionaryCount, sizeof(DictionaryRecord), QSortDictionary);
}

// VERIFY: 423dcff5 manual
// OFFSET: 0x004E6720
void CNameGen::ClearDictionary() {
    this->m_dictionaryCount = 0;
    this->m_dictionary.Clear();
    this->m_dictionary.SetCount(10000);
}

// VERIFY: ca606df1 manual
// OFFSET: 0x004E6790
void CNameGen::LoadNames(int32_t raceID, int32_t sexID) {
    this->m_names.Clear();
    this->ClearDictionary();

    int32_t start;

    for (start = 0; start < g_nameGenDB.m_numRecords; start++) {
        NameGenRec* rec = start >= 0 && start < g_nameGenDB.m_numRecords ? &g_nameGenDB.m_records[start] : nullptr;

        if (rec->m_raceID == raceID && rec->m_sex == sexID) {
            break;
        }
    }

    if (start >= g_nameGenDB.m_numRecords) {
        return;
    }

    int32_t end;

    for (end = start; end < g_nameGenDB.m_numRecords; end++) {
        NameGenRec* rec = end >= 0 && end < g_nameGenDB.m_numRecords ? &g_nameGenDB.m_records[end] : nullptr;

        if (rec->m_raceID != raceID || rec->m_sex != sexID) {
            break;
        }
    }

    int32_t count = end - start;
    this->m_names.SetCount(count);

    for (int32_t i = 0; i < count; i++) {
        int32_t index = start + i;
        NameGenRec* rec = index >= 0 && index < g_nameGenDB.m_numRecords ? &g_nameGenDB.m_records[index] : nullptr;
        this->m_names.m_data[i] = rec->m_name;
    }

    this->CreateDictionary();
}
