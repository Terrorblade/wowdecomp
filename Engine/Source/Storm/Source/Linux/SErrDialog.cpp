#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdio>
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"

namespace StormLinux {

// Storm does not link SDL, so the text goes to stderr and the box's default button is taken.
int32_t ShowErrorDialog(const char* title, const char* text, ErrorDialogButtons buttons, int32_t defaultResult) {
    fprintf(stderr, "\n%s\n%s\n\n", title, text);
    fflush(stderr);

    return defaultResult;
}

}

#endif
