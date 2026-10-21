#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdio>
#include <SDL.h>
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Storm/H/storm.h"

// There are no native dialogs, menus or tooltips on Linux: the window is the SDL window the Gx device created, and
// the COsDialog/COsControl helpers keep their portable logic but never find a native control. OsGuiProcessMessage
// takes the SDL_Event that OsInputGet pumped, where Windows takes an MSG.

static SDL_Window* s_GxDevWindow;

static TSGrowableArray<COsDialog*> s_dialogs;

// INLINED
int32_t COsDialog::OnEvent(int32_t itemId, int32_t notifyCode, int32_t code) {
    if (this->m_callback) {
        OsGuiCallbackParams params;
        params.type = itemId;
        params.subType = notifyCode;
        params.code = code;
        params.user = this->m_callbackParam;
        this->m_callback(params);
    }

    return 1;
}

int32_t COsControl::SendEvent(int32_t event, int32_t code) {
    if (this->OnEvent(this->m_id, event, code)) {
        return 1;
    }

    if (this->m_dialog) {
        return this->m_dialog->OnEvent(this->m_id, event, code);
    }

    return 0;
}

static int32_t sIsCharacterAllowed(char c, uint32_t filters) {
    uint8_t ch = static_cast<uint8_t>(c);

    if (ch <= 0x1F) {
        return 1;
    }

    if (filters & 0x1) {
        if (ch >= 'a' && ch <= 'z') {
            return 1;
        }

        if (ch >= 'A' && ch <= 'Z') {
            return 1;
        }
    }

    if ((filters & 0x2) && ch >= '0' && ch <= '9') {
        return 1;
    }

    if ((filters & 0x4) && (ch == '+' || ch == '-')) {
        return 1;
    }

    if ((filters & 0x8) && ch == '.') {
        return 1;
    }

    if ((filters & 0x10) && ch == ' ') {
        return 1;
    }

    if ((filters & 0x20) && ch == '_') {
        return 1;
    }

    return (filters & 0x40) && ch <= 0x7E;
}

void* COsTreeView::GetEditControl() {
    // No native tree view, so never an in-place label editor.
    return nullptr;
}

int32_t COsTreeView::IsCharacterAllowed(char c) {
    if (!this->GetEditControl()) {
        return 1;
    }

    if (this->m_filtersEnabled) {
        return sIsCharacterAllowed(c, this->m_filters);
    }

    return 1;
}

void OsGuiSetWindowTitle(void* window, const char* title) {
    if (!window) {
        return;
    }

    SDL_SetWindowTitle(static_cast<SDL_Window*>(window), title ? title : "");
}

void* OsGuiGetWindow(int32_t type) {
    switch (type) {
        case 0:
            return s_GxDevWindow;

        case 1:
        case 2:
            // Active and foreground window: the window with keyboard focus, if it is ours.
            return SDL_GetKeyboardFocus();

        default:
            return nullptr;
    }
}

void OsGuiSetGxWindow(void* window) {
    s_GxDevWindow = static_cast<SDL_Window*>(window);
}

int32_t OsGuiMessageBox(void* parentWindow, int32_t style, const char* message, const char* title) {
    if (!message) {
        message = "";
    }

    if (!title) {
        title = "";
    }

    // Button ids are the values returned: OK/Yes = 0, No = 1, Cancel (or closing the box) = 2.
    static const SDL_MessageBoxButtonData okButtons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "OK" },
    };

    static const SDL_MessageBoxButtonData okCancelButtons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel" },
    };

    static const SDL_MessageBoxButtonData yesNoButtons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Yes" },
        { 0, 1, "No" },
    };

    static const SDL_MessageBoxButtonData yesNoCancelButtons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Yes" },
        { 0, 1, "No" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel" },
    };

    SDL_MessageBoxData data = {};
    data.flags = SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    data.window = static_cast<SDL_Window*>(parentWindow);
    data.title = title;
    data.message = message;

    switch (style) {
        case 1:
            data.buttons = okCancelButtons;
            data.numbuttons = 2;
            break;

        case 2:
            data.buttons = yesNoButtons;
            data.numbuttons = 2;
            break;

        case 3:
            data.buttons = yesNoCancelButtons;
            data.numbuttons = 3;
            break;

        case 0:
        default:
            data.buttons = okButtons;
            data.numbuttons = 1;
            break;
    }

    int32_t buttonId = -1;

    if (SDL_ShowMessageBox(&data, &buttonId) < 0) {
        // No display to show it on: keep the message, and fail like MessageBoxW does.
        fprintf(stderr, "%s: %s\n", title, message);
        return 2;
    }

    switch (buttonId) {
        case 0:
            return 0;

        case 1:
            return 1;

        default:
            return 2;
    }
}

