using System.Runtime.InteropServices;

namespace NativeSharp.Stage48_5.Host;

internal static unsafe class Program
{
    private static readonly NativeMethods.IntCallback s_persistentCallback = Twice;

    private static int Twice(int value) => value * 2;

    private static int SumBuffer(int* values, int count)
    {
        int sum = 0;
        for (int i = 0; i < count; ++i)
        {
            sum += values[i];
        }
        return sum;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    private static void VerifyManagedLayouts()
    {
        Require(Marshal.SizeOf<Pair>() == 8, "Pair ABI size must be 8 bytes.");
        Require(Marshal.OffsetOf<Pair>(nameof(Pair.Left)).ToInt64() == 0, "Pair.Left ABI offset must be 0.");
        Require(Marshal.OffsetOf<Pair>(nameof(Pair.Right)).ToInt64() == 4, "Pair.Right ABI offset must be 4.");
        Require(Marshal.SizeOf<LayoutProbe>() == 8, "LayoutProbe ABI size must be 8 bytes.");
        Require(Marshal.OffsetOf<LayoutProbe>(nameof(LayoutProbe.Tag)).ToInt64() == 0, "LayoutProbe.Tag offset must be 0.");
        Require(Marshal.OffsetOf<LayoutProbe>(nameof(LayoutProbe.Value)).ToInt64() == 4, "LayoutProbe.Value offset must be 4.");
    }

    private static int Main()
    {
        if (!OperatingSystem.IsWindows() || RuntimeInformation.ProcessArchitecture != Architecture.X64)
        {
            Console.Error.WriteLine("Stage 48.5 host requires Windows x64.");
            return 100;
        }

        VerifyManagedLayouts();

        Require(NativeMethods.Add(20, 22) == 42, "int32 P/Invoke failed.");
        Require(NativeMethods.EchoBool(true), "one-byte bool P/Invoke failed.");
        Require(NativeMethods.EchoByte(0xA5) == 0xA5, "byte P/Invoke failed.");
        Require(NativeMethods.EchoUInt(0xF0000001u) == 0xF0000001u, "uint32 P/Invoke failed.");
        Require(NativeMethods.EchoLong(-0x102030405060708L) == -0x102030405060708L, "int64 P/Invoke failed.");
        Require(NativeMethods.EchoULong(0xF020304050607080UL) == 0xF020304050607080UL, "uint64 P/Invoke failed.");
        Require(Math.Abs(NativeMethods.EchoFloat(12.5f) - 12.5f) < 0.0001f, "float P/Invoke failed.");
        Require(Math.Abs(NativeMethods.EchoDouble(42.125) - 42.125) < 0.0000001, "double P/Invoke failed.");

        int[] values = [1, 2, 3, 4];
        NativeMethods.BufferCallback sum = SumBuffer;
        fixed (int* valuesPointer = values)
        {
            Require(NativeMethods.EchoIntPointer(valuesPointer) == valuesPointer, "typed pointer round-trip failed.");
            Require(NativeMethods.InvokeBuffer(sum, valuesPointer, values.Length) == 10, "pointer+count callback failed.");
        }
        GC.KeepAlive(sum);

        int marker = 7;
        nint markerPointer = (nint)(&marker);
        Require(NativeMethods.EchoPointer(markerPointer) == markerPointer, "opaque void* round-trip failed.");

        Pair pair = NativeMethods.EchoPair(new Pair { Left = 20, Right = 22 });
        Require(pair.Left == 20 && pair.Right == 22, "POD struct P/Invoke failed.");
        Require(NativeMethods.EchoMode(HostMode.Second) == HostMode.Second, "enum P/Invoke failed.");
        LayoutProbe layout = NativeMethods.EchoLayout(new LayoutProbe { Tag = 1, Value = 42 });
        Require(layout.Tag == 1 && layout.Value == 42, "aligned POD layout P/Invoke failed.");

        NativeMethods.IntCallback immediate = Twice;
        Require(NativeMethods.Invoke(immediate, 21) == 42, "immediate delegate callback failed.");
        GC.KeepAlive(immediate);

        NativeMethods.SetCallback(s_persistentCallback);
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
        Require(NativeMethods.CallCallback(21) == 42, "persistent callback lifetime/rooting failed.");
        GC.KeepAlive(s_persistentCallback);

        using CounterHandle counter = CounterHandle.Create(40);
        counter.Add(2);
        Require(counter.Value == 42, "opaque NativeSharp handle ownership failed.");

        Console.WriteLine("Stage 48.5 C# / PInvoke host: PASS");
        return 0;
    }
}
