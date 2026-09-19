#include "ControllerDevice.hpp"

#include <Windows.h>
#include <DirectXMath.h>
#include <Xinput.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <utility>

#include "InputMath.hpp"

using namespace DirectX;

namespace
{
using OpenVREmulatorDriver::Clamp01;
using OpenVREmulatorDriver::NormalizeThumbAxis;
using OpenVREmulatorDriver::NormalizeTrigger;

constexpr float kMillisecondsPerSecond = 1000.0f;
constexpr float kMinHapticDurationSeconds = 0.02f;
constexpr float kMaxMotorSpeed = 65535.0f;
constexpr float kJoystickTouchThreshold = 0.1f;
constexpr float kTrackpadDigitalValue = 1.0f;
constexpr float kVibrateYOffset = -0.2f;
constexpr float kVibrateAmplitude = 0.01f;
constexpr float kVibrateFrequency = 8.0f;
constexpr float kControllerXOffset = 0.2f;
constexpr float kControllerZOffset = -0.5f;
constexpr float kPi = 3.14159265358979323846f;

struct XInputSnapshot
{
    bool connected = false;
    DWORD user_index = 0;
    XINPUT_STATE state{};
    std::array<DWORD, XUSER_MAX_COUNT> slot_status{};
};

struct XInputDiagnosticState
{
    std::chrono::steady_clock::time_point last_log_time{};
    bool has_logged_once = false;
};

XInputDiagnosticState gXInputDiagnostic;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

struct SharedXInputRumbleState
{
    float left_motor = 0.0f;
    float right_motor = 0.0f;
    DWORD user_index = 0;
    bool has_user = false;
    std::chrono::steady_clock::time_point left_motor_end_time{};
    std::chrono::steady_clock::time_point right_motor_end_time{};
};

SharedXInputRumbleState gXInputRumbleState;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char *XInputStatusName(DWORD status) noexcept
{
    switch (status)
    {
        case ERROR_SUCCESS:
            return "ERROR_SUCCESS";
        case ERROR_DEVICE_NOT_CONNECTED:
            return "ERROR_DEVICE_NOT_CONNECTED";
        default:
            return "OTHER_ERROR";
    }
}

XInputSnapshot ReadFirstConnectedXInput()
{
    XInputSnapshot snapshot{};
    snapshot.slot_status.fill(ERROR_DEVICE_NOT_CONNECTED);

    // Scan every XInput slot so the diagnostic report can distinguish a missing
    // controller from a controller that is connected on a non-zero user index.
    for (DWORD userIndex = 0; userIndex < XUSER_MAX_COUNT; ++userIndex)
    {
        XINPUT_STATE state{};
        const DWORD result = XInputGetState(userIndex, &state);
        snapshot.slot_status[userIndex] = result;

        if (result == ERROR_SUCCESS && !snapshot.connected)
        {
            snapshot.connected = true;
            snapshot.user_index = userIndex;
            snapshot.state = state;
        }
    }

    return snapshot;
}

void QueueXInputRumble(bool leftMotor, float amplitude, float durationSeconds)
{
    const float safeDuration = std::isfinite(durationSeconds)
                                   ? std::fmax(durationSeconds, kMinHapticDurationSeconds)
                                   : kMinHapticDurationSeconds;
    const auto endTime = std::chrono::steady_clock::now() +
                         std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                             std::chrono::duration<float>(safeDuration));
    const float safeAmplitude = std::isfinite(amplitude) ? Clamp01(amplitude) : 0.0f;

    if (leftMotor)
    {
        gXInputRumbleState.left_motor = safeAmplitude;
        gXInputRumbleState.left_motor_end_time = endTime;
    }
    else
    {
        gXInputRumbleState.right_motor = safeAmplitude;
        gXInputRumbleState.right_motor_end_time = endTime;
    }
}

