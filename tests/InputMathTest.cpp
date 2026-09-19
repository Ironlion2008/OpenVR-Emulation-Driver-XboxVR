#include <gtest/gtest.h>

#include <Driver/InputMath.hpp>

using namespace OpenVREmulatorDriver;

TEST(NormalizeThumbAxis, AppliesDeadzone)
{
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(0, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), 0.0f);
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE,
                                       XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), 0.0f);
}

TEST(NormalizeThumbAxis, ReachesUnitRange)
{
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(32767, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), 1.0f);
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(-32768, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), -1.0f);
}

TEST(NormalizeTrigger, SupportsCustomDeadzone)
{
    EXPECT_FLOAT_EQ(NormalizeTrigger(30, 30), 0.0f);
    EXPECT_FLOAT_EQ(NormalizeTrigger(255, 30), 1.0f);
}

TEST(Clamp01, Clamps)
{
    EXPECT_FLOAT_EQ(Clamp01(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(Clamp01(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(Clamp01(2.0f), 1.0f);
}
