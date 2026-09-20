#include "ControllerDevice.hpp"

#include <Windows.h>

namespace OpenVREmulatorDriver
{

ControllerDevice::ControllerDevice(
    std::string serial,
    Handedness handedness,
    InputConfig config)
    : serial_(std::move(serial)),
      handedness_(handedness),
      config_(std::move(config))
{
}

std::string ControllerDevice::GetSerial()
{
    return serial_;
}

void ControllerDevice::Update()
{
    // Controller ownership is intentionally disabled in the OpenVR emulation
    // driver. HIDMaestro owns the two SteamVR hands in the XboxVR hybrid build.
}

vr::TrackedDeviceIndex_t ControllerDevice::GetDeviceIndex()
{
    return device_index_;
}

DeviceType ControllerDevice::GetDeviceType()
{
    return DeviceType::CONTROLLER;
}

vr::EVRInitError ControllerDevice::Activate(uint32_t unObjectId)
{
    device_index_ = unObjectId;

    const auto container =
        vr::VRProperties()->TrackedDeviceToPropertyContainer(unObjectId);

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_ManufacturerName_String,
        "XboxVR");

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_ModelNumber_String,
        "XboxVR HIDMaestro Hybrid Controller");

    vr::VRProperties()->SetStringProperty(
        container,
        vr::Prop_SerialNumber_String,
        serial_.c_str());

    return vr::VRInitError_None;
}

void ControllerDevice::Deactivate()
{
    device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void ControllerDevice::EnterStandby()
{
}

void *ControllerDevice::GetComponent(const char *)
{
    return nullptr;
}

void ControllerDevice::DebugRequest(
    const char *,
    char *pchResponseBuffer,
    uint32_t unResponseBufferSize)
{
    if (pchResponseBuffer != nullptr && unResponseBufferSize > 0)
    {
        pchResponseBuffer[0] = '\0';
    }
}

vr::DriverPose_t ControllerDevice::GetPose()
{
    return last_pose_;
}

}  // namespace OpenVREmulatorDriver
