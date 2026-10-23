#ifndef ENGINE_SOURCE_OS_OSMEMORY_H
#define ENGINE_SOURCE_OS_OSMEMORY_H

#include <cstddef>

void* OsAllocModel(size_t size);
void* OsAllocNear(const void* anchor, size_t size);
bool OsIsNear(const void* ptr);
void OsFreeNear(void* ptr);

#endif
