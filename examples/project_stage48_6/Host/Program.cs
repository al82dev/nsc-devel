using NativeSharp.Stage48_6.Managed;

internal static class Program
{
    private static int Main()
    {
        if (NativeSharpBridge.Add(20, 22) != 42) return 1;
        if (!NativeSharpBridge.EchoBool(true)) return 2;
        if (NativeSharpBridge.EchoByte(42) != 42) return 3;
        if (NativeSharpBridge.EchoUInt(42) != 42) return 4;
        if (NativeSharpBridge.EchoLong(42) != 42) return 5;
        if (NativeSharpBridge.EchoULong(42) != 42) return 6;
        if (Math.Abs(NativeSharpBridge.EchoFloat(4.25f) - 4.25f) > 0.0001f) return 7;
        if (Math.Abs(NativeSharpBridge.EchoDouble(8.5) - 8.5) > 0.0001) return 8;

        Pair pair = new() { Left = 20, Right = 22 };
        pair = NativeSharpBridge.EchoPair(pair);
        if (pair.Left != 20 || pair.Right != 22) return 9;
        if (NativeSharpBridge.EchoMode(HostMode.Second) != HostMode.Second) return 10;
        if (NativeSharpBridge.BufferCount([1, 2, 3, 4]) != 4) return 11;

        IntCallback callback = static value => value * 2;
        if (NativeSharpBridge.Invoke(callback, 21) != 42) return 12;
        NativeSharpBridge.SetCallback(callback);
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
        if (NativeSharpBridge.CallCallback(21) != 42) return 13;

        using Counter counter = new(40);
        counter.Add(2);
        if (counter.Value != 42) return 14;

        Console.WriteLine("Stage48.6 C++/CLI bridge host: PASS");
        return 0;
    }
}
