#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <SDL.h>
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Linux/OsSdl.h"

// waystone.ico, as is (the Windows build has it as the BlizzardIcon.ico resource). ELF has no resource section, so
// the bytes go into .rodata; the path comes from CMake.
asm(".pushsection .rodata\n"
    ".global OsSdlIconData\n"
    ".hidden OsSdlIconData\n"
    "OsSdlIconData:\n"
    ".incbin \"" WOWDECOMP_ICON_FILE "\"\n"
    ".global OsSdlIconDataEnd\n"
    ".hidden OsSdlIconDataEnd\n"
    "OsSdlIconDataEnd:\n"
    ".popsection\n");

extern "C" const uint8_t OsSdlIconData[];
extern "C" const uint8_t OsSdlIconDataEnd[];

static const char* const s_windowProcKey = "OsSdlWindowProc";

void OsSdlSetWindowProc(SDL_Window* window, OsSdlWindowProc proc) {
    if (!window) {
        return;
    }

    SDL_SetWindowData(window, s_windowProcKey, reinterpret_cast<void*>(proc));
}

OsSdlWindowProc OsSdlGetWindowProc(SDL_Window* window) {
    if (!window) {
        return nullptr;
    }

    return reinterpret_cast<OsSdlWindowProc>(SDL_GetWindowData(window, s_windowProcKey));
}

static uint32_t GetEventWindowID(const SDL_Event* event) {
    switch (event->type) {
        case SDL_WINDOWEVENT:
            return event->window.windowID;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return event->key.windowID;

        case SDL_TEXTEDITING:
            return event->edit.windowID;

        case SDL_TEXTEDITING_EXT:
            return event->editExt.windowID;

        case SDL_TEXTINPUT:
            return event->text.windowID;

        case SDL_MOUSEMOTION:
            return event->motion.windowID;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return event->button.windowID;

        case SDL_MOUSEWHEEL:
            return event->wheel.windowID;

        case SDL_DROPFILE:
        case SDL_DROPTEXT:
        case SDL_DROPBEGIN:
        case SDL_DROPCOMPLETE:
            return event->drop.windowID;

        default:
            if (event->type >= SDL_USEREVENT && event->type < SDL_LASTEVENT) {
                return event->user.windowID;
            }

            return 0;
    }
}

intptr_t OsSdlDispatchEvent(const SDL_Event* event) {
    if (!event) {
        return 0;
    }

    uint32_t windowID = GetEventWindowID(event);
    SDL_Window* window = windowID ? SDL_GetWindowFromID(windowID) : nullptr;
    OsSdlWindowProc proc = OsSdlGetWindowProc(window);
    intptr_t lparam = reinterpret_cast<intptr_t>(event);

    if (proc) {
        return proc(window, event->type, 0, lparam);
    }

    return OsWindowProc(window, event->type, 0, lparam);
}

// The client's desktop identity: the .desktop file and icon names, and the window's app_id (Wayland) / WM_CLASS (X11).
static const char* const s_appId = "HoTClient64";

