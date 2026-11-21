#ifndef CONTROLLER_CONFIG_H
#define CONTROLLER_CONFIG_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Controller/Buttons.h"

namespace Controller {

struct AxisConfig {
    std::string m_axis;
    std::optional<float> m_shift;
    std::optional<float> m_scale;
    std::optional<float> m_deadzone;
    std::optional<float> m_buttonThreshold;
    std::string m_buttonPos;
    std::string m_buttonNeg;
    std::string m_comment;
};

struct StickConfig {
    std::string m_stick;
    std::string m_axisX;
    std::string m_axisY;
    std::optional<float> m_deadzone;
    std::optional<float> m_deadzoneX;
    std::optional<float> m_deadzoneY;
    std::string m_comment;
};

struct RawButtonMapping {
    int32_t m_rawIndex = 0;
    std::string m_button;
    std::string m_axis;
    std::optional<float> m_axisValue;
    std::string m_comment;
};

struct RawAxisMapping {
    int32_t m_rawIndex = 0;
    std::string m_axis;
    std::string m_comment;
};

// Mirrors the retail GamePadConfig table, a config without IDs applies to every device.
struct Config {
    std::optional<int32_t> m_vendorID;
    std::optional<int32_t> m_productID;
    std::string m_name;
    std::string m_comment;
    std::string m_labelStyle;
    std::vector<RawButtonMapping> m_rawButtonMappings;
    std::vector<RawAxisMapping> m_rawAxisMappings;
    std::vector<AxisConfig> m_axisConfigs;
    std::vector<StickConfig> m_stickConfigs;
};

struct ResolvedAxis {
    float m_shift = 0.0f;
    float m_scale = 1.0f;
    float m_deadzone = 0.0f;
    float m_buttonThreshold = 0.0f;
    int32_t m_buttonPos = -1;
    int32_t m_buttonNeg = -1;
};

struct ResolvedStick {
    int32_t m_axisX = -1;
    int32_t m_axisY = -1;
    float m_deadzone = 0.0f;
    float m_deadzoneX = 0.0f;
    float m_deadzoneY = 0.0f;
};

struct ResolvedRawButton {
    int32_t m_rawIndex = 0;
    int32_t m_button = -1;
    int32_t m_axis = -1;
    float m_axisValue = 1.0f;
};

struct ResolvedRawAxis {
    int32_t m_rawIndex = 0;
    int32_t m_axis = -1;
};

// The effective settings for one device, built from the default config and its own config.
struct ResolvedConfig {
    bool m_rawMapped = false;
    std::vector<ResolvedRawButton> m_rawButtons;
    std::vector<ResolvedRawAxis> m_rawAxes;
    std::string m_labelStyle;
    ResolvedAxis m_axes[AXIS_COUNT];
    ResolvedStick m_sticks[STICK_PHYSICAL_COUNT];
};

namespace Configs {

void Load();
void Save();

const std::vector<Config>& All();
const Config* Get(std::optional<int32_t> vendorID, std::optional<int32_t> productID);
void Set(const Config& config);
void Delete(std::optional<int32_t> vendorID, std::optional<int32_t> productID);

ResolvedConfig Resolve(uint16_t vendorID, uint16_t productID, bool stickAxisButtons);

std::vector<std::string> LoadSDLMappings();
void SaveSDLMapping(const char* mapping);

}

}

#endif
