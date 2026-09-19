#!/usr/bin/env python3
"""Static resource/source sanity checks for the XboxVR driver."""
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []

profile = ROOT / "driver_files/driver/openvr-emulator/resources/input/openvr-emulator_controller_bindings.json"
manifest = ROOT / "driver_files/driver/openvr-emulator/driver.vrdrivermanifest"
mapping = ROOT / "driver_files/driver/openvr-emulator/resources/input_mapping.ini"

for path in (profile, manifest, mapping):
    if not path.exists():
        errors.append(f"Missing required resource: {path}")

for path in (profile, manifest):
    if path.exists():
        try:
            json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:  # noqa: BLE001
            errors.append(f"Invalid JSON {path}: {exc}")

controller_sources = [
    ROOT / "driver_files/src/Driver/ControllerDevice.cpp",
    ROOT / "driver_files/src/Driver/ControllerDevice.hpp",
    ROOT / "driver_files/src/Driver/InputConfig.cpp",
    ROOT / "driver_files/src/Driver/InputConfig.hpp",
]
source = "\n".join(p.read_text(encoding="utf-8") for p in controller_sources)
for forbidden in ("GetAsyncKeyState", "keyboard_only", "KeyboardControllerConfig", "key_a", "key_b", "keyboard"):
    if forbidden in source:
        errors.append(f"Keyboard-specific controller code leaked into XboxVR: {forbidden}")

if profile.exists():
    data = json.loads(profile.read_text(encoding="utf-8"))
    if data.get("controller_type") != "openvr-emulator_controller":
        errors.append("Controller profile type is not openvr-emulator_controller")
    if "remapping" in data or "legacy_binding" in data:
        errors.append("Experimental SteamVR remapping/legacy binding must not be present in the first-pass profile")
    sources = data.get("input_source", {})
    for required in ("/input/a", "/input/b", "/input/x", "/input/y", "/input/trigger", "/input/grip", "/input/system", "/input/joystick"):
        if required not in sources:
            errors.append(f"Controller profile missing input source: {required}")
    if "/input/skeleton/left" in sources or "/input/skeleton/right" in sources:
        errors.append("Controller profile contains skeleton inputs that the driver does not create")

if mapping.exists():
    mapping_text = mapping.read_text(encoding="utf-8")
    for section in ("[left_controller]", "[right_controller]"):
        if section not in mapping_text:
            errors.append(f"Mapping file missing section: {section}")

if errors:
    print("XboxVR validation FAILED")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("XboxVR validation PASSED")
print(f"Checked {len(controller_sources)} Xbox controller source/header files and controller resources.")
