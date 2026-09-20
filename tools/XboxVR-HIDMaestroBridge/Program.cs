using System.Diagnostics;
using System.Reflection;
using System.Linq.Expressions;
using System.Runtime.InteropServices;

namespace XboxVR.HIDMaestroBridge;

internal static class Program
{
    [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")]
    private static extern uint XInputGetState14(uint userIndex, out XINPUT_STATE state);

    [DllImport("xinput1_3.dll", EntryPoint = "XInputGetState")]
    private static extern uint XInputGetState13(uint userIndex, out XINPUT_STATE state);

    [DllImport("xinput9_1_0.dll", EntryPoint = "XInputGetState")]
    private static extern uint XInputGetState910(uint userIndex, out XINPUT_STATE state);

    [DllImport("xinput1_4.dll", EntryPoint = "XInputSetState")]
    private static extern uint XInputSetState14(uint userIndex, ref XINPUT_VIBRATION vibration);

    [StructLayout(LayoutKind.Sequential)]
    private struct XINPUT_VIBRATION { public ushort wLeftMotorSpeed; public ushort wRightMotorSpeed; }

    [StructLayout(LayoutKind.Sequential)]
    private struct XINPUT_STATE { public uint PacketNumber; public XINPUT_GAMEPAD Gamepad; }

    [StructLayout(LayoutKind.Sequential)]
    private struct XINPUT_GAMEPAD
    {
        public ushort wButtons;
        public byte bLeftTrigger;
        public byte bRightTrigger;
        public short sThumbLX;
        public short sThumbLY;
        public short sThumbRX;
        public short sThumbRY;
    }

    private const ushort XINPUT_GAMEPAD_DPAD_UP = 0x0001;
    private const ushort XINPUT_GAMEPAD_DPAD_DOWN = 0x0002;
    private const ushort XINPUT_GAMEPAD_DPAD_LEFT = 0x0004;
    private const ushort XINPUT_GAMEPAD_DPAD_RIGHT = 0x0008;
    private const ushort XINPUT_GAMEPAD_START = 0x0010;
    private const ushort XINPUT_GAMEPAD_BACK = 0x0020;
    private const ushort XINPUT_GAMEPAD_LEFT_THUMB = 0x0040;
    private const ushort XINPUT_GAMEPAD_RIGHT_THUMB = 0x0080;
    private const ushort XINPUT_GAMEPAD_LEFT_SHOULDER = 0x0100;
    private const ushort XINPUT_GAMEPAD_RIGHT_SHOULDER = 0x0200;
    private const ushort XINPUT_GAMEPAD_A = 0x1000;
    private const ushort XINPUT_GAMEPAD_B = 0x2000;
    private const ushort XINPUT_GAMEPAD_X = 0x4000;
    private const ushort XINPUT_GAMEPAD_Y = 0x8000;

    private const uint ERROR_SUCCESS = 0;
    private const uint ERROR_DEVICE_NOT_CONNECTED = 1167;

    private static MethodInfo? _submitState;
    private static object? _vrController;
    private static Type? _stateType;
    private static Type? _handStateType;
    private static MemberInfo? _leftMember;
    private static MemberInfo? _rightMember;

    private static int Main()
    {
        Console.Title = "XboxVR — HIDMaestro Bridge";
        Console.WriteLine("=== XboxVR HIDMaestro Bridge ===");
        Console.WriteLine("Xbox 360 -> HIDMaestro virtual SteamVR hands");
        Console.WriteLine();

        try
        {
            var corePath = Path.Combine(AppContext.BaseDirectory, "HIDMaestro.Core.dll");
            if (!File.Exists(corePath))
            {
                Console.Error.WriteLine($"[FATAL] Missing: {corePath}");
                Console.Error.WriteLine("Run setup_hidmaestro.ps1 first.");
                return 2;
            }

            var asm = Assembly.LoadFrom(corePath);
            Console.WriteLine($"[OK] HIDMaestro.Core loaded: {asm.GetName().Version}");

            EnsureHidMaestroRegistered(asm);
            CreateVrController(asm);

            Console.WriteLine("[OK] HIDMaestro virtual hands connected.");
            Console.WriteLine("[OK] Waiting for Xbox controller slot 0...");
            Console.WriteLine();
            Console.WriteLine("Mapping:");
            Console.WriteLine("  Left stick  -> LEFT joystick");
            Console.WriteLine("  LT          -> LEFT trigger");
            Console.WriteLine("  LB          -> LEFT grip");
            Console.WriteLine("  X/Y         -> LEFT A/B");
            Console.WriteLine("  Back        -> LEFT system");
            Console.WriteLine("  Right stick -> RIGHT joystick");
            Console.WriteLine("  RT          -> RIGHT trigger");
            Console.WriteLine("  RB          -> RIGHT grip");
            Console.WriteLine("  A/B         -> RIGHT A/B");
            Console.WriteLine("  Start       -> RIGHT system");
            Console.WriteLine("  L3/R3       -> joystick click");
            Console.WriteLine();

            RunLoop();
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("[FATAL] " + ex);
            return 1;
        }
    }

    private static void EnsureHidMaestroRegistered(Assembly asm)
    {
        var hmvr = asm.GetType("HIDMaestro.HMVR")
                   ?? throw new InvalidOperationException("HIDMaestro.HMVR type not found.");
        var ensure = hmvr.GetMethod("EnsureDriverRegistered", BindingFlags.Public | BindingFlags.Static)
                     ?? throw new InvalidOperationException("HMVR.EnsureDriverRegistered not found.");

        try
        {
            var result = ensure.Invoke(null, null);
            Console.WriteLine($"[OK] HIDMaestro SteamVR driver registration: {result}");
        }
        catch (TargetInvocationException tie) when (tie.InnerException != null)
        {
            throw new InvalidOperationException(
                "HIDMaestro driver registration failed. Run setup_hidmaestro.ps1 as Administrator once, then start this bridge normally.",
                tie.InnerException);
        }
    }

    private static void CreateVrController(Assembly asm)
    {
        var controllerType = asm.GetType("HIDMaestro.HMVRController")
            ?? throw new InvalidOperationException("HIDMaestro.HMVRController type not found.");
        _stateType = asm.GetType("HIDMaestro.HMVRState")
            ?? throw new InvalidOperationException("HIDMaestro.HMVRState type not found.");

        _vrController = Activator.CreateInstance(controllerType)
            ?? throw new InvalidOperationException("Could not create HMVRController.");

        _submitState = controllerType.GetMethod("SubmitState", BindingFlags.Public | BindingFlags.Instance)
            ?? throw new InvalidOperationException("HMVRController.SubmitState not found.");

        SubscribeHaptics(controllerType, _vrController);

        Console.WriteLine($"[OK] HMVRController created. DriverConnected={GetBoolProperty(controllerType, _vrController, "DriverConnected")}, ControllersLive={GetBoolProperty(controllerType, _vrController, "ControllersLive")}");

        DumpStateLayout();
    }

    private static bool GetBoolProperty(Type t, object instance, string name)
        => t.GetProperty(name, BindingFlags.Public | BindingFlags.Instance)?.GetValue(instance) is bool b && b;

    private static void SubscribeHaptics(Type controllerType, object controller)
    {
        var evt = controllerType.GetEvent("HapticReceived", BindingFlags.Public | BindingFlags.Instance);
        if (evt?.EventHandlerType == null || evt.AddMethod == null)
        {
            Console.WriteLine("[WARN] HIDMaestro HapticReceived event not found; rumble feedback disabled.");
            return;
        }

        var invoke = evt.EventHandlerType.GetMethod("Invoke")
            ?? throw new InvalidOperationException("HIDMaestro haptic delegate has no Invoke method.");
        var parameters = invoke.GetParameters();
        var lambdaParameters = parameters.Select(p => Expression.Parameter(p.ParameterType, p.Name ?? "arg")).ToArray();
        var sender = Expression.Convert(lambdaParameters[0], typeof(object));
        var args = Expression.Convert(lambdaParameters[1], typeof(object));
        var callback = typeof(Program).GetMethod(nameof(OnHaptic), BindingFlags.NonPublic | BindingFlags.Static)!;
        var body = Expression.Call(callback, sender, args);
        var lambda = Expression.Lambda(evt.EventHandlerType, body, lambdaParameters).Compile();
        evt.AddMethod.Invoke(controller, new object[] { lambda });
        Console.WriteLine("[OK] HIDMaestro haptic callback connected.");
    }

    private static void OnHaptic(object? sender, object args)
    {
        try
        {
            var t = args.GetType();
            float amplitude = GetFloat(t, args, "Amplitude");
            float duration = GetFloat(t, args, "Duration");
            string hand = GetText(t, args, "Hand", "Controller", "ControllerIndex");

            ushort speed = (ushort)Math.Clamp((int)(amplitude * 65535f), 0, 65535);
            var vibration = new XINPUT_VIBRATION();
            if (hand.Contains("left", StringComparison.OrdinalIgnoreCase)) vibration.wLeftMotorSpeed = speed;
            else if (hand.Contains("right", StringComparison.OrdinalIgnoreCase)) vibration.wRightMotorSpeed = speed;
            else { vibration.wLeftMotorSpeed = speed; vibration.wRightMotorSpeed = speed; }
            XInputSetState14(0, ref vibration);

            int ms = Math.Clamp((int)(duration * 1000f), 1, 1000);
            _ = Task.Run(async () =>
            {
                await Task.Delay(ms);
                var stop = new XINPUT_VIBRATION();
                XInputSetState14(0, ref stop);
            });
        }
        catch (Exception ex)
        {
            Console.WriteLine("[HAPTIC] " + ex.Message);
        }
    }

    private static float GetFloat(Type t, object o, string name)
    {
        var m = t.GetMember(name, BindingFlags.Public | BindingFlags.Instance).FirstOrDefault();
        var v = m is PropertyInfo p ? p.GetValue(o) : m is FieldInfo f ? f.GetValue(o) : null;
        return v == null ? 0f : Convert.ToSingle(v);
    }

    private static string GetText(Type t, object o, params string[] names)
    {
        foreach (var name in names)
        {
            var m = t.GetMember(name, BindingFlags.Public | BindingFlags.Instance).FirstOrDefault();
            var v = m is PropertyInfo p ? p.GetValue(o) : m is FieldInfo f ? f.GetValue(o) : null;
            if (v != null) return v.ToString() ?? string.Empty;
        }
        return string.Empty;
    }

    private static void DumpStateLayout()
    {
        if (_stateType == null) return;
        Console.WriteLine("[INFO] HMVRState members: " + string.Join(", ", _stateType.GetMembers(BindingFlags.Public | BindingFlags.Instance).Where(m => m.MemberType is MemberTypes.Field or MemberTypes.Property).Select(m => m.Name)));
    }

    private static void RunLoop()
    {
        var lastPacket = uint.MaxValue;
        var lastConnected = false;
        var diagnosticTimer = Stopwatch.StartNew();

        while (true)
        {
            var rc = ReadXInput(0, out var state);
            var connected = rc == ERROR_SUCCESS;

            if (connected)
            {
                if (!lastConnected)
                {
                    Console.WriteLine("[XINPUT] Xbox controller CONNECTED on slot 0.");
                    lastConnected = true;
                }

                if (state.PacketNumber != lastPacket || diagnosticTimer.ElapsedMilliseconds >= 1000)
                {
                    lastPacket = state.PacketNumber;
                    diagnosticTimer.Restart();
                    Console.WriteLine($"[XINPUT] LX={state.Gamepad.sThumbLX,6} LY={state.Gamepad.sThumbLY,6} RX={state.Gamepad.sThumbRX,6} RY={state.Gamepad.sThumbRY,6} LT={state.Gamepad.bLeftTrigger,3} RT={state.Gamepad.bRightTrigger,3} BTN=0x{state.Gamepad.wButtons:X4}");
                }

                SubmitMappedState(state.Gamepad);
            }
            else
            {
                if (lastConnected)
                {
                    Console.WriteLine($"[XINPUT] Xbox controller disconnected. XInput error={rc}.");
                    lastConnected = false;
                }
                SubmitNeutralState();
            }

            Thread.Sleep(5);
        }
    }

    private static uint ReadXInput(uint slot, out XINPUT_STATE state)
    {
        var rc = XInputGetState14(slot, out state);
        if (rc == ERROR_SUCCESS || rc != ERROR_DEVICE_NOT_CONNECTED) return rc;
        rc = XInputGetState13(slot, out state);
        if (rc == ERROR_SUCCESS || rc != ERROR_DEVICE_NOT_CONNECTED) return rc;
        return XInputGetState910(slot, out state);
    }

    private static void SubmitMappedState(XINPUT_GAMEPAD pad)
    {
        if (_stateType == null || _submitState == null || _vrController == null)
            return;

        var state = Activator.CreateInstance(_stateType)!;
        var left = CreateHandState(_stateType, "Left", pad.sThumbLX, pad.sThumbLY,
            pad.bLeftTrigger / 255f, (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0,
            (pad.wButtons & XINPUT_GAMEPAD_X) != 0, (pad.wButtons & XINPUT_GAMEPAD_Y) != 0,
            (pad.wButtons & XINPUT_GAMEPAD_BACK) != 0, (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
        var right = CreateHandState(_stateType, "Right", pad.sThumbRX, pad.sThumbRY,
            pad.bRightTrigger / 255f, (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0,
            (pad.wButtons & XINPUT_GAMEPAD_A) != 0, (pad.wButtons & XINPUT_GAMEPAD_B) != 0,
            (pad.wButtons & XINPUT_GAMEPAD_START) != 0, (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);

        if (!TrySetHand(ref state, "Left", left) || !TrySetHand(ref state, "Right", right))
        {
            throw new InvalidOperationException("Could not map HMVRState hands. See the printed HMVRState members and HIDMaestro version.");
        }

        InvokeSubmit(state);
    }

    private static void SubmitNeutralState()
    {
        if (_stateType == null || _submitState == null || _vrController == null) return;
        var state = Activator.CreateInstance(_stateType)!;
        var left = CreateHandState(_stateType, "Left", 0, 0, 0, false, false, false, false, false);
        var right = CreateHandState(_stateType, "Right", 0, 0, 0, false, false, false, false, false);
        TrySetHand(ref state, "Left", left);
        TrySetHand(ref state, "Right", right);
        InvokeSubmit(state);
    }

    private static object CreateHandState(Type stateType, string handName, short sx, short sy, float trigger,
        bool gripClick, bool a, bool b, bool system, bool stickClick)
    {
        var members = stateType.GetMembers(BindingFlags.Public | BindingFlags.Instance);
        var handMember = members.FirstOrDefault(m => m.Name.Equals(handName, StringComparison.OrdinalIgnoreCase) ||
                                                      m.Name.Equals(handName + "Hand", StringComparison.OrdinalIgnoreCase));
        if (handMember == null) throw new InvalidOperationException($"HMVRState has no {handName} hand member.");
        var handType = GetMemberType(handMember) ?? throw new InvalidOperationException("Invalid hand member type.");
        _handStateType = handType;
        var hand = Activator.CreateInstance(handType)!;

        uint bits = 0;
        if (system) bits |= 1u << 0;
        if (a) bits |= 1u << 1;
        if (b) bits |= 1u << 3;
        if (trigger >= 0.5f) bits |= 1u << 5;
        if (gripClick) bits |= 1u << 6;
        if (stickClick) bits |= 1u << 7;

        SetAny(ref hand, handType, new[] { "ButtonBits", "Buttons" }, bits);
        SetAny(ref hand, handType, new[] { "Trigger" }, trigger);
        SetAny(ref hand, handType, new[] { "Grip" }, gripClick ? 1f : 0f);
        SetAny(ref hand, handType, new[] { "StickX", "JoystickX" }, NormalizeStick(sx));
        SetAny(ref hand, handType, new[] { "StickY", "JoystickY" }, -NormalizeStick(sy));
        return hand;
    }

    private static bool TrySetHand(ref object state, string handName, object hand)
    {
        if (_stateType == null) return false;
        var member = _stateType.GetMembers(BindingFlags.Public | BindingFlags.Instance)
            .FirstOrDefault(m => m.Name.Equals(handName, StringComparison.OrdinalIgnoreCase) ||
                                 m.Name.Equals(handName + "Hand", StringComparison.OrdinalIgnoreCase));
        if (member == null) return false;
        return SetMember(ref state, _stateType, member, hand);
    }

    private static bool SetAny(ref object obj, Type type, string[] names, object value)
    {
        foreach (var name in names)
        {
            var member = type.GetMembers(BindingFlags.Public | BindingFlags.Instance)
                .FirstOrDefault(m => m.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
            if (member != null && SetMember(ref obj, type, member, value)) return true;
        }
        return false;
    }

    private static bool SetMember(ref object obj, Type type, MemberInfo member, object value)
    {
        try
        {
            if (member is FieldInfo f)
            {
                f.SetValue(obj, ConvertValue(value, f.FieldType));
                return true;
            }
            if (member is PropertyInfo p && p.CanWrite)
            {
                p.SetValue(obj, ConvertValue(value, p.PropertyType));
                return true;
            }
        }
        catch { }
        return false;
    }

    private static Type? GetMemberType(MemberInfo member)
        => member is FieldInfo f ? f.FieldType : member is PropertyInfo p ? p.PropertyType : null;

    private static object ConvertValue(object value, Type target)
    {
        var nullable = Nullable.GetUnderlyingType(target);
        target = nullable ?? target;
        if (target.IsEnum) return Enum.ToObject(target, value);
        return Convert.ChangeType(value, target);
    }

    private static float NormalizeStick(short value)
        => value >= 0 ? value / 32767f : value / 32768f;

    private static void InvokeSubmit(object state)
    {
        if (_submitState == null || _vrController == null) return;
        _submitState.Invoke(_vrController, new[] { state });
    }
}
