#ifndef ENGINE_OS_MAC_COMPATOS_H
#define ENGINE_OS_MAC_COMPATOS_H

// Force-included after Linux/Compat.h on macOS.

#include <malloc/malloc.h>

#define _msize malloc_size

// macOS reports a peer's shutdown through POLLHUP only.
#define POLLRDHUP 0

#endif
