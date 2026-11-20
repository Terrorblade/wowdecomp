#ifndef HOT_EDITOR_MODE_H
#define HOT_EDITOR_MODE_H

#include <cstdint>
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Tempest/C3Vector.h"

class CGWorldFrame;

namespace HoT::Editor::Mode {

// World editors implement this and register once, every call only happens while the editor is active.
class Handler {
    public:
    virtual ~Handler() = default;

    // Tool names this handler owns, "map." owns every map tool. Input only reaches the owner of the active tool.
    virtual bool OwnsTool(const char* tool) const = 0;

    // Mouse coordinates are DDC. Return true to consume the event.
    virtual bool OnMouseDown(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) { return false; }
    virtual bool OnMouseUp(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) { return false; }
    virtual bool OnMouseWheel(int32_t delta, uint32_t metaKeys) { return false; }

    // Key strings are binding style ("CTRL-Z", "DELETE"). Return true to consume.
    virtual bool OnKey(const char* key, bool down) { return false; }

    // Every frame while the editor is active, also for handlers that do not own the active tool.
    virtual void OnUpdate(float seconds) {}
    virtual void OnWorldRender(CGWorldFrame* frame) {}

    // The spell cursor decal, a handler may point it at a brush. Style 3 hides it.
    virtual void OnDecal(C3Vector& position, float& size, int32_t& style) {}

    virtual void OnActivate() {}
    virtual void OnDeactivate() {}
    virtual void OnToolChanged(const char* tool) {}

    // The world was torn down (logout, teleport to another map), drop every pointer into it.
    virtual void OnWorldUnloaded() {}
};

void RegisterHandler(Handler* handler);

bool IsActive();

// The mini editor keeps the game UI, camera and controls, only Ctrl hotkeys reach the editor.
bool IsMini();

// Switching between the full and the mini editor while open keeps every editor window as it was.
void SetActive(bool active, bool mini = false);

const char* ActiveTool();
bool ToolIs(const char* name);
bool ToolHasPrefix(const char* prefix);

// Mouse position in DDC and whether it is over the world frame rather than UI.
void GetMouse(float& x, float& y);
bool IsMouseOverWorld();
bool IsMouseButtonDown(MOUSEBUTTON button);
// Raw button state, also counts presses that landed on UI frames.
bool IsMouseButtonHeld(MOUSEBUTTON button);

// World ray under the mouse, false when there is no world camera.
bool GetMouseRay(C3Vector& start, C3Vector& end);

// Hooks called from CGWorldFrame, they return true when the editor consumed the input.
bool OnWorldKey(const char* key, uint32_t metaKeys, bool down);
bool OnWorldMouse(MOUSEBUTTON button, uint32_t metaKeys, float ndcX, float ndcY, bool down);
bool OnWorldMouseWheel(int32_t delta, uint32_t metaKeys);

void OnDecal(C3Vector& position, float& size, int32_t& style);
void OnWorldRender(CGWorldFrame* frame);

void RegisterScripts();
void OnGameInitialize();
void OnGameDestroy();

// A loading screen started, the world is about to go away.
void OnWorldUnloaded();

// Every frame, reopens an editor a loading screen closed.
void OnIdle();

// The game Lua state was rebuilt, the UI frames are gone.
void OnScriptsReloaded();

}

#endif
