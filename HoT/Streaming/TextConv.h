#ifndef HOT_STREAMING_TEXTCONV_H
#define HOT_STREAMING_TEXTCONV_H

#include <cstdint>
#include <string>

namespace HoT::Streaming::Text {

std::wstring WidenAcp(const std::string& s);
std::string NarrowAcp(const std::wstring& s);
std::wstring WidenUtf8(const std::string& s);
std::string NarrowUtf8(const std::wstring& s);

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