void ApplyXInputRumble(const XInputSnapshot &snapshot)
{
    const auto now = std::chrono::steady_clock::now();

    if (now >= gXInputRumbleState.left_motor_end_time)
    {
        gXInputRumbleState.left_motor = 0.0f;
    }
    if (now >= gXInputRumbleState.right_motor_end_time)
    {
        gXInputRumbleState.right_motor = 0.0f;
    }

    if (snapshot.connected)
    {
        gXInputRumbleState.user_index = snapshot.user_index;
        gXInputRumbleState.has_user = true;
    }

    if (!gXInputRumbleState.has_user)
    {
        return;
    }

    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = static_cast<WORD>(gXInputRumbleState.left_motor * kMaxMotorSpeed);
    vibration.wRightMotorSpeed = static_cast<WORD>(gXInputRumbleState.right_motor * kMaxMotorSpeed);

    const DWORD result = XInputSetState(gXInputRumbleState.user_index, &vibration);
    if (result != ERROR_SUCCESS)
    {
        gXInputRumbleState.has_user = false;
        gXInputRumbleState.left_motor = 0.0f;
        gXInputRumbleState.right_motor = 0.0f;
    }
}

bool ButtonPressed(const XInputSnapshot &snapshot, WORD mask) noexcept
{
    return snapshot.connected && mask != 0 && (snapshot.state.Gamepad.wButtons & mask) != 0;
}

float ApplySensitivity(float value, float sensitivity) noexcept
{
    const float scaled = value * std::abs(sensitivity);
    return std::fmax(-1.0f, std::fmin(scaled, 1.0f));
}

}  // namespace

namespace OpenVREmulatorDriver
{

ControllerDevice::ControllerDevice(std::string serial, ControllerDevice::Handedness handedness,
                                   InputConfig config)
    : serial_(std::move(serial)), handedness_(handedness), config_(config)
{
}

std::string ControllerDevice::GetSerial()
{
    return this->serial_;
}

void ControllerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
    {
        return;
    }

    const float deltaSeconds = static_cast<float>(GetDriver()->GetLastFrameTime().count()) /
                               kMillisecondsPerSecond;

    const auto events = GetDriver()->GetOpenVREvents();
    for (const auto &event : events)
    {
        if (event.eventType != vr::EVREventType::VREvent_Input_HapticVibration ||
            event.data.hapticVibration.componentHandle != this->haptic_component_)
        {
            continue;
        }

        this->did_vibrate_ = true;
        this->vibrate_anim_state_ = 0.0f;
        const bool leftMotor = this->handedness_ == Handedness::LEFT;
        QueueXInputRumble(leftMotor, event.data.hapticVibration.fAmplitude,
                          event.data.hapticVibration.fDurationSeconds);
    }

