#ifndef ENGINE_SOURCE_OS_LINUX_OSIMESDL_H
#define ENGINE_SOURCE_OS_LINUX_OSIMESDL_H

#include <cstdint>
#include <SDL.h>

// SDL text editing events stand in for the WM_IME_* composition messages. OsWindowProc hands SDL_TEXTEDITING,
// SDL_TEXTEDITING_EXT and SDL_TEXTINPUT to this; it queues the same OS_INPUT_IME events the Windows proc does and
// returns 1 when the event was consumed as composition (a TEXTINPUT that commits a composition), in which case the
// caller must not also post the text as OS_INPUT_CHAR.
int32_t OsIMEProcessSdlEvent(const SDL_Event* event);

#endif
