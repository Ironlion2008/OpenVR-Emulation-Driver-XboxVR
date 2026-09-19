#include "ControllerDevice.h"

#include <Windows.h>
#include <Xinput.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#pragma comment(lib, "user32.lib")

namespace
{
    // -------------------------------------------------------------------------
    // Explicit XInput 1.4 loading
    // -------------------------------------------------------------------------

    using XInputGetStateFn =
        DWORD(WINAPI*)(DWORD dwUserIndex, XINPUT_STATE* pState);

    HMODULE g_xinputModule = nullptr;
    XInputGetStateFn g_xinputGetState = nullptr;
    bool g_xinputInitialized = false;

    bool InitializeXInput()
    {
        if (g_xinputInitialized)
            return g_xinputGetState != nullptr;

        g_xinputInitialized = true;

        g_xinputModule = LoadLibraryW(L"xinput1_4.dll");

        if (!g_xinputModule)
        {
            const DWORD error = GetLastError();

            char buffer[256];
            snprintf(
                buffer,
                sizeof(buffer),
                "[XboxVR][XINPUT] Failed to load xinput1_4.dll, error=%lu\n",
                static_cast<unsigned long>(error));

            vr::VRDriverLog()->Log(buffer);
            return false;
        }

        g_xinputGetState =
            reinterpret_cast<XInputGetStateFn>(
                GetProcAddress(g_xinputModule, "XInputGetState"));

        if (!g_xinputGetState)
        {
            const DWORD error = GetLastError();

            char buffer[256];
            snprintf(
                buffer,
                sizeof(buffer),
                "[XboxVR][XINPUT] XInputGetState not found, error=%lu\n",
                static_cast<unsigned long>(error));

            vr::VRDriverLog()->Log(buffer);

            FreeLibrary(g_xinputModule);
            g_xinputModule = nullptr;

            return false;
        }

        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(
            g_xinputModule,
            modulePath,
            MAX_PATH);

        char narrowPath[MAX_PATH] = {};

        WideCharToMultiByte(
            CP_UTF8,
            0,
            modulePath,
            -1,
            narrowPath,
            MAX_PATH,
            nullptr,
            nullptr);

        char buffer[512];

        snprintf(
            buffer,
            sizeof(buffer),
            "[XboxVR][XINPUT] Loaded xinput1_4.dll: %s\n",
            narrowPath);

        vr::VRDriverLog()->Log(buffer);

        return true;
    }

    // -------------------------------------------------------------------------
    // XInput state
    // -------------------------------------------------------------------------

    bool ReadFirstConnectedXInput(XINPUT_STATE& state)
    {
        if (!InitializeXInput())
        {
            vr::VRDriverLog()->Log(
                "[XboxVR][XINPUT] XInput initialization failed\n");

            ZeroMemory(&state, sizeof(state));
            return false;
        }

        for (DWORD userIndex = 0; userIndex < XUSER_MAX_COUNT; ++userIndex)
        {
            ZeroMemory(&state, sizeof(state));

            const DWORD result =
                g_xinputGetState(userIndex, &state);

            if (result == ERROR_SUCCESS)
            {
                char buffer[512];

                snprintf(
                    buffer,
                    sizeof(buffer),
                    "[XboxVR][S4] XInput connected: slot=%lu buttons=%u LX=%d LY=%d RX=%d RY=%d LT=%u RT=%u\n",
                    static_cast<unsigned long>(userIndex),
                    static_cast<unsigned int>(state.Gamepad.wButtons),
                    static_cast<int>(state.Gamepad.sThumbLX),
                    static_cast<int>(state.Gamepad.sThumbLY),
                    static_cast<int>(state.Gamepad.sThumbRX),
                    static_cast<int>(state.Gamepad.sThumbRY),
                    static_cast<unsigned int>(state.Gamepad.bLeftTrigger),
                    static_cast<unsigned int>(state.Gamepad.bRightTrigger));

                vr::VRDriverLog()->Log(buffer);

                return true;
            }

            if (result == ERROR_DEVICE_NOT_CONNECTED)
            {
                char buffer[256];

                snprintf(
                    buffer,
                    sizeof(buffer),
                    "[XboxVR][S4] XInput slot %lu: ERROR_DEVICE_NOT_CONNECTED\n",
                    static_cast<unsigned long>(userIndex));

                vr::VRDriverLog()->Log(buffer);

                continue;
            }

            char buffer[256];

            snprintf(
                buffer,
                sizeof(buffer),
                "[XboxVR][S4] XInput slot %lu: error=%lu\n",
                static_cast<unsigned long>(userIndex),
                static_cast<unsigned long>(result));

            vr::VRDriverLog()->Log(buffer);
        }

        ZeroMemory(&state, sizeof(state));

        vr::VRDriverLog()->Log(
            "[XboxVR][S4] NO XINPUT CONTROLLER\n");

        return false;
    }