    if (this->did_vibrate_)
    {
        this->vibrate_anim_state_ += deltaSeconds;
        if (this->vibrate_anim_state_ >= 1.0f)
        {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    const XInputSnapshot snapshot = ReadFirstConnectedXInput();

    // Diagnostic logging is intentionally rate-limited so vrserver.txt remains readable.
    // One report per second contains the complete XInput scan plus the values that this
    // controller will send to SteamVR.
    const auto diagnosticNow = std::chrono::steady_clock::now();
    if (!gXInputDiagnostic.has_logged_once ||
        diagnosticNow - gXInputDiagnostic.last_log_time >= std::chrono::seconds(1))
    {
        gXInputDiagnostic.has_logged_once = true;
        gXInputDiagnostic.last_log_time = diagnosticNow;

        std::ostringstream diagnostic;
        diagnostic << "[XboxVR][S4] XInput scan: ";
        for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot)
        {
            diagnostic << "slot" << slot << "=" << XInputStatusName(snapshot.slot_status[slot]);
            if (slot + 1 < XUSER_MAX_COUNT)
            {
                diagnostic << ", ";
            }
        }

        if (snapshot.connected)
        {
            const auto &gamepad = snapshot.state.Gamepad;
            diagnostic << " | CONNECTED slot=" << snapshot.user_index
                       << " buttons=" << gamepad.wButtons
                       << " LX=" << gamepad.sThumbLX
                       << " LY=" << gamepad.sThumbLY
                       << " RX=" << gamepad.sThumbRX
                       << " RY=" << gamepad.sThumbRY
                       << " LT=" << static_cast<int>(gamepad.bLeftTrigger)
                       << " RT=" << static_cast<int>(gamepad.bRightTrigger);
        }
        else
        {
            diagnostic << " | NO XINPUT CONTROLLER";
        }

        GetDriver()->Log(diagnostic.str());
        GetDriver()->Log(std::string("[XboxVR][S5] XInput state ") +
                         (snapshot.connected ? "accepted from connected controller"
                                              : "not available; using neutral input values"));
    }

    const ControllerInputConfig &controllerConfig =
        this->handedness_ == Handedness::LEFT ? static_cast<const ControllerInputConfig &>(config_.left_controller)
                                              : static_cast<const ControllerInputConfig &>(config_.right_controller);

    auto pose = IVRDevice::MakeDefaultPose();

    // Controllers follow the HMD pose and sit slightly below/aside it.
    const auto devices = GetDriver()->GetDevices();
    IVRDevice *hmdDevice = nullptr;
    for (const auto &device : devices)
    {
        if (device->GetDeviceType() == DeviceType::HMD)
        {
            hmdDevice = device.get();
            break;
        }
    }

    if (hmdDevice != nullptr)
    {
        const vr::DriverPose_t hmdPose = hmdDevice->GetPose();
        const XMFLOAT3 hmdPosition{static_cast<float>(hmdPose.vecPosition[0]),
                                   static_cast<float>(hmdPose.vecPosition[1]),
                                   static_cast<float>(hmdPose.vecPosition[2])};
        const XMFLOAT4 hmdRotation{static_cast<float>(hmdPose.qRotation.x),
                                   static_cast<float>(hmdPose.qRotation.y),
                                   static_cast<float>(hmdPose.qRotation.z),
                                   static_cast<float>(hmdPose.qRotation.w)};

        const float controllerY =
            kVibrateYOffset +
            kVibrateAmplitude *
                std::sin(kVibrateFrequency * kPi * this->vibrate_anim_state_);
        const float controllerX = this->handedness_ == Handedness::LEFT
                                      ? -kControllerXOffset
                                      : (this->handedness_ == Handedness::RIGHT ? kControllerXOffset : 0.0f);

        const XMFLOAT3 offset{controllerX, controllerY, kControllerZOffset};
        XMFLOAT3 rotatedOffset{};
        XMStoreFloat3(&rotatedOffset,
                      XMVector3Rotate(XMLoadFloat3(&offset), XMLoadFloat4(&hmdRotation)));

        pose.vecPosition[0] = rotatedOffset.x + hmdPosition.x;
        pose.vecPosition[1] = rotatedOffset.y + hmdPosition.y;
        pose.vecPosition[2] = rotatedOffset.z + hmdPosition.z;
        pose.qRotation = hmdPose.qRotation;
    }

    const bool aPressed = ButtonPressed(snapshot, controllerConfig.btn_a);
    const bool bPressed = ButtonPressed(snapshot, controllerConfig.btn_b);
    const bool xPressed = ButtonPressed(snapshot, controllerConfig.btn_x);
    const bool yPressed = ButtonPressed(snapshot, controllerConfig.btn_y);
    const bool gripPressed = ButtonPressed(snapshot, controllerConfig.btn_grip);
    const bool systemPressed = ButtonPressed(snapshot, controllerConfig.btn_system);
    const bool joystickClick = ButtonPressed(snapshot, controllerConfig.btn_joystick_click);

    const SHORT stickX = this->handedness_ == Handedness::LEFT
                             ? snapshot.state.Gamepad.sThumbLX
                             : snapshot.state.Gamepad.sThumbRX;
    const SHORT stickY = this->handedness_ == Handedness::LEFT
                             ? snapshot.state.Gamepad.sThumbLY
                             : snapshot.state.Gamepad.sThumbRY;
    const BYTE triggerRaw = this->handedness_ == Handedness::LEFT
                                ? snapshot.state.Gamepad.bLeftTrigger
                                : snapshot.state.Gamepad.bRightTrigger;

    float joystickX = NormalizeThumbAxis(stickX, controllerConfig.stick_deadzone);
    float joystickY = NormalizeThumbAxis(stickY, controllerConfig.stick_deadzone);
    joystickX = ApplySensitivity(joystickX, controllerConfig.stick_sensitivity);
    joystickY = ApplySensitivity(joystickY, controllerConfig.stick_sensitivity);

    const float triggerValue = NormalizeTrigger(triggerRaw, controllerConfig.trigger_deadzone);
    const float triggerClickThreshold = Clamp01(controllerConfig.trigger_click_threshold);

    if (gXInputDiagnostic.has_logged_once &&
        diagnosticNow - gXInputDiagnostic.last_log_time < std::chrono::milliseconds(50))
    {
        std::ostringstream values;
        values << "[XboxVR][S6] "
               << (this->handedness_ == Handedness::LEFT ? "LEFT" : "RIGHT")
               << " normalized: joystickX=" << joystickX
               << " joystickY=" << joystickY
               << " trigger=" << triggerValue
               << " triggerClick=" << (triggerValue >= triggerClickThreshold ? 1 : 0);
        GetDriver()->Log(values.str());
    }

    const WORD dpadHorizontalMask = XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
    const WORD dpadVerticalMask = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
    const bool dpadLeft = ButtonPressed(snapshot, XINPUT_GAMEPAD_DPAD_LEFT);
    const bool dpadRight = ButtonPressed(snapshot, XINPUT_GAMEPAD_DPAD_RIGHT);
    const bool dpadUp = ButtonPressed(snapshot, XINPUT_GAMEPAD_DPAD_UP);
    const bool dpadDown = ButtonPressed(snapshot, XINPUT_GAMEPAD_DPAD_DOWN);
    const bool dpadPressed = ButtonPressed(snapshot, dpadHorizontalMask) ||
                             ButtonPressed(snapshot, dpadVerticalMask);

    float trackpadX = 0.0f;
    float trackpadY = 0.0f;
    if (controllerConfig.dpad_to_trackpad)
    {
        trackpadX = (dpadRight ? kTrackpadDigitalValue : 0.0f) -
                    (dpadLeft ? kTrackpadDigitalValue : 0.0f);
        trackpadY = (dpadUp ? kTrackpadDigitalValue : 0.0f) -
                    (dpadDown ? kTrackpadDigitalValue : 0.0f);
    }

    auto updateButton = [this](vr::VRInputComponentHandle_t click,
                               vr::VRInputComponentHandle_t touch, bool pressed) {
        GetDriver()->GetInput()->UpdateBooleanComponent(click, pressed, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(touch, pressed, 0);
    };
    auto updateScalar = [this](vr::VRInputComponentHandle_t component, float value) {
        GetDriver()->GetInput()->UpdateScalarComponent(component, value, 0);
    };

    updateButton(this->a_button_click_component_, this->a_button_touch_component_, aPressed);
    updateButton(this->b_button_click_component_, this->b_button_touch_component_, bPressed);
    updateButton(this->x_button_click_component_, this->x_button_touch_component_, xPressed);
    updateButton(this->y_button_click_component_, this->y_button_touch_component_, yPressed);

    GetDriver()->GetInput()->UpdateBooleanComponent(
        this->trigger_click_component_, triggerValue >= triggerClickThreshold, 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(
        this->trigger_touch_component_, triggerValue > 0.0f, 0);
    updateScalar(this->trigger_value_component_, triggerValue);

    GetDriver()->GetInput()->UpdateBooleanComponent(
        this->grip_touch_component_, gripPressed, 0);
    updateScalar(this->grip_value_component_, gripPressed ? 1.0f : 0.0f);
    updateScalar(this->grip_force_component_, gripPressed ? 1.0f : 0.0f);

    updateButton(this->system_click_component_, this->system_touch_component_, systemPressed);

    updateButton(this->trackpad_click_component_, this->trackpad_touch_component_, dpadPressed);
    updateScalar(this->trackpad_x_component_, trackpadX);
    updateScalar(this->trackpad_y_component_, trackpadY);

    GetDriver()->GetInput()->UpdateBooleanComponent(
        this->joystick_click_component_, joystickClick, 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(
        this->joystick_touch_component_,
        joystickClick || std::abs(joystickX) > kJoystickTouchThreshold ||
            std::abs(joystickY) > kJoystickTouchThreshold, 0);
    updateScalar(this->joystick_x_component_, joystickX);
    updateScalar(this->joystick_y_component_, joystickY);

    if (gXInputDiagnostic.has_logged_once &&
        diagnosticNow - gXInputDiagnostic.last_log_time < std::chrono::milliseconds(50))
    {
        GetDriver()->Log(std::string("[XboxVR][S7] ") +
                         (this->handedness_ == Handedness::LEFT ? "LEFT" : "RIGHT") +
                         " OpenVR input components updated");
    }

    ApplyXInputRumble(snapshot);

    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose,
                                                            sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType ControllerDevice::GetDeviceType()
{
    return DeviceType::CONTROLLER;
}

vr::TrackedDeviceIndex_t ControllerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError ControllerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("[XboxVR][S3] Activating Xbox/XInput controller " + this->serial_);
    const auto props =
        GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic", &this->haptic_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/click",
                                                    &this->a_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/touch",
                                                    &this->a_button_touch_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/click",
                                                    &this->b_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/touch",
                                                    &this->b_button_touch_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/click",
                                                    &this->x_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/touch",
                                                    &this->x_button_touch_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/click",
                                                    &this->y_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/touch",
                                                    &this->y_button_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/click",
                                                    &this->trigger_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/touch",
                                                    &this->trigger_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/trigger/value", &this->trigger_value_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/grip/touch",
                                                    &this->grip_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/grip/value", &this->grip_value_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/grip/force", &this->grip_force_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click",
                                                    &this->system_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/touch",
                                                    &this->system_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trackpad/click",
                                                    &this->trackpad_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trackpad/touch",
                                                    &this->trackpad_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/trackpad/x", &this->trackpad_x_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/trackpad/y", &this->trackpad_y_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/click",
                                                    &this->joystick_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/touch",
                                                    &this->joystick_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/joystick/x", &this->joystick_x_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/joystick/y", &this->joystick_y_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

    std::ostringstream activation;
    activation << "[XboxVR][S3] "
               << (this->handedness_ == Handedness::LEFT ? "LEFT" : "RIGHT")
               << " input components created: joystick=(" << this->joystick_x_component_
               << "," << this->joystick_y_component_ << ") trigger="
               << this->trigger_value_component_ << " grip=" << this->grip_value_component_;
    GetDriver()->Log(activation.str());

    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String,
                                                    "openvr-emulator_controller");

    const std::string renderModelName =
        this->handedness_ == Handedness::LEFT ? "oculus_quest_plus_controller_left"
                                              : "oculus_quest_plus_controller_right";
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String,
                                                    renderModelName.c_str());

    const auto role = this->handedness_ == Handedness::LEFT
                          ? vr::ETrackedControllerRole::TrackedControllerRole_LeftHand
                          : vr::ETrackedControllerRole::TrackedControllerRole_RightHand;
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, role);

    // Keep the source project's stable custom controller type and compatibility profile.
    // This first Xbox pass intentionally avoids experimental remapping/legacy JSON changes.
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_ControllerType_String, "openvr-emulator_controller");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_InputProfilePath_String,
        "{openvr-emulator}/input/openvr-emulator_controller_bindings.json");

    const std::string handedness =
        this->handedness_ == Handedness::LEFT ? "left" : "right";
    const std::string readyIcon = "{openvr-emulator}/icons/controller_ready_" + handedness + ".png";
    const std::string notReadyIcon = "{openvr-emulator}/icons/controller_not_ready_" + handedness + ".png";
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceReady_String,
                                                    readyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceOff_String,
                                                    notReadyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceSearching_String,
                                                    notReadyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceSearchingAlert_String,
                                                    notReadyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceReadyAlert_String,
                                                    readyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceNotReady_String,
                                                    notReadyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceStandby_String,
                                                    notReadyIcon.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceAlertLow_String,
                                                    notReadyIcon.c_str());

    return vr::EVRInitError::VRInitError_None;
}

void ControllerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void ControllerDevice::EnterStandby() {}

void *ControllerDevice::GetComponent(const char * /*pchComponentNameAndVersion*/)
{
    return nullptr;
}

void ControllerDevice::DebugRequest(const char * /*pchRequest*/, char *pchResponseBuffer,
                                    uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize > 0)
    {
        *pchResponseBuffer = 0;
    }
}

vr::DriverPose_t ControllerDevice::GetPose()
{
    return this->last_pose_;
}

}  // namespace OpenVREmulatorDriver