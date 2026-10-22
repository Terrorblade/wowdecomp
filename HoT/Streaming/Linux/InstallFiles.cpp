#include "HoT/Streaming/InstallFiles.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include "HoT/Platform/Process.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/TextConv.h"

namespace fs = std::filesystem;

namespace HoT::Streaming::InstallFiles {

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

    const bool runningExe = Platform::IsRunningExecutable(to);
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

}
