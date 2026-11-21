#include "Controller/Config.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Storm/H/storm.h"

namespace Controller::Configs {

static const char* CONFIG_FILE = "WTF\\GamePadConfigs.wtf";
static const char* MAPPING_FILE = "WTF\\GamePadMappings.txt";

static std::vector<Config> s_configs;

static bool SameID(const std::optional<int32_t>& a, const std::optional<int32_t>& b) {
    return a.has_value() == b.has_value() && (!a || *a == *b);
}

const std::vector<Config>& All() {
    return s_configs;
}

const Config* Get(std::optional<int32_t> vendorID, std::optional<int32_t> productID) {
    for (const Config& config : s_configs) {
        if (SameID(config.m_vendorID, vendorID) && SameID(config.m_productID, productID)) {
            return &config;
        }
    }

    return nullptr;
}

void Set(const Config& config) {
    for (Config& existing : s_configs) {
        if (SameID(existing.m_vendorID, config.m_vendorID) && SameID(existing.m_productID, config.m_productID)) {
            existing = config;
            return;
        }
    }

    s_configs.push_back(config);
}

void Delete(std::optional<int32_t> vendorID, std::optional<int32_t> productID) {
    for (auto it = s_configs.begin(); it != s_configs.end(); ++it) {
        if (SameID(it->m_vendorID, vendorID) && SameID(it->m_productID, productID)) {
            s_configs.erase(it);
            return;
        }
    }
}

static int32_t ParseButton(const std::string& name) {
    return name.empty() ? -1 : ButtonFromName(name.c_str());
}

static void ApplyConfig(ResolvedConfig& resolved, const Config& config) {
    if (!config.m_labelStyle.empty()) {
        resolved.m_labelStyle = config.m_labelStyle;
    }

    for (const AxisConfig& axisConfig : config.m_axisConfigs) {
        int32_t axis = AxisFromName(axisConfig.m_axis.c_str());

        if (axis < 0) {
            continue;
        }

        ResolvedAxis& target = resolved.m_axes[axis];

        if (axisConfig.m_shift) {
            target.m_shift = *axisConfig.m_shift;
        }

        if (axisConfig.m_scale) {
            target.m_scale = *axisConfig.m_scale;
        }

        if (axisConfig.m_deadzone) {
            target.m_deadzone = *axisConfig.m_deadzone;
        }

        if (axisConfig.m_buttonThreshold) {
            target.m_buttonThreshold = *axisConfig.m_buttonThreshold;
        }

        if (!axisConfig.m_buttonPos.empty()) {
            target.m_buttonPos = ParseButton(axisConfig.m_buttonPos);
        }

        if (!axisConfig.m_buttonNeg.empty()) {
            target.m_buttonNeg = ParseButton(axisConfig.m_buttonNeg);
        }
    }

    for (const StickConfig& stickConfig : config.m_stickConfigs) {
        int32_t stick = StickFromName(stickConfig.m_stick.c_str());

        if (stick < 0 || stick >= STICK_PHYSICAL_COUNT) {
            continue;
        }

        ResolvedStick& target = resolved.m_sticks[stick];

        if (!stickConfig.m_axisX.empty()) {
            target.m_axisX = AxisFromName(stickConfig.m_axisX.c_str());
        }

        if (!stickConfig.m_axisY.empty()) {
            target.m_axisY = AxisFromName(stickConfig.m_axisY.c_str());
        }

        if (stickConfig.m_deadzone) {
            target.m_deadzone = *stickConfig.m_deadzone;
        }

        if (stickConfig.m_deadzoneX) {
            target.m_deadzoneX = *stickConfig.m_deadzoneX;
        }

        if (stickConfig.m_deadzoneY) {
            target.m_deadzoneY = *stickConfig.m_deadzoneY;
        }
    }
}

ResolvedConfig Resolve(uint16_t vendorID, uint16_t productID, bool stickAxisButtons) {
    ResolvedConfig resolved;

    resolved.m_sticks[STICK_LEFT] = { AXIS_LSTICK_X, AXIS_LSTICK_Y, 0.2f, 0.0f, 0.0f };
    resolved.m_sticks[STICK_RIGHT] = { AXIS_RSTICK_X, AXIS_RSTICK_Y, 0.2f, 0.0f, 0.0f };

    resolved.m_axes[AXIS_LTRIGGER].m_deadzone = 0.05f;
    resolved.m_axes[AXIS_LTRIGGER].m_buttonThreshold = 0.3f;
    resolved.m_axes[AXIS_LTRIGGER].m_buttonPos = BUTTON_LTRIGGER;
    resolved.m_axes[AXIS_RTRIGGER].m_deadzone = 0.05f;
    resolved.m_axes[AXIS_RTRIGGER].m_buttonThreshold = 0.3f;
    resolved.m_axes[AXIS_RTRIGGER].m_buttonPos = BUTTON_RTRIGGER;

    if (stickAxisButtons) {
        resolved.m_axes[AXIS_LSTICK_X] = { 0.0f, 1.0f, 0.0f, 0.5f, BUTTON_LSTICK_RIGHT, BUTTON_LSTICK_LEFT };
        resolved.m_axes[AXIS_LSTICK_Y] = { 0.0f, 1.0f, 0.0f, 0.5f, BUTTON_LSTICK_UP, BUTTON_LSTICK_DOWN };
        resolved.m_axes[AXIS_RSTICK_X] = { 0.0f, 1.0f, 0.0f, 0.5f, BUTTON_RSTICK_RIGHT, BUTTON_RSTICK_LEFT };
        resolved.m_axes[AXIS_RSTICK_Y] = { 0.0f, 1.0f, 0.0f, 0.5f, BUTTON_RSTICK_UP, BUTTON_RSTICK_DOWN };
    }

    if (const Config* config = Get(std::nullopt, std::nullopt)) {
        ApplyConfig(resolved, *config);
    }

    const Config* device = Get(vendorID, productID);

    if (!device) {
        device = Get(vendorID, std::nullopt);
    }

    if (device) {
        ApplyConfig(resolved, *device);
        resolved.m_rawMapped = !device->m_rawButtonMappings.empty() || !device->m_rawAxisMappings.empty();

        for (const RawButtonMapping& mapping : device->m_rawButtonMappings) {
            resolved.m_rawButtons.push_back({ mapping.m_rawIndex, ButtonFromName(mapping.m_button.c_str()), AxisFromName(mapping.m_axis.c_str()), mapping.m_axisValue.value_or(1.0f) });
        }

        for (const RawAxisMapping& mapping : device->m_rawAxisMappings) {
            resolved.m_rawAxes.push_back({ mapping.m_rawIndex, AxisFromName(mapping.m_axis.c_str()) });
        }
    }

    return resolved;
}

static void WriteFloat(FILE* file, const char* key, const std::optional<float>& value) {
    if (value) {
        fprintf(file, " %s=%g", key, *value);
    }
}

static void WriteString(FILE* file, const char* key, const std::string& value) {
    if (!value.empty()) {
        fprintf(file, " %s=%s", key, value.c_str());
    }
}

static void WriteComment(FILE* file, const std::string& comment) {
    if (!comment.empty()) {
        fprintf(file, " comment=%s", comment.c_str());
    }

    fputc('\n', file);
}

void Save() {
    FILE* file = fopen(CONFIG_FILE, "w");

    if (!file) {
        return;
    }

    for (const Config& config : s_configs) {
        fputs("CONFIG\n", file);

        if (config.m_vendorID) {
            fprintf(file, "vendorID %d\n", *config.m_vendorID);
        }

        if (config.m_productID) {
            fprintf(file, "productID %d\n", *config.m_productID);
        }

        if (!config.m_name.empty()) {
            fprintf(file, "name %s\n", config.m_name.c_str());
        }

        if (!config.m_comment.empty()) {
            fprintf(file, "comment %s\n", config.m_comment.c_str());
        }

        if (!config.m_labelStyle.empty()) {
            fprintf(file, "labelStyle %s\n", config.m_labelStyle.c_str());
        }

        for (const RawButtonMapping& mapping : config.m_rawButtonMappings) {
            fprintf(file, "rawButton rawIndex=%d", mapping.m_rawIndex);
            WriteString(file, "button", mapping.m_button);
            WriteString(file, "axis", mapping.m_axis);
            WriteFloat(file, "axisValue", mapping.m_axisValue);
            WriteComment(file, mapping.m_comment);
        }

        for (const RawAxisMapping& mapping : config.m_rawAxisMappings) {
            fprintf(file, "rawAxis rawIndex=%d", mapping.m_rawIndex);
            WriteString(file, "axis", mapping.m_axis);
            WriteComment(file, mapping.m_comment);
        }

        for (const AxisConfig& axis : config.m_axisConfigs) {
            fputs("axis", file);
            WriteString(file, "axis", axis.m_axis);
            WriteFloat(file, "shift", axis.m_shift);
            WriteFloat(file, "scale", axis.m_scale);
            WriteFloat(file, "deadzone", axis.m_deadzone);
            WriteFloat(file, "buttonThreshold", axis.m_buttonThreshold);
            WriteString(file, "buttonPos", axis.m_buttonPos);
            WriteString(file, "buttonNeg", axis.m_buttonNeg);
            WriteComment(file, axis.m_comment);
        }

        for (const StickConfig& stick : config.m_stickConfigs) {
            fputs("stick", file);
            WriteString(file, "stick", stick.m_stick);
            WriteString(file, "axisX", stick.m_axisX);
            WriteString(file, "axisY", stick.m_axisY);
            WriteFloat(file, "deadzone", stick.m_deadzone);
            WriteFloat(file, "deadzoneX", stick.m_deadzoneX);
            WriteFloat(file, "deadzoneY", stick.m_deadzoneY);
            WriteComment(file, stick.m_comment);
        }

        fputs("END\n", file);
    }

    fclose(file);
}

struct Fields {
    std::vector<std::pair<std::string, std::string>> m_values;
    std::string m_comment;