    // -------------------------------------------------------------------------
    // Utility functions
    // -------------------------------------------------------------------------

    float NormalizeStick(short value)
    {
        constexpr float maxPositive = 32767.0f;
        constexpr float maxNegative = 32768.0f;

        if (value >= 0)
            return static_cast<float>(value) / maxPositive;

        return static_cast<float>(value) / maxNegative;
    }

    float NormalizeTrigger(BYTE value)
    {
        return static_cast<float>(value) / 255.0f;
    }

    bool IsPressed(WORD buttons, WORD mask)
    {
        return (buttons & mask) != 0;
    }

    float ApplyDeadzone(float value, float deadzone)
    {
        if (std::fabs(value) <= deadzone)
            return 0.0f;

        const float sign = value < 0.0f ? -1.0f : 1.0f;

        const float magnitude =
            (std::fabs(value) - deadzone) /
            (1.0f - deadzone);

        return sign * std::clamp(magnitude, 0.0f, 1.0f);
    }
}

// ============================================================================
// Constructor
// ============================================================================

ControllerDevice::ControllerDevice(vr::ETrackedControllerRole role)
    : role_(role),
      device_id_(vr::k_unTrackedDeviceIndexInvalid)
{
}

// ============================================================================
// Activate
// ============================================================================

vr::EVRInitError ControllerDevice::Activate(uint32_t unObjectId)
{
    vr::VRDriverLog()->Log(
        "[XboxVR][S3] ControllerDevice::Activate entered\n");

    device_id_ = unObjectId;

    const vr::PropertyContainerHandle_t container =
        vr::VRProperties()->TrackedDeviceToPropertyContainer(
            unObjectId);

    // -------------------------------------------------------------------------
    // Controller role
    // -------------------------------------------------------------------------

    vr::VRProperties()->SetInt32Property(
        container,
        vr::Prop_ControllerRoleHint_Int32,
        role_);

    // -------------------------------------------------------------------------
    // Controller identification
    // -------------------------------------------------------------------------

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_ManufacturerName_String,
        "XboxVR");

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_ModelNumber_String,
        "XboxVR Virtual Controller");

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_SerialNumber_String,
        role_ == vr::TrackedControllerRole_LeftHand
            ? "XboxVR_Left"
            : "XboxVR_Right");

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_ControllerType_String,
        "xbox360_controller");

    // -------------------------------------------------------------------------
    // Input profile
    // -------------------------------------------------------------------------

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_InputProfilePath_String,
        "{openvr-emulator}/resources/input/openvr-emulator_controller_bindings.json");

    // -------------------------------------------------------------------------
    // Boolean inputs
    // -------------------------------------------------------------------------

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/a/click",
        &input_handles_[kInputHandle_A_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/b/click",
        &input_handles_[kInputHandle_B_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/x/click",
        &input_handles_[kInputHandle_X_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/y/click",
        &input_handles_[kInputHandle_Y_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/system/click",
        &input_handles_[kInputHandle_System_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/menu/click",
        &input_handles_[kInputHandle_Menu_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/grip/click",
        &input_handles_[kInputHandle_Grip_click]);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/joystick/click",
        &input_handles_[kInputHandle_Joystick_click]);

    // -------------------------------------------------------------------------
    // Trigger
    // -------------------------------------------------------------------------

    vr::VRDriverInput()->CreateScalarComponent(
        container,
        "/input/trigger/value",
        &input_handles_[kInputHandle_Trigger_value],
        vr::VRScalarType_Absolute,
        vr::VRScalarUnits_NormalizedOneSided);

    vr::VRDriverInput()->CreateBooleanComponent(
        container,
        "/input/trigger/click",
        &input_handles_[kInputHandle_Trigger_click]);

    // -------------------------------------------------------------------------
    // Joystick
    // -------------------------------------------------------------------------

    vr::VRDriverInput()->CreateScalarComponent(
        container,
        "/input/joystick/x",
        &input_handles_[kInputHandle_Joystick_x],
        vr::VRScalarType_Absolute,
        vr::VRScalarUnits_NormalizedTwoSided);

    vr::VRDriverInput()->CreateScalarComponent(
        container,
        "/input/joystick/y",
        &input_handles_[kInputHandle_Joystick_y],
        vr::VRScalarType_Absolute,
        vr::VRScalarUnits_NormalizedTwoSided);

    // -------------------------------------------------------------------------
    // Haptic
    // -------------------------------------------------------------------------

    vr::VRDriverInput()->CreateHapticComponent(
        container,
        "/output/haptic",
        &input_handles_[kInputHandle_Haptic]);

    // -------------------------------------------------------------------------
    // Initialize XInput
    // -------------------------------------------------------------------------

    if (InitializeXInput())
    {
        vr::VRDriverLog()->Log(
            "[XboxVR][S3] Explicit xinput1_4.dll initialization SUCCESS\n");
    }
    else
    {
        vr::VRDriverLog()->Log(
            "[XboxVR][S3] Explicit xinput1_4.dll initialization FAILED\n");
    }

    vr::VRDriverLog()->Log(
        role_ == vr::TrackedControllerRole_LeftHand
            ? "[XboxVR][S3] LEFT controller input components created\n"
            : "[XboxVR][S3] RIGHT controller input components created\n");

    return vr::VRInitError_None;
}

// ============================================================================
// Update
// ============================================================================

void ControllerDevice::Update()
{
    XINPUT_STATE state{};

    const bool connected = ReadFirstConnectedXInput(state);

    if (!connected)
    {
        // Send neutral values when controller is disconnected.
        vr::VRDriverInput()->UpdateScalarComponent(
            input_handles_[kInputHandle_Joystick_x],
            0.0f,
            0.0);

        vr::VRDriverInput()->UpdateScalarComponent(
            input_handles_[kInputHandle_Joystick_y],
            0.0f,
            0.0);

        vr::VRDriverInput()->UpdateScalarComponent(
            input_handles_[kInputHandle_Trigger_value],
            0.0f,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_A_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_B_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_X_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_Y_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_System_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_Menu_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_Grip_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_Joystick_click],
            false,
            0.0);

        vr::VRDriverInput()->UpdateBooleanComponent(
            input_handles_[kInputHandle_Trigger_click],
            false,
            0.0);

        return;
    }

    // ========================================================================
    // Raw Xbox values
    // ========================================================================

    const WORD buttons = state.Gamepad.wButtons;

    // ========================================================================
    // Determine which physical Xbox controller inputs belong to this VR hand
    // ========================================================================

    const bool isLeft =
        role_ == vr::TrackedControllerRole_LeftHand;

    // Left VR controller:
    //   Left stick
    //   LT
    //   LB
    //   X
    //   Y
    //   Back
    //
    // Right VR controller:
    //   Right stick
    //   RT
    //   RB
    //   A
    //   B
    //   Start

    float joystickX;
    float joystickY;
    float trigger;

    bool a;
    bool b;
    bool x;
    bool y;
    bool system;
    bool menu;
    bool grip;
    bool joystickClick;

    if (isLeft)
    {
        joystickX = NormalizeStick(state.Gamepad.sThumbLX);
        joystickY = NormalizeStick(state.Gamepad.sThumbLY);

        trigger = NormalizeTrigger(
            state.Gamepad.bLeftTrigger);

        a = false;
        b = false;

        x = IsPressed(
            buttons,
            XINPUT_GAMEPAD_X);

        y = IsPressed(
            buttons,
            XINPUT_GAMEPAD_Y);

        system = IsPressed(
            buttons,
            XINPUT_GAMEPAD_BACK);

        menu = false;

        grip = IsPressed(
            buttons,
            XINPUT_GAMEPAD_LEFT_SHOULDER);

        joystickClick = IsPressed(
            buttons,
            XINPUT_GAMEPAD_LEFT_THUMB);
    }
    else
    {
        joystickX = NormalizeStick(state.Gamepad.sThumbRX);
        joystickY = NormalizeStick(state.Gamepad.sThumbRY);

        trigger = NormalizeTrigger(
            state.Gamepad.bRightTrigger);

        a = IsPressed(
            buttons,
            XINPUT_GAMEPAD_A);

        b = IsPressed(
            buttons,
            XINPUT_GAMEPAD_B);

        x = false;
        y = false;

        system = false;

        menu = IsPressed(
            buttons,
            XINPUT_GAMEPAD_START);

        grip = IsPressed(
            buttons,
            XINPUT_GAMEPAD_RIGHT_SHOULDER);

        joystickClick = IsPressed(
            buttons,
            XINPUT_GAMEPAD_RIGHT_THUMB);
    }

    // ========================================================================
    // Deadzone
    // ========================================================================

    constexpr float joystickDeadzone = 0.08f;

    joystickX = ApplyDeadzone(
        joystickX,
        joystickDeadzone);

    joystickY = ApplyDeadzone(
        joystickY,
        joystickDeadzone);

    // ========================================================================
    // Trigger click
    // ========================================================================

    constexpr float triggerClickThreshold = 0.75f;

    const bool triggerClick =
        trigger >= triggerClickThreshold;

    // ========================================================================
    // Update OpenVR components
    // ========================================================================

    vr::VRDriverInput()->UpdateScalarComponent(
        input_handles_[kInputHandle_Joystick_x],
        joystickX,
        0.0);

    vr::VRDriverInput()->UpdateScalarComponent(
        input_handles_[kInputHandle_Joystick_y],
        joystickY,
        0.0);

    vr::VRDriverInput()->UpdateScalarComponent(
        input_handles_[kInputHandle_Trigger_value],
        trigger,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_Trigger_click],
        triggerClick,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_A_click],
        a,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_B_click],
        b,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_X_click],
        x,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_Y_click],
        y,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_System_click],
        system,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_Menu_click],
        menu,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_Grip_click],
        grip,
        0.0);

    vr::VRDriverInput()->UpdateBooleanComponent(
        input_handles_[kInputHandle_Joystick_click],
        joystickClick,
        0.0);

    // ========================================================================
    // Diagnostic output
    // ========================================================================

    char buffer[768];

    snprintf(
        buffer,
        sizeof(buffer),
        "[XboxVR][S6/S7] %s "
        "LX=%.3f LY=%.3f RX=%.3f RY=%.3f "
        "LT=%.3f RT=%.3f Buttons=0x%04X\n",

        isLeft ? "LEFT" : "RIGHT",

        NormalizeStick(state.Gamepad.sThumbLX),
        NormalizeStick(state.Gamepad.sThumbLY),
        NormalizeStick(state.Gamepad.sThumbRX),
        NormalizeStick(state.Gamepad.sThumbRY),

        NormalizeTrigger(state.Gamepad.bLeftTrigger),
        NormalizeTrigger(state.Gamepad.bRightTrigger),

        static_cast<unsigned int>(buttons));

    vr::VRDriverLog()->Log(buffer);
}

