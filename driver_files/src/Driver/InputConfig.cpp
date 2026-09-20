#include "InputConfig.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <array>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace OpenVREmulatorDriver
{
namespace
{

void Trim(std::string &s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        s.clear();
        return;
    }
    s = s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

std::string Upper(std::string value)
{
    for (char &ch : value)
    {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

WORD XInputButtonFromName(const std::string &value)
{
    const std::string name = Upper(value);
    static const std::unordered_map<std::string, WORD> table = {
        {"DPAD_UP", XINPUT_GAMEPAD_DPAD_UP},
        {"DPAD_DOWN", XINPUT_GAMEPAD_DPAD_DOWN},
        {"DPAD_LEFT", XINPUT_GAMEPAD_DPAD_LEFT},
        {"DPAD_RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT},
        {"START", XINPUT_GAMEPAD_START},
        {"BACK", XINPUT_GAMEPAD_BACK},
        {"LEFT_THUMB", XINPUT_GAMEPAD_LEFT_THUMB},
        {"RIGHT_THUMB", XINPUT_GAMEPAD_RIGHT_THUMB},
        {"LEFT_SHOULDER", XINPUT_GAMEPAD_LEFT_SHOULDER},
        {"RIGHT_SHOULDER", XINPUT_GAMEPAD_RIGHT_SHOULDER},
        {"A", XINPUT_GAMEPAD_A},
        {"B", XINPUT_GAMEPAD_B},
        {"X", XINPUT_GAMEPAD_X},
        {"Y", XINPUT_GAMEPAD_Y},
    };

    const auto it = table.find(name);
    return it == table.end() ? 0 : it->second;
}

WORD ParseXInputCombo(const std::string &value)
{
    WORD result = 0;
    std::istringstream stream(value);
    std::string token;
    while (std::getline(stream, token, '|'))
    {
        Trim(token);
        result = static_cast<WORD>(result | XInputButtonFromName(token));
    }
    return result;
}

int ParseKeyValue(const std::string &value, int fallback)
{
    const std::string name = Upper(value);
    static const std::unordered_map<std::string, int> table = {
        {"VK_SPACE", VK_SPACE}, {"VK_LEFT", VK_LEFT},   {"VK_RIGHT", VK_RIGHT},
        {"VK_UP", VK_UP},       {"VK_DOWN", VK_DOWN},   {"VK_RETURN", VK_RETURN},
        {"VK_SHIFT", VK_SHIFT}, {"VK_CONTROL", VK_CONTROL}, {"VK_MENU", VK_MENU},
        {"VK_TAB", VK_TAB},     {"VK_ESCAPE", VK_ESCAPE},
    };

    const auto it = table.find(name);
    if (it != table.end())
    {
        return it->second;
    }

    try
    {
        std::size_t parsed = 0;
        const unsigned long number = std::stoul(value, &parsed, 0);
        if (parsed != value.size())
        {
            return fallback;
        }
        return static_cast<int>(number);
    }
    catch (...)
    {
        return fallback;
    }
}

float ParseFloat(const std::string &value, float fallback)
{
    try
    {
        std::size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        return parsed == value.size() ? result : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

long ParseLong(const std::string &value, long fallback)
{
    try
    {
        std::size_t parsed = 0;
        const long result = std::stol(value, &parsed, 10);
        return parsed == value.size() ? result : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

bool ParseBool(const std::string &value, bool fallback)
{
    const std::string name = Upper(value);
    if (name == "1" || name == "TRUE" || name == "YES")
    {
        return true;
    }
    if (name == "0" || name == "FALSE" || name == "NO")
    {
        return false;
    }
    return fallback;
}

using IniMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

IniMap ParseIniFile(const std::string &path)
{
    IniMap result;
    std::ifstream file(path);
    if (!file)
    {
        return result;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line))
    {
        const auto comment = line.find_first_of(";#");
        if (comment != std::string::npos)
        {
            line.resize(comment);
        }
        Trim(line);
        if (line.empty())
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            Trim(section);
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);
        Trim(key);
        Trim(value);
        if (!key.empty())
        {
            result[section][key] = value;
        }
    }
    return result;
}

std::string GetDriverRootPath()
{
    std::array<char, MAX_PATH> buffer{};
    HMODULE module = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&GetDriverRootPath),
        &module);
    GetModuleFileNameA(module, buffer.data(), static_cast<DWORD>(buffer.size()));

    std::string path(buffer.data());
    for (int i = 0; i < 2; ++i)
    {
        const auto separator = path.find_last_of("\\/");
        if (separator == std::string::npos)
        {
            break;
        }
        path.resize(separator);
    }
    return path;
}

}  // namespace

InputConfig InputConfig::Defaults()
{
    return {};
}

InputConfig InputConfig::LoadFromDriverRoot()
{
    return LoadFromFile(GetDriverRootPath() + "\\resources\\input_mapping.ini");
}

InputConfig InputConfig::LoadFromFile(const std::string &path)
{
    InputConfig config = Defaults();
    const IniMap ini = ParseIniFile(path);
    if (ini.empty())
    {
        return config;
    }

    const auto get = [&ini](const char *section, const char *key) -> std::string {
        const auto sectionIt = ini.find(section);
        if (sectionIt == ini.end())
        {
            return {};
        }
        const auto keyIt = sectionIt->second.find(key);
        return keyIt == sectionIt->second.end() ? std::string{} : keyIt->second;
    };

    const std::string toggle = get("hmd", "key_mouse_toggle");
    if (!toggle.empty())
    {
        config.hmd.key_mouse_toggle = ParseKeyValue(toggle, config.hmd.key_mouse_toggle);
    }
    const std::string sensitivity = get("hmd", "mouse_sensitivity");
    if (!sensitivity.empty())
    {
        config.hmd.mouse_sensitivity = ParseFloat(sensitivity, config.hmd.mouse_sensitivity);
    }

    const auto loadController = [&get](const char *section, ControllerInputConfig &controller) {
        const auto loadButton = [&get, section](const char *key, WORD &target) {
            const std::string value = get(section, key);
            if (!value.empty())
            {
                target = ParseXInputCombo(value);
            }
        };
        const auto loadFloat = [&get, section](const char *key, float &target) {
            const std::string value = get(section, key);
            if (!value.empty())
            {
                target = ParseFloat(value, target);
            }
        };

        loadButton("btn_a", controller.btn_a);
        loadButton("btn_b", controller.btn_b);
        loadButton("btn_x", controller.btn_x);
        loadButton("btn_y", controller.btn_y);
        loadButton("btn_grip", controller.btn_grip);
        loadButton("btn_system", controller.btn_system);
        loadButton("btn_joystick_click", controller.btn_joystick_click);

        const std::string stickDeadzone = get(section, "stick_deadzone");
        if (!stickDeadzone.empty())
        {
            controller.stick_deadzone = static_cast<SHORT>(ParseLong(stickDeadzone, controller.stick_deadzone));
        }

        const std::string triggerDeadzone = get(section, "trigger_deadzone");
        if (!triggerDeadzone.empty())
        {
            const long parsed = ParseLong(triggerDeadzone, controller.trigger_deadzone);
            const long clamped = std::max(0L, std::min(parsed, 254L));
            controller.trigger_deadzone = static_cast<BYTE>(clamped);
        }

        loadFloat("stick_sensitivity", controller.stick_sensitivity);
        loadFloat("trigger_click_threshold", controller.trigger_click_threshold);

        const std::string dpad = get(section, "dpad_to_trackpad");
        if (!dpad.empty())
        {
            controller.dpad_to_trackpad = ParseBool(dpad, controller.dpad_to_trackpad);
        }
    };

    loadController("left_controller", config.left_controller);
    loadController("right_controller", config.right_controller);

    return config;
}

}  // namespace OpenVREmulatorDriver
