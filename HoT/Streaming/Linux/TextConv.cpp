#include "HoT/Streaming/TextConv.h"

namespace HoT::Streaming::Text {

// Off Windows wchar_t is UTF-32 and every code page is treated as UTF-8.
static std::wstring Decode(const std::string& s) {
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

static std::string Encode(const std::wstring& s) {
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

std::wstring WidenAcp(const std::string& s) {
    return Decode(s);
}

std::string NarrowAcp(const std::wstring& s) {
    return Encode(s);
}

std::wstring WidenUtf8(const std::string& s) {
    return Decode(s);
}

std::string NarrowUtf8(const std::wstring& s) {
    return Encode(s);
}

}
