#include "ChecksumExecutables.h"
#include <cstring>
#include "Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.h"
#include "Engine/Source/Os/OsVersionHash.h"
#include "WoW/Source/WowServices/HMAC.h"

enum CHECKSUM_FILE_TYPE : int32_t {
    CHECKSUM_FILE_NONE = 0,
    CHECKSUM_FILE_ON_DISK = 1,
    CHECKSUM_FILE_LOADED = 2,
};

struct ChecksumFile {
    /* 0000 */ const char* m_path;
    /* 0004 */ int32_t m_type;
};

// STATIC: 0x00AD96E8
static ChecksumFile s_files[3] = { { nullptr, 2 }, { "DivxDecoder.dll", 2 }, { "unicows.dll", 1 } };

// VERIFY: b726580e manual
// OFFSET: 0x006B9CF0
void ChecksumExecutables(const uint8_t* challenge, uint32_t challengeLength, uint8_t* checksum) {
    HMAC<SHA1> hmac;
    hmac.Init(challenge, challengeLength);

    for (uint32_t i = 0; i < sizeof(s_files) / sizeof(s_files[0]); i++) {
        if (s_files[i].m_type) {
            IOsVersionHashCallback2<HMAC<SHA1>> callback(&hmac);
            IOsVersionChecksum(callback, s_files[i].m_path, s_files[i].m_type == CHECKSUM_FILE_LOADED);
        }
    }

    SHA1::Digest digest;
    hmac.Finish(digest);
    memcpy(checksum, &digest, sizeof(digest));
}
