// Event entry points from the game window into the Os input layer (Input.mm). Internal to Os/Mac.

#ifndef ENGINE_SOURCE_OS_MAC_MACINPUT_H
#define ENGINE_SOURCE_OS_MAC_MACINPUT_H

#include <cstdint>

#if defined(__OBJC__)
@class NSEvent;
@class NSView;
#else
typedef struct objc_object NSEvent;
typedef struct objc_object NSView;
#endif

void OsMacInputKeyDown(NSEvent* event);
void OsMacInputKeyUp(NSEvent* event);
void OsMacInputFlagsChanged(NSEvent* event);
void OsMacInputMouseButton(NSView* view, NSEvent* event, bool down);
void OsMacInputMouseMoved(NSView* view, NSEvent* event);
void OsMacInputScrollWheel(NSView* view, NSEvent* event);
void OsMacInputCharacter(uint32_t character);
void OsMacInputFocus(bool active);
void OsMacInputResized();
void OsMacInputClose();
bool OsMacInputCursorDetached();

// IME: text being composed (marked text in Cocoa terms) and the selection inside it, in UTF-16 units.
void OsMacIMEBeginComposition();
void OsMacIMESetComposition(const uint16_t* text, uint32_t length, uint32_t selectionStart, uint32_t selectionLength);
void OsMacIMECommit(const uint16_t* text, uint32_t length);
void OsMacIMEEndComposition();
bool OsMacIMEEnabled();

#endif
