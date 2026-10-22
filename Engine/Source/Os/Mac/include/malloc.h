#ifndef ENGINE_OS_MAC_INCLUDE_MALLOC_H
#define ENGINE_OS_MAC_INCLUDE_MALLOC_H

// macOS has no <malloc.h>. The code includes it for alloca and the malloc family.

#include <alloca.h>
#include <malloc/malloc.h>
#include <stdlib.h>

#endif
