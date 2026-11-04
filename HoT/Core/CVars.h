#ifndef HOT_CORE_CVARS_H
#define HOT_CORE_CVARS_H

#include <cstdint>

struct CVar;

namespace HoT::CVars {

using ChangeFn = void (*)(const char* value);

// Archived HoT setting, onChange runs once with the loaded value and again on every change.
CVar* Register(const char* name, const char* help, const char* defaultValue, ChangeFn onChange = nullptr);

CVar* RegisterHidden(const char* name, const char* help, const char* defaultValue, ChangeFn onChange = nullptr);

int32_t Int(const char* value);
bool Bool(const char* value);
float Float(const char* value);

}

#endif