    const std::string* Find(const char* key) const {
        for (const auto& value : this->m_values) {
            if (!SStrCmpI(value.first.c_str(), key, STORM_MAX_STR)) {
                return &value.second;
            }
        }

        return nullptr;
    }

    std::string String(const char* key) const {
        const std::string* value = this->Find(key);
        return value ? *value : std::string();
    }

    std::optional<float> Float(const char* key) const {
        const std::string* value = this->Find(key);
        return value ? std::optional<float>(static_cast<float>(atof(value->c_str()))) : std::nullopt;
    }
};

static Fields ParseFields(const char* text) {
    Fields fields;

    while (*text) {
        while (*text == ' ' || *text == '\t') {
            text++;
        }

        if (!*text) {
            break;
        }

        if (!strncmp(text, "comment=", 8)) {
            fields.m_comment = text + 8;
            break;
        }

        const char* end = text;

        while (*end && *end != ' ' && *end != '\t') {
            end++;
        }

        std::string token(text, end);
        size_t equals = token.find('=');

        if (equals != std::string::npos) {
            fields.m_values.emplace_back(token.substr(0, equals), token.substr(equals + 1));
        }

        text = end;
    }

    return fields;
}

static const char* After(const char* line, const char* keyword) {
    size_t length = strlen(keyword);
    return !strncmp(line, keyword, length) && (line[length] == ' ' || !line[length]) ? line + length + (line[length] ? 1 : 0) : nullptr;
}

void Load() {
    s_configs.clear();

    FILE* file = fopen(CONFIG_FILE, "r");

    if (!file) {
        return;
    }

    char line[2048];
    Config* current = nullptr;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        const char* rest;

        if (!strcmp(line, "CONFIG")) {
            s_configs.emplace_back();
            current = &s_configs.back();
        } else if (!strcmp(line, "END")) {
            current = nullptr;
        } else if (!current) {
            continue;
        } else if ((rest = After(line, "vendorID"))) {
            current->m_vendorID = atoi(rest);
        } else if ((rest = After(line, "productID"))) {
            current->m_productID = atoi(rest);
        } else if ((rest = After(line, "name"))) {
            current->m_name = rest;
        } else if ((rest = After(line, "comment"))) {
            current->m_comment = rest;
        } else if ((rest = After(line, "labelStyle"))) {
            current->m_labelStyle = rest;
        } else if ((rest = After(line, "rawButton"))) {
            Fields fields = ParseFields(rest);
            RawButtonMapping mapping;
            mapping.m_rawIndex = atoi(fields.String("rawIndex").c_str());
            mapping.m_button = fields.String("button");
            mapping.m_axis = fields.String("axis");
            mapping.m_axisValue = fields.Float("axisValue");
            mapping.m_comment = fields.m_comment;
            current->m_rawButtonMappings.push_back(mapping);
        } else if ((rest = After(line, "rawAxis"))) {
            Fields fields = ParseFields(rest);
            RawAxisMapping mapping;
            mapping.m_rawIndex = atoi(fields.String("rawIndex").c_str());
            mapping.m_axis = fields.String("axis");
            mapping.m_comment = fields.m_comment;
            current->m_rawAxisMappings.push_back(mapping);
        } else if ((rest = After(line, "axis"))) {
            Fields fields = ParseFields(rest);
            AxisConfig axis;
            axis.m_axis = fields.String("axis");
            axis.m_shift = fields.Float("shift");
            axis.m_scale = fields.Float("scale");
            axis.m_deadzone = fields.Float("deadzone");
            axis.m_buttonThreshold = fields.Float("buttonThreshold");
            axis.m_buttonPos = fields.String("buttonPos");
            axis.m_buttonNeg = fields.String("buttonNeg");
            axis.m_comment = fields.m_comment;
            current->m_axisConfigs.push_back(axis);
        } else if ((rest = After(line, "stick"))) {
            Fields fields = ParseFields(rest);
            StickConfig stick;
            stick.m_stick = fields.String("stick");
            stick.m_axisX = fields.String("axisX");
            stick.m_axisY = fields.String("axisY");
            stick.m_deadzone = fields.Float("deadzone");
            stick.m_deadzoneX = fields.Float("deadzoneX");
            stick.m_deadzoneY = fields.Float("deadzoneY");
            stick.m_comment = fields.m_comment;
            current->m_stickConfigs.push_back(stick);
        }
    }

    fclose(file);
}

std::vector<std::string> LoadSDLMappings() {
    std::vector<std::string> mappings;
    FILE* file = fopen(MAPPING_FILE, "r");

    if (!file) {
        return mappings;
    }

    char line[4096];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;

        if (line[0] && line[0] != '#') {
            mappings.emplace_back(line);
        }
    }

    fclose(file);
    return mappings;
}

void SaveSDLMapping(const char* mapping) {
    std::vector<std::string> mappings = LoadSDLMappings();
    std::string guid(mapping, strcspn(mapping, ","));

    FILE* file = fopen(MAPPING_FILE, "w");

    if (!file) {
        return;
    }

    for (const std::string& existing : mappings) {
        if (existing.compare(0, existing.find(','), guid)) {
            fprintf(file, "%s\n", existing.c_str());
        }
    }

    fprintf(file, "%s\n", mapping);
    fclose(file);
}

}