static void WriteIfChanged(const std::filesystem::path& path, const std::string& data) {
    std::ifstream in(path, std::ios::binary);
    std::string current((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (in.is_open() && current == data) return;
    std::filesystem::create_directories(path.parent_path());

    // Written aside and renamed in, so the desktop never reads half a file.
    std::filesystem::path tmp = path;
    tmp += ".tmp";

    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out.write(data.data(), data.size());
    if (!out) { std::filesystem::remove(tmp); return; }
    std::filesystem::rename(tmp, path);
}

// A quoted Exec argument escapes ", `, $ and \ with a backslash, then the value's own string escaping doubles every
// backslash again; % is the field code prefix.
static std::string QuoteExecArg(const std::string& arg) {
    std::string out = "\"";

    for (char c : arg) {
        switch (c) {
            case '"':
            case '`':
            case '$':
                out += "\\\\";
                out += c;
                break;

            case '\\':
                out += "\\\\\\\\";
                break;

            case '%':
                out += "%%";
                break;

            default:
                out += c;
                break;
        }
    }

    return out + "\"";
}

static std::string EscapeString(const std::string& value) {
    std::string out;
    for (char c : value) out += c == '\\' ? "\\\\" : std::string(1, c);
    return out;
}

void OsSdlRegisterDesktopEntry() {
    // Real SDL2 reads the WMCLASS variables when video starts, SDL3 (sdl2-compat included) reads SDL_APP_ID when a
    // window is created. Values the user set win.
    setenv("SDL_VIDEO_X11_WMCLASS", s_appId, 0);
    setenv("SDL_VIDEO_WAYLAND_WMCLASS", s_appId, 0);
    setenv("SDL_APP_ID", s_appId, 0);

    std::filesystem::path dataHome;
    const char* xdgDataHome = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");

    if (xdgDataHome && xdgDataHome[0] == '/')
        dataHome = xdgDataHome;
    else if (home && home[0] == '/')
        dataHome = std::filesystem::path(home) / ".local" / "share";
    else return;

    const std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe");
    const std::string exePath = exe.string();

    // A line break would end the Exec line early.
    if (exePath.empty() || exePath.find_first_of("\r\n") != std::string::npos) return;

    // The largest entry of waystone.ico. ICONDIR is reserved, type, count, then one 16 byte ICONDIRENTRY per image:
    // width in the first byte (0 means 256), size and offset in the last eight. Its entries are PNG files as is.
    const uint8_t* ico = OsSdlIconData;
    const size_t icoSize = OsSdlIconDataEnd - OsSdlIconData;
    const uint32_t count = icoSize >= 6 ? ico[4] | (ico[5] << 8) : 0;
    const uint8_t* best = nullptr;
    uint32_t bestSize = 0;

    for (uint32_t i = 0; i < count && 6 + (i + 1) * 16 <= icoSize; i++) {
        const uint8_t* entry = ico + 6 + i * 16;
        const uint32_t size = entry[0] ? entry[0] : 256;

        if (size > bestSize) {
            best = entry;
            bestSize = size;
        }
    }

    if (best) {
        const uint32_t length = best[8] | (best[9] << 8) | (best[10] << 16) | (uint32_t(best[11]) << 24);
        const uint32_t offset = best[12] | (best[13] << 8) | (best[14] << 16) | (uint32_t(best[15]) << 24);

        if (offset <= icoSize && length <= icoSize - offset && length >= 8 && memcmp(ico + offset, "\x89PNG", 4) == 0) {
            const std::string size = std::to_string(bestSize);
            const std::filesystem::path icon = dataHome / "icons" / "hicolor" / (size + "x" + size) / "apps" / (std::string(s_appId) + ".png");
            try { WriteIfChanged(icon, std::string(reinterpret_cast<const char*>(ico + offset), length)); }
                catch (const std::filesystem::filesystem_error& e) {}
        }
    }

    // Rewritten only when it changes, i.e. when the install moved. NoDisplay keeps it out of menus; the window is
    // still matched to it by app_id / StartupWMClass.
    const std::string entry = std::string("[Desktop Entry]\n")
        + "# Written by the game at startup so the desktop shows its icon.\n"
        + "Type=Application\n"
        + "Name=Hour of Twilight\n"
        // GLib checks that the program exists before expanding %% and drops the entry if not, so a path with a % goes through env.
        + "Exec=" + (exePath.find('%') != std::string::npos ? "env " : "") + QuoteExecArg(exePath) + "\n"
        + "Path=" + EscapeString(exe.parent_path().string()) + "\n"
        + "Icon=" + s_appId + "\n"
        + "StartupWMClass=" + s_appId + "\n"
        + "Terminal=false\n"
        + "NoDisplay=false\n"; // set to true to prevent `Hour of Twilight` from appearing in the application menu.

    try { WriteIfChanged(dataHome / "applications" / (std::string(s_appId) + ".desktop"), entry); }
        catch (const std::filesystem::filesystem_error& e) {}
}

#endif
