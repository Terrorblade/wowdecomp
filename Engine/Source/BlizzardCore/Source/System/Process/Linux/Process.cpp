#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Linux/LinuxPath.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/TempBuffer.h"
#include "Engine/Source/BlizzardCore/Source/System/Process/Process.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"

extern char** environ;

struct ChildProcess {
    pid_t m_process;
    void (*m_callback)(void*);
    void* m_param;
};

// Windows polls the children from a 250 ms thread timer on the launching thread's message loop. There is no such loop
// here, so each child with a callback gets a detached thread that waits for it and then runs the callback.
static void* ChildWatcher(void* param) {
    ChildProcess* child = static_cast<ChildProcess*>(param);

    int status;

    while (waitpid(child->m_process, &status, 0) < 0 && errno == EINTR) {
    }

    child->m_callback(child->m_param);
    delete child;

    return nullptr;
}

bool Blizzard::Process::GetProcessPath(char* buffer, int32_t buffersize) {
    if (!buffer || buffersize <= 0) {
        return false;
    }

    char filename[4096];
    ssize_t length = readlink("/proc/self/exe", filename, sizeof(filename) - 1);
    filename[length > 0 ? length : 0] = '\0';
    System_File::FromNativeName(buffer, buffersize, filename);

    return true;
}

bool Blizzard::Process::GetProcessDirectory(char* buffer, int32_t buffersize) {
    Blizzard::Memory::TempBuffer<char, 1024> path(buffersize);

    if (!GetProcessPath(path.m_data, buffersize)) {
        return false;
    }

    if (path.m_data) {
        if (buffer) {
            int32_t length = static_cast<int32_t>(Blizzard::String::FindFilename(path.m_data) - path.m_data + 1);
            Blizzard::String::Copy(buffer, path.m_data, std::min(length, buffersize));
        }
    } else if (buffer && buffersize > 0) {
        *buffer = '\0';
    }

    return true;
}

bool Blizzard::Process::LaunchChildProcessWithArguments(const char* path, const char* arguments, void (*callback)(void*), void* param) {
    if (!arguments) {
        arguments = "";
    }

    // Arguments are '\n'-separated, as on Windows; execve takes them as a vector, so no quoting is needed.
    std::vector<std::string> argStrings;
    argStrings.push_back(path);

    while (*arguments) {
        size_t length = strcspn(arguments, "\n");
        argStrings.push_back(std::string(arguments, length));
        arguments += length;
        arguments += strspn(arguments, "\n");
    }

    char nativePath[4096];

    if (!System_File::LinuxResolvePath(path, nativePath, sizeof(nativePath), false)) {
        return false;
    }

    std::vector<char*> argv;

    for (size_t i = 0; i < argStrings.size(); i++) {
        argv.push_back(const_cast<char*>(argStrings[i].c_str()));
    }

    argv.push_back(nullptr);

    pid_t pid;

    if (posix_spawn(&pid, nativePath, nullptr, nullptr, argv.data(), environ) != 0) {
        return false;
    }

    if (!callback) {
        return true;
    }

    ChildProcess* child = new ChildProcess;
    child->m_process = pid;
    child->m_callback = callback;
    child->m_param = param;

    pthread_t thread;

    if (pthread_create(&thread, nullptr, ChildWatcher, child) != 0) {
        delete child;
        return false;
    }

    pthread_detach(thread);

    return true;
}

void Blizzard::Process::Sleep(uint32_t milliseconds) {
    if (!milliseconds) {
        sched_yield();
        return;
    }

    timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000;

    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
    }
}

#endif
