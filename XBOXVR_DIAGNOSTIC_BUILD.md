# XboxVR Diagnostic Build

This build adds rate-limited diagnostic messages to the SteamVR driver log. It is intended to locate the exact stage where Xbox/XInput input stops reaching the virtual controllers.

## What to do

1. Build/install this driver in SteamVR.
2. Fully restart SteamVR.
3. Make sure the Xbox 360/XInput controller is connected.
4. Move the left stick, right stick, press LT/RT, LB/RB, A/B/X/Y.
5. Open:

`C:\Program Files (x86)\Steam\logs\vrserver.txt`

6. Extract the XboxVR lines with PowerShell:

```powershell
Select-String -Path "C:\Program Files (x86)\Steam\logs\vrserver.txt" -Pattern "\[XboxVR\]" | Set-Content "$env:USERPROFILE\Desktop\xboxvr-diagnostic.txt"
notepad "$env:USERPROFILE\Desktop\xboxvr-diagnostic.txt"
```

## Diagnostic stages

- `[S1]` Driver initialization entered.
- `[S2]` Virtual left/right controllers were successfully registered with SteamVR.
- `[S3]` A controller was activated and its OpenVR input component handles were created.
- `[S4]` XInput was scanned. Every XInput slot (0-3) is reported, plus raw Xbox values when connected.
- `[S5]` The first connected XInput state was selected, or neutral values are being used when no controller is available.
- `[S6]` Raw stick/trigger values were normalized and are about to be sent through the existing OpenVR input update path.
- `[S7]` OpenVR input component update calls were reached.

## Interpretation

- No `[S1]`: SteamVR did not load this driver build.
- `[S2] ... FAILED`: virtual controller registration failed.
- `[S3]` missing: controller activation/component creation is failing.
- `[S4]` shows all `ERROR_DEVICE_NOT_CONNECTED`: the driver is not receiving an XInput controller.
- `[S4]` shows `CONNECTED` and changing raw values: Xbox -> XInput works.
- `[S4]` changes but `[S6]` values do not: normalization/mapping logic needs investigation.
- `[S6]` changes correctly while SteamVR/game input does not react: investigate the OpenVR input profile/binding side next.

Do not change `resources/input_mapping.ini` while collecting the first diagnostic log. The current source already maps the left/right analog sticks and triggers directly from the XInput state.


## v1.0.3 fix
The driver-root resolver now walks from `bin\win64\driver_openvr-emulator.dll` up two directories to the `openvr-emulator` driver root, so `resources\input_mapping.ini` resolves from the installed SteamVR driver directory.
