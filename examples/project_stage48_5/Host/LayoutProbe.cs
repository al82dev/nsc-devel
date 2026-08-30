using System.Runtime.InteropServices;

namespace NativeSharp.Stage48_5.Host;

[StructLayout(LayoutKind.Sequential)]
internal struct LayoutProbe
{
    public byte Tag;
    public int Value;
}
