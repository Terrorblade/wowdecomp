#include "Engine/Source/Os/OsAutorelease.h"

extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void* pool);

void* OsAutoreleasePush() {
    return objc_autoreleasePoolPush();
}

void OsAutoreleasePop(void* pool) {
    objc_autoreleasePoolPop(pool);
}
