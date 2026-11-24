#include "WoW/Source/Game/DeclinedWords.h"
#include <cstring>
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/DeclinedWordCasesRec.h"
#include "WoW/Source/DB/Rec/DeclinedWordRec.h"
#include "WoW/Source/Game/GameClient/NameCache.h"
#include "WoW/Source/Game/GameClient/PetNameCache.h"
#include "WoW/Source/Game/ValidateName.h"

// STATIC: 0x00D37DE4
static TSHashTable<DECLINEDWORD, HASHKEY_CONSTSTR> s_declinedWords;

// VERIFY: 6162c5b1 manual
// OFFSET: 0x007E01B0
void DeclinedWordsDestroy() {
    s_declinedWords.Clear();
}

// VERIFY: a01d8da3 manual
// OFFSET: 0x007E08E0
void DeclinedWordsInitialize() {
    g_declinedWordDB.Load(__FILE__, __LINE__);
    g_declinedWordCasesDB.Load(__FILE__, __LINE__);
    int32_t caseIndex = 0;
    DeclinedWordCasesRec* cases = g_declinedWordCasesDB.m_numRecords <= 0 ? nullptr : g_declinedWordCasesDB.m_records;

    for (int32_t i = 0; i < g_declinedWordDB.m_numRecords; i++) {
        DeclinedWordRec* word = g_declinedWordDB.GetRecordByIndex(i);

        if (!cases) {
            continue;
        }

        while (cases->m_declinedWordID < word->m_id) {
            cases = g_declinedWordCasesDB.GetRecordByIndex(++caseIndex);

            if (!cases) {
                break;
            }
        }

        if (cases && cases->m_declinedWordID == word->m_id) {
            DECLINEDWORD* entry = s_declinedWords.New(word->m_word, 0, 0);
            entry->m_wordId = word->m_id;
            entry->m_caseIndex = caseIndex;
        }
    }
}

// VERIFY: a1a88ad6 manual
// OFFSET: 0x007E0A50
int32_t GetDeclinedWord(const char* word, int32_t declension, char* buffer, uint32_t bufferSize) {
    if (!s_declinedWords.m_fulllist.Head()) {
        DeclinedWordsInitialize();
    }

    const char* src = word;
    char c = *src;

    if (c != '|') {
        if (!NameNeedsDeclensions(s_locale, word)) {
            return static_cast<int32_t>(SStrCopy(buffer, word, bufferSize));
        }

        int32_t index = declension - 1;

        if (static_cast<uint32_t>(index) <= 4) {
            NameCache* name = g_nameDBCache.GetRecordByName(word);

            if (name && name->m_declinedNames) {
                return static_cast<int32_t>(SStrCopy(buffer, name->m_declinedNames->m_names[index], bufferSize));
            }

            PetNameCache* pet = g_petNameCache.GetRecordByName(word);

            if (pet && pet->m_declinedNames) {
                return static_cast<int32_t>(SStrCopy(buffer, pet->m_declinedNames->m_names[index], bufferSize));
            }
        }

        DECLINEDWORD* entry = s_declinedWords.Ptr(word);

        if (entry) {
            int32_t caseIndex = entry->m_caseIndex;
            int32_t wordId = entry->m_wordId;

            for (DeclinedWordCasesRec* rec = g_declinedWordCasesDB.GetRecordByIndex(caseIndex); rec && rec->m_declinedWordID == wordId && rec->m_caseIndex <= declension; rec = g_declinedWordCasesDB.GetRecordByIndex(++caseIndex)) {
                if (rec->m_caseIndex == declension) {
                    return static_cast<int32_t>(SStrCopy(buffer, rec->m_declinedWord, bufferSize));
                }
            }

            return static_cast<int32_t>(SStrCopy(buffer, word, bufferSize));
        }

        const char* dash = SStrChr(word, '-');

        if (dash) {
            char prefix[1024];
            uint32_t length = static_cast<uint32_t>(dash - word);

            if (length > sizeof(prefix) - 1) {
                length = sizeof(prefix) - 1;
            }

            memcpy(prefix, word, length);
            prefix[length] = '\0';

            if (index < 0) {
                return static_cast<int32_t>(SStrCopy(buffer, word, bufferSize));
            }

            if (index < 5) {
                NameCache* name = g_nameDBCache.GetRecordByName(prefix);

                if (name && name->m_declinedNames) {
                    uint32_t count = static_cast<uint32_t>(SStrCopy(buffer, name->m_declinedNames->m_names[index], bufferSize));
                    return static_cast<int32_t>(count + SStrCopy(&buffer[count], dash, bufferSize - count));
                }

                PetNameCache* pet = g_petNameCache.GetRecordByName(prefix);

                if (pet && pet->m_declinedNames) {
                    uint32_t count = static_cast<uint32_t>(SStrCopy(buffer, pet->m_declinedNames->m_names[index], bufferSize));
                    return static_cast<int32_t>(count + SStrCopy(&buffer[count], dash, bufferSize - count));
                }
            }
        }

        char declensions[NUM_DECLENSIONS][1024];

        if (index < NUM_DECLENSIONS && GetNameDeclensions(s_locale, declensions[0], word, 2, 0)) {
            return static_cast<int32_t>(SStrCopy(buffer, declensions[index], bufferSize));
        }

        return static_cast<int32_t>(SStrCopy(buffer, word, bufferSize));
    }

    char* dest = buffer;
    uint32_t count = 0;

    while (c == '|') {
        if (count < bufferSize) {
            *dest++ = '|';
            count++;
        }

        c = *++src;

        if (c == 'C' || c == 'c') {
            if (count < bufferSize) {
                *dest++ = c;
                count++;
            }

            src++;
            uint32_t length = 0;

            while (*src) {
                if (count < bufferSize) {
                    *dest++ = *src;
                    count++;
                }

                length++;
                src++;

                if (length >= 8) {
                    break;
                }
            }

            if (length < 8) {
                break;
            }
        } else if (c == 'H' || c == 'T') {
            char end = c == 'H' ? 'h' : 't';

            while (c != '|' || src[1] != end) {
                if (count < bufferSize) {
                    *dest++ = c;
                    count++;
                }

                c = *++src;

                if (!c) {
                    break;
                }
            }

            if (!c) {
                break;
            }

            if (count < bufferSize) {
                *dest++ = *src;
                count++;
            }

            src++;

            if (count < bufferSize) {
                *dest++ = *src;
                count++;
            }

            src++;
        } else {
            if (count < bufferSize) {
                *dest++ = c;
                count++;
            }

            src++;
        }

        c = *src;

        if (!c) {
            break;
        }
    }

    if (*src == '[') {
        if (count < bufferSize) {
            *dest++ = '[';
            count++;
        }

        src++;
    }

    char inner[1024];
    char* innerEnd = inner;

    for (uint32_t length = 0; *src && *src != ']' && *src != '|' && length < sizeof(inner) - 1; length++) {
        *innerEnd++ = *src++;
    }

    *innerEnd = '\0';

    uint32_t length = GetDeclinedWord(inner, declension, dest, bufferSize - count);
    dest += length;
    count += length;

    for (c = *src; c; c = *++src) {
        if (count < bufferSize) {
            *dest++ = c;
            count++;
        }
    }

    if (count == bufferSize) {
        dest--;
    }

    *dest = '\0';
    return count;
}
