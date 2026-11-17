#include "HoT/Streaming/InstallFiles.h"
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "HoT/Platform/Process.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/TextConv.h"

extern char** environ;

namespace fs = std::filesystem;

namespace HoT::Streaming::InstallFiles {

// rename cannot replace a folder that has anything in it, so the old one is moved aside first.
static bool MoveFolder(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    const long long stampMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const fs::path backup = to.string() + ".old-" + std::to_string(stampMs);
    const bool hadOld = fs::exists(fs::symlink_status(to, ec));

    if (hadOld) {
        fs::rename(to, backup, ec);

        if (ec) {
            return false;
        }
    }

    fs::rename(from, to, ec);

    if (ec) {
        if (hadOld) {
            std::error_code ignored;
            fs::rename(backup, to, ignored);
        }

        return false;
    }

    if (hadOld) {
        fs::remove_all(backup, ec);
    }

    return true;
}

// POSIX renames over files that are open or running, so nothing here fails for being in use. A replaced running
// binary only takes effect on the next launch, which is what a staged exe means on Windows.
bool Move(const std::wstring& src, const std::wstring& dst, uint32_t flags, bool& replacedRunningExe) {
    replacedRunningExe = false;
    std::error_code ec;
    const fs::path from(src);
    const fs::path to(dst);

    if (!(flags & MOVE_REPLACE_EXISTING) && fs::exists(to, ec)) {
        return false;
    }

    const bool runningExe = HoldsRunningExe(dst);

    if (fs::is_directory(from, ec)) {
        if (!MoveFolder(from, to)) {
            return false;
        }

        replacedRunningExe = runningExe;
        return true;
    }

    // A download lands as 0644, the file it replaces decides whether it stays executable.
    struct stat old;

    if (stat(to.c_str(), &old) == 0) {
        chmod(from.c_str(), old.st_mode & 07777);
    }

    fs::rename(from, to, ec);

    if (ec == std::errc::cross_device_link && (flags & MOVE_COPY_ALLOWED)) {
        const fs::path tmp = to.wstring() + L".copy";
        ec.clear();
        fs::copy_file(from, tmp, fs::copy_options::overwrite_existing, ec);

        if (!ec) {
            fs::rename(tmp, to, ec);
        }

        if (ec) {
            std::error_code ignored;
            fs::remove(tmp, ignored);
            return false;
        }

        fs::remove(from, ec);
        ec.clear();
    }

    if (ec) {
        return false;
    }

    if (runningExe) {
        replacedRunningExe = true;
    }

    return true;
}

bool HoldsRunningExe(const std::wstring& path) {
    if (Platform::IsRunningExecutable(path)) {
        return true;
    }

    std::error_code ec;
    const fs::path exe = fs::weakly_canonical(Platform::ExecutablePath(), ec);
    const fs::path dir = fs::weakly_canonical(fs::path(path), ec);

    if (exe.empty() || dir.empty()) {
        return false;
    }

    const fs::path rel = exe.lexically_relative(dir);
    return !rel.empty() && *rel.begin() != "..";
}

std::string NarrowPendingPath(const std::wstring& path, bool* lossy) {
    return Text::NarrowUtf8(path);
}

uint32_t PendingCodePage() {
    return 65001;
}

static int32_t s_patchLock = -1;

// An flock on a file inside the install stands in for the install-path named mutex, held for the process lifetime.
bool AcquirePatchLock(const std::wstring& installDir) {
    if (s_patchLock >= 0) {
        return true;
    }

    std::error_code ec;
    fs::create_directories(Paths::WorkDir(), ec);
    const std::string path = (Paths::WorkDir() / "hotstream-patcher.lock").string();
    int32_t fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);

    if (fd < 0) {
        return true;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const bool held = errno == EWOULDBLOCK;
        close(fd);
        return !held;
    }

    s_patchLock = fd;
    return true;
}

// Nothing the client holds open blocks a rename here, so whatever is still pending is retried in process by
// SwapOnClose and no helper has to outlive the client.
void SpawnCloseSwapHelper(const fs::path& pendingFile) {
}

void SpawnExitSwapHelper(const fs::path& pendingFile) {
}

bool IsPackage(const std::string& path, const std::string& platform) {
    static const std::string s_ext = ".tar.gz";
    const std::string lower = Text::LowerAscii(path);
    return platform == ManifestPlatform() && lower.size() > s_ext.size() && lower.compare(lower.size() - s_ext.size(), s_ext.size(), s_ext) == 0;
}

// Both systems ship a tar that reads gzip, and it keeps the modes and symlinks a bundle needs.
bool Unpack(const std::wstring& tarball, const std::wstring& into) {
    std::string archive = fs::path(tarball).string();
    std::string dir = fs::path(into).string();
    char program[] = "tar";
    char extract[] = "-xzf";
    char target[] = "-C";
    char* argv[] = { program, extract, archive.data(), target, dir.data(), nullptr };
    pid_t pid = 0;

    if (posix_spawnp(&pid, program, nullptr, nullptr, argv, environ) != 0) {
        return false;
    }

    int status = 0;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return false;
        }
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}
