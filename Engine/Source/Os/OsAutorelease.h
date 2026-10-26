#ifndef ENGINE_SOURCE_OS_OSAUTORELEASE_H
#define ENGINE_SOURCE_OS_OSAUTORELEASE_H

// An Objective-C autorelease pool on macOS, nothing elsewhere. Lets C++ loops drain the Metal and AppKit objects
// they create each iteration.
void* OsAutoreleasePush();
void OsAutoreleasePop(void* pool);

class OsAutoreleaseScope {
    public:
    OsAutoreleaseScope() : m_pool(OsAutoreleasePush()) {}
    ~OsAutoreleaseScope() { OsAutoreleasePop(this->m_pool); }
    OsAutoreleaseScope(const OsAutoreleaseScope&) = delete;
    OsAutoreleaseScope& operator=(const OsAutoreleaseScope&) = delete;

    private:
    void* m_pool;
};

#endif
