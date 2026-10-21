#ifndef HOT_STREAMING_TEXTCONV_H
#define HOT_STREAMING_TEXTCONV_H

#if defined(_WIN32)
#include <windows.h>
#endif
#include <cstdint>
#include <string>

namespace HoT::Streaming::Text {

#if defined(_WIN32)
inline std::wstring Widen(UINT codePage, const std::string& s) {
    if (s.empty()) {
        return L"";
    }

    int32_t n = MultiByteToWideChar(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), out.data(), n);
    return out;
}

inline std::string Narrow(UINT codePage, const std::wstring& s) {
    if (s.empty()) {
        return "";
    }

    int32_t n = WideCharToMultiByte(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(codePage, 0, s.c_str(), static_cast<int32_t>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

#else
// Off Windows wchar_t is UTF-32 and every code page is treated as UTF-8.
inline std::wstring Widen(UINT codePage, const std::string& s) {
    (void)codePage;
    std::wstring out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size();) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        uint32_t cp;
        size_t len;

        if (c < 0x80) {
            cp = c;
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
        } else {
            out.push_back(0xFFFD);
            i++;
            continue;
        }

        if (i + len > s.size()) {
            out.push_back(0xFFFD);
            break;
        }

        for (size_t j = 1; j < len; j++) {
            cp = (cp << 6) | (static_cast<uint8_t>(s[i + j]) & 0x3F);
        }

        out.push_back(static_cast<wchar_t>(cp));
        i += len;
    }

    return out;
}

inline std::string Narrow(UINT codePage, const std::wstring& s) {
    (void)codePage;
    std::string out;
    out.reserve(s.size());

    for (wchar_t wc : s) {
        uint32_t cp = static_cast<uint32_t>(wc);

        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    return out;
}

#endif

inline std::wstring WidenAcp(const std::string& s) {
    return Widen(CP_ACP, s);
}

inline std::string NarrowAcp(const std::wstring& s) {
    return Narrow(CP_ACP, s);
}

inline std::wstring WidenUtf8(const std::string& s) {
    return Widen(CP_UTF8, s);
}

inline std::string NarrowUtf8(const std::wstring& s) {
    return Narrow(CP_UTF8, s);
}

inline std::wstring LowerPath(std::wstring p) {
    for (wchar_t& c : p) {
        if (c == L'/') {
            c = L'\\';
        } else if (c >= L'A' && c <= L'Z') {
            c = static_cast<wchar_t>(c + 32);
        }
    }

    return p;
}

inline std::string LowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + 32);
        }
    }

    return s;
}

}

#endif
