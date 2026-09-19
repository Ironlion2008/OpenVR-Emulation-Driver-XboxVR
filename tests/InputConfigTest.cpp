#include <gtest/gtest.h>

#include <Driver/InputConfig.hpp>

#include <fstream>
#include <string>

using namespace OpenVREmulatorDriver;

namespace
{
std::string WriteTempIni(const std::string &content)
{
    const std::string path = "test_input_mapping_xbox.ini";
    std::ofstream file(path, std::ios::trunc);
    file << content;
    return path;
}
}  // namespace

TEST(InputConfigDefaults, XboxFaceButtons)
{
    const auto config = InputConfig::Defaults();
    EXPECT_EQ(config.left_controller.btn_x, XINPUT_GAMEPAD_X);
    EXPECT_EQ(config.left_controller.btn_y, XINPUT_GAMEPAD_Y);
    EXPECT_EQ(config.right_controller.btn_a, XINPUT_GAMEPAD_A);
    EXPECT_EQ(config.right_controller.btn_b, XINPUT_GAMEPAD_B);
}

TEST(InputConfigDefaults, XboxControllerSystemButtons)
{
    const auto config = InputConfig::Defaults();
    EXPECT_EQ(config.left_controller.btn_system, XINPUT_GAMEPAD_BACK);
    EXPECT_EQ(config.right_controller.btn_system, XINPUT_GAMEPAD_START);
}

TEST(InputConfigLoad, ParsesXboxMappings)
{
    const auto path = WriteTempIni(
        "[left_controller]\n"
        "btn_x = Y\n"
        "btn_grip = RIGHT_SHOULDER\n"
        "stick_deadzone = 9000\n"
        "trigger_deadzone = 40\n"
        "stick_sensitivity = 0.8\n"
        "trigger_click_threshold = 0.6\n"
        "dpad_to_trackpad = 0\n");

    const auto config = InputConfig::LoadFromFile(path);
    EXPECT_EQ(config.left_controller.btn_x, XINPUT_GAMEPAD_Y);
    EXPECT_EQ(config.left_controller.btn_grip, XINPUT_GAMEPAD_RIGHT_SHOULDER);
    EXPECT_EQ(config.left_controller.stick_deadzone, 9000);
    EXPECT_EQ(config.left_controller.trigger_deadzone, 40);
    EXPECT_FLOAT_EQ(config.left_controller.stick_sensitivity, 0.8f);
    EXPECT_FLOAT_EQ(config.left_controller.trigger_click_threshold, 0.6f);
    EXPECT_FALSE(config.left_controller.dpad_to_trackpad);
}
