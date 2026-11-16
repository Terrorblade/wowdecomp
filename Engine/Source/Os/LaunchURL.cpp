#include <cstdint>
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"

// Scripts reach OsLaunchURL, so only web links go to the shell. Anything else could name a local program or file,
// and spaces or quotes could add arguments to the browser command line.
int32_t OsIsSafeLaunchURL(const char* url) {
    if (!url) {
        return 0;
    }

    if (SStrCmpI(url, "http://", 7) && SStrCmpI(url, "https://", 8)) {
        return 0;
    }

    for (const uint8_t* c = reinterpret_cast<const uint8_t*>(url); *c; c++) {
        if (*c <= ' ' || *c == 0x7F || SStrChr("\"'<>\\^`|{}", static_cast<char>(*c))) {
            return 0;
        }
    }

    return 1;
}
