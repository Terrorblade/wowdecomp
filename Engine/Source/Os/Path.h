#ifndef ENGINE_SOURCE_OS_PATH_H
#define ENGINE_SOURCE_OS_PATH_H

#include <cstdint>

class CommandLineUTF8 {
    public:
    /* 0000 */ char* m_commandLine;

    CommandLineUTF8();
    ~CommandLineUTF8();
};

uint32_t GetExecutablePathUTF8(char* path, uint32_t size);
uint32_t GetModuleFileNameUTF8(void* module, char* path, uint32_t size);
char* IOsPathFindExtension(char* path, int32_t offset);
void OsBuildFontFilePath(const char* fontName, char* path, uint32_t size);
void OsGetExePath(char* path, uint32_t size);
// The folder holding Data and WTF, with a trailing separator. On macOS that is the folder around the .app bundle.
void OsGetGameDirectory(char* path, uint32_t size);
char* OsPathFindExtensionWithoutDot(char* path);
uint32_t OsPathGetRootChars(const char* path);
void OsPathStripFilename(char* path);

#endif
