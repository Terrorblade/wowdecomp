// The macOS side of the Os layer that other macOS code builds on.
//
// The Os layer owns the application, the game window and its input, as Win32 code owns its window
// procedure. A graphics device asks for the game window here and renders into the CAMetalLayer of
// its view; it sets the layer's drawable size, which is the client size the rest of the client sees
// (window rect, mouse coordinates, resize events). Everything here runs on the main thread.

#ifndef ENGINE_SOURCE_OS_MAC_MACOS_H
#define ENGINE_SOURCE_OS_MAC_MACOS_H

#include <cstdint>

struct OsMacWindowParams {
    int32_t m_width;
    int32_t m_height;
    bool m_fullscreen;
    bool m_maximize;
};

// Sets up NSApplication for a client that runs its own loop: regular activation policy, the main menu
// and the application delegate. Safe to call more than once.
void OsMacInitApplication();

// Creates (or reconfigures) the game window. Returns the NSWindow, unretained, as the client's window
// handle; its content view is layer-backed by a CAMetalLayer.
void* OsMacCreateGameWindow(const OsMacWindowParams& params, const char* title);
void OsMacDestroyGameWindow(void* window);

// The window's CAMetalLayer, unretained.
void* OsMacGetMetalLayer(void* window);

// Client size in pixels: the drawable size of the window's layer.
void OsMacGetClientSize(void* window, int32_t* width, int32_t* height);

// Fixes the layer's drawable size, the resolution the client renders at, independent of the window
// size; the compositor scales it to the window. 0 x 0 makes it follow the window's size in pixels,
// which is the default.
void OsMacSetDrawableSize(void* window, int32_t width, int32_t height);

// Sets the pointer shown over the game view: an NSCursor (unretained, the caller keeps it), or the
// arrow when null. A hidden pointer shows nothing over the view; elsewhere the system pointer shows.
void OsMacSetGameCursor(void* cursor, bool visible);

// Shows the game cursor again over the game view, after the client hid or replaced it.
void OsMacRefreshGameCursor();

// Bounces the Dock icon once while the game is in the background, as a flashed Windows taskbar button. Any thread.
void OsMacRequestAttention();

// Hands queued Cocoa events to the application without blocking. Returns false when the application
// was asked to quit.
bool OsMacPumpEvents();

#endif
