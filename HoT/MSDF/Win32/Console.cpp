#include "HoT/MSDF/Console.h"
#include <cstdio>
#include <windows.h>

namespace HoT::MSDF::Console {

struct State {
    FILE* m_out = nullptr;
    FILE* m_in = nullptr;
    bool m_allocated = false;
    HWND m_window = nullptr;
};

static HANDLE s_lockFile = INVALID_HANDLE_VALUE;

Session::Session() {
    State* state = new State();
    this->m_state = state;

    state->m_window = GetActiveWindow();
    state->m_allocated = AllocConsole() || GetLastError() == ERROR_ACCESS_DENIED;

    if (state->m_allocated) {
        freopen_s(&state->m_out, "CONOUT$", "w", stdout);
        freopen_s(&state->m_in, "CONIN$", "r", stdin);

        if (state->m_window) {
            ShowWindow(state->m_window, SW_MINIMIZE);
        }

        SetForegroundWindow(GetConsoleWindow());
    }

    this->m_open = state->m_allocated;
}

Session::~Session() {
    State* state = static_cast<State*>(this->m_state);

    if (state->m_out) {
        fclose(state->m_out);
    }

    if (state->m_in) {
        fclose(state->m_in);
    }

    if (state->m_allocated) {
        FreeConsole();
    }

    if (state->m_window) {
        ShowWindow(state->m_window, SW_RESTORE);
        SetForegroundWindow(state->m_window);
    }

    delete state;
}

bool ReadInt(int& value) {
    return scanf_s("%d", &value) == 1;
}

bool ReadHex(uint32_t& value) {
    return scanf_s("%x", &value) == 1;
}

bool ReadDouble(double& value) {
    return scanf_s("%lf", &value) == 1;
}

bool AcquireLock(const std::string& path) {
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    s_lockFile = handle;
    return true;
}

void ReleaseLock() {
    if (s_lockFile != INVALID_HANDLE_VALUE) {
        CloseHandle(s_lockFile);
        s_lockFile = INVALID_HANDLE_VALUE;
    }
}

}
