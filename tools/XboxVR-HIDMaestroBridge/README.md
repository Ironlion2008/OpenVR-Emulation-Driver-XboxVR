# XboxVR + HIDMaestro bridge

This bridge deliberately separates the two jobs:

- OpenVR-Emulation-Driver: HMD, mouse/HMD emulation, tracker/reference devices.
- HIDMaestro: the two SteamVR hands and their modern/legacy input profile.
- XboxVR-HIDMaestroBridge: reads the physical Xbox 360 through XInput and submits the state to HIDMaestro.

The old OpenVR-Emulation controller devices are disabled so SteamVR does not get duplicate hands.

## Mapping

Left: left stick, LT, LB, X, Y, Back, L3.
Right: right stick, RT, RB, A, B, Start, R3.

The bridge intentionally does not use Xbox input for HMD movement.
