# XboxVR build notes

This first-pass XboxVR implementation deliberately keeps the original OpenVR device architecture and modifies only the controller input path plus the minimum configuration/build scaffolding needed for a reliable Windows build.

## Compilation contract

- Driver language standard: C++17.
- Driver external dependencies: OpenVR SDK + Windows SDK/XInput.
- Conan, GTest, and clang-tidy are not part of the driver compilation path.
- CI builds the driver with Windows MSVC 2022 and a pinned OpenVR SDK revision.
- Unit tests remain optional and are not enabled by default.
- Preview application remains optional and is disabled by default.

## Xbox input contract

One physical Xbox 360/XInput controller is scanned across all four XInput user slots. The first connected slot is used.

Left virtual hand: X, Y, LT, LB, LS/click, Back, D-pad trackpad.
Right virtual hand: A, B, RT, RB, RS/click, Start.
OpenVR haptics are sent to the physical Xbox rumble motors.

The HMD continues to use mouse look; Xbox input is no longer consumed by HMD movement.

## SteamVR profile contract

The original stable controller input profile is intentionally retained for this first Xbox pass. No experimental `remapping` or cross-driver `legacy_binding` resource was added here. Compatibility improvements can be layered on only after the physical controller/device path is confirmed working.


## v1.0.1 build-hardening fixes

- Added an explicit DirectXMath include to every translation unit that uses `DirectX::XMFLOAT*` or DirectXMath vector/quaternion functions. DirectXMath is header-only and is included with the Windows SDK.
- Defined `NOMINMAX` for the driver target and locally around Windows headers so MSVC's `min`/`max` macros cannot corrupt `std::min`/`std::max` expressions.
- Removed unnecessary `const` qualification from temporary HMD device pointers before calling the non-const `IVRDevice::GetPose()` interface.
- Kept the CI dependency model unchanged: OpenVR SDK + Windows SDK/XInput only.


## Diagnostic build
This source snapshot includes rate-limited `[XboxVR][S1]` through `[S7]` diagnostics for SteamVR `vrserver.txt`. See `XBOXVR_DIAGNOSTIC_BUILD.md`.
