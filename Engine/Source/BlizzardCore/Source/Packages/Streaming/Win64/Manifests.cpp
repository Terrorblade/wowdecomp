
#include <cstdint>
#include <string>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Manifests.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"

// OFFSET: 0x0042E100
void System_Streaming::HttpCallback::Completed(Blizzard::URL::DownloadCallback::Status status) {
    this->m_status = status;
    SetEvent(this->m_event);
}

// OFFSET: 0x00439ED0
System_Streaming::HttpCallback::HttpCallback() {
    this->m_data.reserve(1024);
    this->m_data.erase(this->m_data.begin(), this->m_data.end());
    this->m_received = 0;
    this->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_SUCCESS;
    this->m_timeout = 3000;
    this->m_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
}

// OFFSET: 0x00439F80
System_Streaming::HttpCallback::~HttpCallback() {
    CloseHandle(this->m_event);
}

// OFFSET: 0x004567A0
bool Blizzard::Streaming::FileManifest::ReadIndirectFile(const char* url) {
    System_Streaming::HttpCallback* callback = new System_Streaming::HttpCallback();

    bool result = false;
    uint32_t attempt = 0;

    while (attempt < 5) {
        result = Blizzard::URL::DownloadURL(url, callback);

        if (result) {
            WaitForSingleObject(callback->m_event, INFINITE);

            if (callback->m_status) {
                result = false;
            }
        }

        if (!result) {
            callback->m_data.erase(callback->m_data.begin(), callback->m_data.end());
            callback->m_received = 0;

            if (callback->m_status == Blizzard::URL::DownloadCallback::DOWNLOAD_TIMEOUT) {
                callback->SetTimeOutValue(2 * callback->GetTimeout());
            }
        }

        attempt++;

        if (result) {
            result = ReadDirectFileFromBuffer(const_cast<char*>(callback->m_data.c_str()), callback->m_data.size());
            delete callback;
            return result;
        }
    }

    if (callback->m_status) {
        Blizzard::File::AddToLastErrorStack(callback->m_status, Blizzard::String::QuickFormat<1024>("FileManifest::ReadIndirectFile - DownloadURL failed - %s", url).m_buffer, 20);
    } else {
        Blizzard::File::AddToLastErrorStack(0, "FileManifest::ReadIndirectFile - DownloadURL failed with no error", 20);
    }

    delete callback;
    return result;
}

// OFFSET: 0x00459050
bool Blizzard::Streaming::Transport::ReadIndirectFile(const char* url) {
    System_Streaming::HttpCallback* callback = new System_Streaming::HttpCallback();

    bool result = false;
    uint32_t attempt = 0;

    while (attempt < 5) {
        result = Blizzard::URL::DownloadURL(url, callback);

        if (result) {
            WaitForSingleObject(callback->m_event, INFINITE);

            if (callback->m_status) {
                result = false;
            }
        }

        if (!result) {
            callback->m_data.erase(callback->m_data.begin(), callback->m_data.end());
            callback->m_received = 0;

            if (callback->m_status == Blizzard::URL::DownloadCallback::DOWNLOAD_TIMEOUT) {
                callback->SetTimeOutValue(2 * callback->GetTimeout());
            }
        }

        attempt++;

        if (result) {
            result = ReadDirectFileFromBuffer(const_cast<char*>(callback->m_data.c_str()), callback->m_data.size());
            delete callback;
            return result;
        }
    }

    if (callback->m_status) {
        Blizzard::File::AddToLastErrorStack(callback->m_status, Blizzard::String::QuickFormat<1024>("Transport::ReadIndirectFile - DownloadURL failed - %s", url).m_buffer, 20);
        Blizzard::File::AddToLastErrorStack(callback->m_status, "Failed to load transport manifest.", 20);
    } else {
        Blizzard::File::AddToLastErrorStack(0, "Transport::ReadIndirectFile - DownloadURL failed", 20);
    }

    delete callback;
    return result;
}

// OFFSET: 0x00459400
bool Blizzard::Streaming::SourceManifest::ReadIndirectFile(const char* url) {
    System_Streaming::HttpCallback* callback = new System_Streaming::HttpCallback();

    bool result = false;
    uint32_t attempt = 0;

    while (attempt < 5) {
        result = Blizzard::URL::DownloadURL(url, callback);

        if (result) {
            WaitForSingleObject(callback->m_event, INFINITE);

            if (callback->m_status) {
                result = false;
            }
        }

        if (!result) {
            callback->m_data.erase(callback->m_data.begin(), callback->m_data.end());
            callback->m_received = 0;

            if (callback->m_status == Blizzard::URL::DownloadCallback::DOWNLOAD_TIMEOUT) {
                callback->SetTimeOutValue(2 * callback->GetTimeout());
            }
        }

        attempt++;

        if (result) {
            result = ReadDirectFileFromBuffer(const_cast<char*>(callback->m_data.c_str()), callback->m_data.size());
            delete callback;
            return result;
        }
    }

    if (callback->m_status) {
        Blizzard::File::AddToLastErrorStack(callback->m_status, Blizzard::String::QuickFormat<1024>("SourceManifest::ReadIndirectFile - DownloadURL failed - %s", url).m_buffer, 20);
        Blizzard::File::AddToLastErrorStack(callback->m_status, "Failed to load source manifest.", 20);
    } else {
        Blizzard::File::AddToLastErrorStack(0, "SourceManifest::ReadIndirectFile - DownloadURL failed", 20);
    }

    delete callback;
    return result;
}

// OFFSET: 0x00461430
bool Blizzard::Streaming::ManifestInfo::ReadIndirectFile(const char* url) {
    System_Streaming::HttpCallback* callback = new System_Streaming::HttpCallback();

    bool result = false;
    uint32_t attempt = 0;

    while (attempt < 5) {
        result = Blizzard::URL::DownloadURL(url, callback);

        if (result) {
            WaitForSingleObject(callback->m_event, INFINITE);

            if (callback->m_status) {
                result = false;
            }
        }

        if (!result) {
            callback->m_data.erase(callback->m_data.begin(), callback->m_data.end());
            callback->m_received = 0;

            if (callback->m_status == Blizzard::URL::DownloadCallback::DOWNLOAD_TIMEOUT) {
                callback->SetTimeOutValue(2 * callback->GetTimeout());
            }
        }

        attempt++;

        if (result) {
            result = this->ReadDirectFileFromBuffer(const_cast<char*>(callback->m_data.c_str()), callback->m_data.size());
            delete callback;
            return result;
        }
    }

    if (callback->m_status) {
        Blizzard::File::AddToLastErrorStack(callback->m_status, Blizzard::String::QuickFormat<1024>("FileManifest::ReadIndirectFile - DownloadURL failed - %s", url).m_buffer, 20);
    } else {
        Blizzard::File::AddToLastErrorStack(0, "FileManifest::ReadIndirectFile - DownloadURL failed with no error", 20);
    }

    delete callback;
    return result;
}

