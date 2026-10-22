#include "HoT/Streaming/TextConv.h"
#include <windows.h>

namespace HoT::Streaming::Text {

static std::wstring Widen(UINT codePage, const std::string& s) {
    if (s.empty()) {
        return L"";
    }

    int32_t n = MultiByteToWideChar(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), out.data(), n);
    return out;
}

static std::string Narrow(UINT codePage, const std::wstring& s) {
    if (s.empty()) {
        return "";
    }

    int32_t n = WideCharToMultiByte(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring WidenAcp(const std::string& s) {
    return Widen(CP_ACP, s);
}

std::string NarrowAcp(const std::wstring& s) {
    return Narrow(CP_ACP, s);
}

std::wstring WidenUtf8(const std::string& s) {
    return Widen(CP_UTF8, s);
}

std::string NarrowUtf8(const std::wstring& s) {
    return Narrow(CP_UTF8, s);
}

}
