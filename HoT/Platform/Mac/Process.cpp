#include "HoT/Platform/Process.h"
#include <climits>
#include <cstdlib>
#include <utility>
#include <vector>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <sys/stat.h>

namespace HoT::Platform {

std::filesystem::path ExecutablePath() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');

    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return std::filesystem::path();
    }

    char resolved[PATH_MAX];

    if (!realpath(buffer.data(), resolved)) {
        return std::filesystem::path(buffer.data());
    }

    return std::filesystem::path(resolved);
}

// Inside HoTClient64.app/Contents/MacOS the game data sits next to the bundle, like the original Mac client.
std::filesystem::path GameDirectory() {
    std::filesystem::path exe = ExecutablePath();

    if (exe.empty()) {
        std::error_code ec;
        return std::filesystem::current_path(ec);
    }

    std::filesystem::path folder = exe.parent_path();
    std::filesystem::path contents = folder.parent_path();
    std::filesystem::path bundle = contents.parent_path();

    if (folder.filename() == "MacOS" && contents.filename() == "Contents" && bundle.extension() == ".app") {
        return bundle.parent_path();
    }

    return folder;
}

// The running image is identified by the inode it had on first use, a file renamed over it later is not the running exe.
bool IsRunningExecutable(const std::filesystem::path& path) {
    static const std::pair<dev_t, ino_t> s_image = [] {
        struct stat info;
        std::filesystem::path exe = ExecutablePath();

        if (exe.empty() || stat(exe.c_str(), &info) != 0) {
            return std::pair<dev_t, ino_t>(0, 0);
        }

        return std::pair<dev_t, ino_t>(info.st_dev, info.st_ino);
    }();

    struct stat info;

    if (!s_image.second || stat(path.c_str(), &info) != 0) {
        return false;
    }

    return info.st_dev == s_image.first && info.st_ino == s_image.second;
}

uint64_t CurrentThreadId() {
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return tid;
}

// macOS has no per thread nice value, quality of service classes take its place.
void LowerThreadPriority() {
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
}

void SetBackgroundThreadPriority() {
    pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
}

bool ProcessMemory(uint64_t& workingSet, uint64_t& privateBytes) {
    task_vm_info_data_t info = {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return false;
    }

    workingSet = info.resident_size;
    privateBytes = info.phys_footprint;
    return true;
}

}
