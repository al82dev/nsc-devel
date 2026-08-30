using System.Runtime.InteropServices;

namespace NativeSharp.Stage48_5.Host;

internal static unsafe class NativeMethods
{
    internal const string LibraryName = "Stage48_5HostApi.dll";

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    internal delegate int IntCallback(int value);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    internal unsafe delegate int BufferCallback(int* values, int count);

    [DllImport(LibraryName, EntryPoint = "NS485_Add", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern int Add(int left, int right);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoBool", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool EchoBool([MarshalAs(UnmanagedType.I1)] bool value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoByte", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern byte EchoByte(byte value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoUInt", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern uint EchoUInt(uint value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoLong", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern long EchoLong(long value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoULong", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern ulong EchoULong(ulong value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoFloat", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern float EchoFloat(float value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoDouble", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern double EchoDouble(double value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoIntPointer", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern int* EchoIntPointer(int* values);

    [DllImport(LibraryName, EntryPoint = "NS485_InvokeBuffer", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern int InvokeBuffer(BufferCallback callback, int* values, int count);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoPointer", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern nint EchoPointer(nint value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoPair", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern Pair EchoPair(Pair value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoMode", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern HostMode EchoMode(HostMode value);

    [DllImport(LibraryName, EntryPoint = "NS485_EchoLayout", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern LayoutProbe EchoLayout(LayoutProbe value);

    [DllImport(LibraryName, EntryPoint = "NS485_Invoke", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern int Invoke(IntCallback callback, int value);

    [DllImport(LibraryName, EntryPoint = "NS485_SetCallback", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern void SetCallback(IntCallback callback);

    [DllImport(LibraryName, EntryPoint = "NS485_CallCallback", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern int CallCallback(int value);

    [DllImport(LibraryName, EntryPoint = "NS485_Counter_Create", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern nint CounterCreate(int value);

    [DllImport(LibraryName, EntryPoint = "NS485_Counter_Get", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern int CounterGet(nint counter);

    [DllImport(LibraryName, EntryPoint = "NS485_Counter_Add", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern void CounterAdd(nint counter, int delta);

    [DllImport(LibraryName, EntryPoint = "NS485_Counter_Destroy", ExactSpelling = true, CallingConvention = CallingConvention.Winapi)]
    internal static extern void CounterDestroy(nint counter);
}
