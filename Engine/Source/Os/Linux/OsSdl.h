#ifndef ENGINE_SOURCE_OS_LINUX_OSSDL_H
#define ENGINE_SOURCE_OS_LINUX_OSSDL_H

#include <cstdint>
#include <SDL.h>

// The Linux client's window is an SDL window, and SDL events stand in for window messages:
//
//  - The Gx device creates the window and registers a window proc for it with OsSdlSetWindowProc, like the
//    Windows device registering WindowProcGl in its window class.
//  - OsInputGet pumps SDL events (the PeekMessage loop) and hands each one to the proc registered for the window it
//    belongs to (DispatchMessage). Events without a window go to OsWindowProc directly.
//  - The device's proc handles window state (size, focus, close) and forwards the event to its m_windowProc, which
//    is OsWindowProc; OsWindowProc turns input into OsQueuePut events, as it does for WM_* messages.
//
// A window proc keeps the Windows signature: window is the SDL_Window*, message is the SDL event type, wparam is 0
// and lparam points to the SDL_Event.
typedef intptr_t (*OsSdlWindowProc)(void* window, uint32_t message, uintptr_t wparam, intptr_t lparam);

void OsSdlSetWindowProc(SDL_Window* window, OsSdlWindowProc proc);
OsSdlWindowProc OsSdlGetWindowProc(SDL_Window* window);

// Dispatches one event as described above; OsInputGet calls it for every event it pumps.
intptr_t OsSdlDispatchEvent(const SDL_Event* event);

#endif