int32_t OsGuiIsModifierKeyDown(int32_t key) {
    SDL_Keymod mod = SDL_GetModState();

    switch (key) {
        case 0:
            return (mod & KMOD_CTRL) != 0;

        case 1:
            return (mod & KMOD_SHIFT) != 0;

        case 2:
            return (mod & KMOD_ALT) != 0;

        default:
            return 0;
    }
}

void OsGuiBeep() {
    // SDL has no system beep.
}

int32_t COsEditBox::IsCharacterAllowed(char c) {
    if (!this->m_filtersEnabled) {
        return 1;
    }

    return sIsCharacterAllowed(c, this->m_filters);
}

int32_t COsTabControl::GetNumItems() {
    // No native tab control to ask.
    return 0;
}

void* COsDialog::GetParentWindow() {
    return nullptr;
}

int32_t COsDialog::HasFlag(uint32_t flag) {
    return (this->m_flags & flag) != 0;
}

int32_t COsDialog::IsInFront() {
    return this->m_handle == OsGuiGetWindow(2);
}

COsControl* COsDialog::FindControl(void* handle) {
    for (uint32_t i = 0; i < this->m_controls.Count(); i++) {
        if (this->m_controls[i]->IsHandleFromControl(handle)) {
            return this->m_controls[i];
        }
    }

    return nullptr;
}

int32_t COsDialog::OnCancel() {
    if (!this->m_cancelButton || this->m_cancelButton->OnEvent(this->m_cancelButton->m_id, 0, 0)) {
        return 1;
    }

    return this->OnEvent(this->m_cancelButton->m_id, 0, 0);
}

int32_t COsTabControl::OnControlTab() {
    int32_t shift = OsGuiIsModifierKeyDown(1);
    int32_t value = this->GetValue();
    int32_t count = this->GetNumItems();

    int32_t next = shift ? value - 1 : value + 1;

    if (next < 0) {
        next = count - 1;
    }

    if (next >= count) {
        next = 0;
    }

    this->SetValue(next);
    this->SendEvent(2, 0);

    return 1;
}

int32_t COsDialog::OnControlTab() {
    int32_t count = this->m_controls.Count();

    for (int32_t i = 0; i < count; i++) {
        if (this->m_controls[i]->m_type == 15) {
            COsTabControl* tabControl = static_cast<COsTabControl*>(this->m_controls[i]);
            return tabControl ? tabControl->OnControlTab() : 0;
        }
    }

    return 0;
}

static SDL_Window* sGetEventWindow(const SDL_Event* event) {
    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return SDL_GetWindowFromID(event->key.windowID);

        case SDL_TEXTINPUT:
            return SDL_GetWindowFromID(event->text.windowID);

        default:
            return nullptr;
    }
}

int32_t COsDialog::ProcessMessage(void* message) {
    const SDL_Event* event = static_cast<const SDL_Event*>(message);
    SDL_Window* window = sGetEventWindow(event);

    // Escape in a multi-line edit box cancels the dialog on Windows; there are no native multi-line edit boxes.

    if (event->type == SDL_TEXTINPUT) {
        COsControl* control = this->FindControl(window);
        if (control) {
            char c = event->text.text[0];
            int32_t allowed = 1;

            if (control->m_type == 4) {
                allowed = static_cast<COsEditBox*>(control)->IsCharacterAllowed(c);
            } else if (control->m_type == 10) {
                allowed = static_cast<COsTreeView*>(control)->IsCharacterAllowed(c);
            }

            if (!allowed) {
                OsGuiBeep();
                return 1;
            }
        }
    }

    if (event->type == SDL_KEYDOWN && (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER)) {
        COsControl* control = this->FindControl(window);
        if (control && (control->m_type == 6 || control->m_type == 10 || control->m_type == 16) && control->OnReturn()) {
            return 1;
        }
    }

    if (this->IsInFront() && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_TAB && OsGuiIsModifierKeyDown(0) && this->OnControlTab()) {
        return 1;
    }

    // IsDialogMessage: no native dialog navigation.
    return 0;
}

int32_t OsGuiProcessMessage(void* message) {
    if (!message) {
        return 0;
    }

    // Native menu bar accelerators and tooltips don't exist here; only registered dialogs see the event.
    for (uint32_t i = 0; i < s_dialogs.Count(); i++) {
        if (s_dialogs[i]->ProcessMessage(message)) {
            return 1;
        }
    }

    return 0;
}

#endif
