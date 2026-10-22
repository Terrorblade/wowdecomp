#include "HoT/Streaming/HttpClient.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include "HoT/Streaming/HttpTransport.h"

namespace HoT::Streaming {

bool HashPrefix(const std::wstring& path, long long count, Sha256Stream& hash) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);

    if (!in) {
        return false;
    }

    std::vector<char> buf(1 << 20);

    while (count > 0) {
        std::streamsize want = static_cast<std::streamsize>(std::min<long long>(count, static_cast<long long>(buf.size())));
        in.read(buf.data(), want);
        std::streamsize got = in.gcount();

        if (got <= 0) {
            return false;
        }

        hash.Update(buf.data(), static_cast<size_t>(got));
        count -= got;
    }

    return true;
}

DownloadThrottle::DownloadThrottle(std::function<long long()> bytesPerSecond) : m_bytesPerSecond(std::move(bytesPerSecond)), m_start(std::chrono::steady_clock::now()) {
}

void DownloadThrottle::Account(long long bytes) {
    double sleepSec = 0;

    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        long long cap = this->m_bytesPerSecond ? this->m_bytesPerSecond() : 0;

        if (cap != this->m_limit) {
            this->m_limit = cap;
            this->m_bytes = 0;
            this->m_start = std::chrono::steady_clock::now();
        }

        if (this->m_limit <= 0) {
            return;
        }

        this->m_bytes += bytes;
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - this->m_start).count();
        double aheadSec = static_cast<double>(this->m_bytes) / this->m_limit - elapsed;

        if (aheadSec > 0.005) {
            sleepSec = aheadSec < 0.5 ? aheadSec : 0.5;
        }
    }

    if (sleepSec > 0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(sleepSec));
    }
}

}
