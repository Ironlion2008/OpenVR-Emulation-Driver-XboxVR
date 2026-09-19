#pragma once

#include <Windows.h>
#include <Xinput.h>

#include <string>

namespace OpenVREmulatorDriver
{

struct HMDInputConfig
{
    static constexpr float DefaultMouseSensitivity = 0.003f;
    int key_mouse_toggle = VK_SPACE;
    float mouse_sensitivity = DefaultMouseSensitivity;
};

struct ControllerInputConfig
{
    static constexpr SHORT DefaultStickDeadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    static constexpr BYTE DefaultTriggerDeadzone = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    static constexpr float DefaultStickSensitivity = 1.0f;
    static constexpr float DefaultTriggerClickThreshold = 0.75f;

    WORD btn_a = 0;
    WORD btn_b = 0;
    WORD btn_x = 0;
    WORD btn_y = 0;
    WORD btn_grip = 0;
    WORD btn_system = 0;
    WORD btn_joystick_click = 0;

    SHORT stick_deadzone = DefaultStickDeadzone;
    BYTE trigger_deadzone = DefaultTriggerDeadzone;
    float stick_sensitivity = DefaultStickSensitivity;
    float trigger_click_threshold = DefaultTriggerClickThreshold;
    bool dpad_to_trackpad = true;
};

struct LeftControllerConfig : ControllerInputConfig
{
    LeftControllerConfig()
    {
        btn_x = XINPUT_GAMEPAD_X;
        btn_y = XINPUT_GAMEPAD_Y;
        btn_grip = XINPUT_GAMEPAD_LEFT_SHOULDER;
        btn_system = XINPUT_GAMEPAD_BACK;
        btn_joystick_click = XINPUT_GAMEPAD_LEFT_THUMB;
    }
};

struct RightControllerConfig : ControllerInputConfig
{
    RightControllerConfig()
    {
        btn_a = XINPUT_GAMEPAD_A;
        btn_b = XINPUT_GAMEPAD_B;
        btn_grip = XINPUT_GAMEPAD_RIGHT_SHOULDER;
        btn_system = XINPUT_GAMEPAD_START;
        btn_joystick_click = XINPUT_GAMEPAD_RIGHT_THUMB;
        dpad_to_trackpad = false;
    }
};

struct InputConfig
{
    HMDInputConfig hmd;
    LeftControllerConfig left_controller;
    RightControllerConfig right_controller;

    static InputConfig Defaults();
    static InputConfig LoadFromFile(const std::string &path);
    static InputConfig LoadFromDriverRoot();
};

}  // namespace OpenVREmulatorDriver
