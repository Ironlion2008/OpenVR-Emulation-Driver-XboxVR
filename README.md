# OpenVR Emulation Driver - XboxVR

A Windows-only OpenVR/SteamVR driver that exposes two virtual VR controllers driven by a standard Xbox 360/XInput controller.

This repository starts from the original OpenVR Emulation Driver and keeps the stable device architecture. The working KeyboardVR project was used only as a reference for build/CI organization; **no keyboard controller input code is included here**.

## Xbox 360 mapping

| Xbox 360 / XInput | Left VR controller | Right VR controller |
|---|---|---|
| X / Y | X / Y | — |
| A / B | — | A / B |
| LT / RT | Trigger | Trigger |
| LB / RB | Grip | Grip |
| LS / RS | Joystick + click | Joystick + click |
| Back / Start | System | System |
| D-pad | Trackpad directions | — |
| Haptic vibration | Left motor | Right motor |

The XInput Guide button is not exposed as a normal button by the standard XInput state used by the driver, so it is not mapped.

## Runtime behavior

The driver scans all four standard XInput user slots each update and uses the first connected controller. Disconnecting and reconnecting therefore does not require a driver rebuild. OpenVR haptic events are forwarded to the Xbox controller rumble motors.

The HMD keeps the original mouse-look behavior. Xbox input is dedicated to the virtual hands so the left stick and triggers are not consumed by the HMD.

## Configuration

Edit `driver_files/driver/openvr-emulator/resources/input_mapping.ini` and restart SteamVR. Deadzones, stick sensitivity, trigger thresholds, button bindings, and D-pad trackpad behavior can be changed without recompiling.

## Building

The driver itself has **no Conan or other package-manager dependency**. It only needs:

- Windows 10/11
- Visual Studio 2022 Build Tools / MSVC and Windows SDK
- CMake 3.16+
- The pinned OpenVR SDK

Fetch the OpenVR SDK with:

```powershell
.\scripts\fetch_openvr.ps1
```

Then configure/build with CMake or open the generated Visual Studio solution.

GitHub Actions uses the same Windows MSVC toolchain and builds the driver without Conan, GTest, or clang-tidy in the compilation path.

## Repository structure

- `driver_files/src/Driver/ControllerDevice.cpp` - Xbox/XInput to OpenVR controller mapping
- `driver_files/src/Driver/InputConfig.cpp` - INI configuration parser
- `driver_files/src/Driver/InputMath.hpp` - XInput axis/trigger normalization
- `driver_files/driver/openvr-emulator/resources/input/` - SteamVR controller profile and UI assets
- `.github/workflows/ci.yml` - Windows build validation

## License

MIT License — Copyright (c) 2020 Jacob Hilton (Terminal29), portions Copyright (c) 2026 omarekik

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