// ============================================================================
// Pose
// ============================================================================

vr::DriverPose_t ControllerDevice::GetPose()
{
    vr::DriverPose_t pose{};

    pose.poseIsValid = true;
    pose.result = vr::TrackingResult_Running_OK;
    pose.deviceIsConnected = true;

    pose.qWorldFromDriverRotation.w = 1.0;
    pose.qDriverFromHeadRotation.w = 1.0;
    pose.qRotation.w = 1.0;

    vr::TrackedDevicePose_t hmdPose{};

    vr::VRServerDriverHost()->GetRawTrackedDevicePoses(
        0.0f,
        &hmdPose,
        1);

    if (hmdPose.bPoseIsValid)
    {
        pose.qRotation.w = 1.0;
        pose.qRotation.x = 0.0;
        pose.qRotation.y = 0.0;
        pose.qRotation.z = 0.0;

        pose.vecPosition[0] =
            hmdPose.mDeviceToAbsoluteTracking.m[0][3] +
            (role_ == vr::TrackedControllerRole_LeftHand
                ? -0.20f
                : 0.20f);

        pose.vecPosition[1] =
            hmdPose.mDeviceToAbsoluteTracking.m[1][3];

        pose.vecPosition[2] =
            hmdPose.mDeviceToAbsoluteTracking.m[2][3] -
            0.50f;
    }

    return pose;
}

// ============================================================================
// Deactivate
// ============================================================================

void ControllerDevice::Deactivate()
{
    device_id_ =
        vr::k_unTrackedDeviceIndexInvalid;
}

// ============================================================================
// Standby
// ============================================================================

void ControllerDevice::EnterStandby()
{
}

// ============================================================================
// GetComponent
// ============================================================================

void* ControllerDevice::GetComponent(
    const char* pchComponentNameAndVersion)
{
    return nullptr;
}

// ============================================================================
// DebugRequest
// ============================================================================

void ControllerDevice::DebugRequest(
    const char* pchRequest,
    char* pchResponseBuffer,
    uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}