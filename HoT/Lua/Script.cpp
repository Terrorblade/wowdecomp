#include "HoT/Lua/Script.h"
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "HoT/Core/Log.h"

namespace HoT::Lua {

struct FunctionEntry {
    const char* name;
    lua_CFunction function;
    State state;
};

struct EventEntry {
    const char* name;
    State state;
};

static std::vector<FunctionEntry>& Functions() {
    static std::vector<FunctionEntry> functions;
    return functions;
}

static std::vector<EventEntry>& Events() {
    static std::vector<EventEntry> events;
    return events;
}

static std::unordered_map<std::string_view, uint32_t> s_eventIndex;
static State s_state = State::Glue;

void RegisterFunction(const char* name, lua_CFunction function, State state) {
    Functions().push_back({ name, function, state });
}

void RegisterEvent(const char* name, State state) {
    for (const EventEntry& e : Events()) {
        if (!strcmp(e.name, name)) {
            return;
        }
    }

    Events().push_back({ name, state });
}

static bool Matches(State entry, State current) {
    return entry == State::Any || entry == current;
}

static void Load(State state, const char** stockEvents, uint32_t stockCount) {
    s_state = state;

    for (const FunctionEntry& f : Functions()) {
        if (Matches(f.state, state)) {
            FrameScript_RegisterFunction(f.name, f.function);
        }
    }

    std::vector<const char*> names(stockEvents, stockEvents + stockCount);

    for (const EventEntry& e : Events()) {
        if (Matches(e.state, state)) {
            names.push_back(e.name);
        }
    }

    FrameScript_CreateEvents(names.data(), static_cast<uint32_t>(names.size()));

    s_eventIndex.clear();

    for (uint32_t i = 0; i < names.size(); i++) {
        if (names[i]) {
            s_eventIndex.emplace(names[i], i);
        }
    }
}

void LoadGame(const char** stockEvents, uint32_t stockCount) {
    Load(State::Game, stockEvents, stockCount);
}

void LoadGlue(const char** stockEvents, uint32_t stockCount) {
    Load(State::Glue, stockEvents, stockCount);
}

State CurrentState() {
    return s_state;
}

bool HasEvent(const char* name) {
    return name && s_eventIndex.count(name);
}

int32_t EventIndex(const char* name) {
    auto it = name ? s_eventIndex.find(name) : s_eventIndex.end();
    return it == s_eventIndex.end() ? -1 : static_cast<int32_t>(it->second);
}

bool vSignalEvent(const char* name, const char* format, va_list args) {
    auto it = name ? s_eventIndex.find(name) : s_eventIndex.end();

    if (it == s_eventIndex.end()) {
        LOG_DEBUG << "SignalEvent: no event named " << (name ? name : "(null)") << " in the current event table";
        return false;
    }

    vFrameScript_SignalEvent(it->second, format, args);
    return true;
}

bool SignalEvent(const char* name, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool result = vSignalEvent(name, format, args);
    va_end(args);
    return result;
}

}
