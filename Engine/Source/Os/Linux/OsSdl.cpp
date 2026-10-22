#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <SDL.h>
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Linux/OsSdl.h"

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

#endif
