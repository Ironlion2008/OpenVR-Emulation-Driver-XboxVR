#pragma once

// Prevent Windows min/max macros from colliding with std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <Xinput.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace OpenVREmulatorDriver
{

inline float Clamp01(float value) noexcept
{
    return std::fmax(0.0f, std::fmin(value, 1.0f));
}

inline float NormalizeThumbAxis(SHORT value, SHORT deadzone) noexcept
{
    const int magnitude = value < 0 ? -static_cast<int>(value) : static_cast<int>(value);
    const int dz = std::max(0, std::min(static_cast<int>(deadzone), 32767));

    if (magnitude <= dz)
    {
        return 0.0f;
    }

    if (value > 0)
    {
        const float normalized = static_cast<float>(magnitude - dz) /
                                 static_cast<float>(32767 - dz);
        return Clamp01(normalized);
    }

    const float normalized = -static_cast<float>(magnitude - dz) /
                             static_cast<float>(32768 - dz);
    return std::fmax(normalized, -1.0f);
}

inline float NormalizeTrigger(BYTE value, BYTE deadzone) noexcept
{
    const int dz = std::min(static_cast<int>(deadzone), 254);
    const int adjusted = static_cast<int>(value) - dz;
    if (adjusted <= 0)
    {
        return 0.0f;
    }

    return Clamp01(static_cast<float>(adjusted) / static_cast<float>(255 - dz));
}

}  // namespace OpenVREmulatorDriver
