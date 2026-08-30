using System.Runtime.InteropServices;

namespace NativeSharp.Stage48_5.Host;

internal sealed class CounterHandle : SafeHandle
{
    private CounterHandle()
        : base(IntPtr.Zero, true)
    {
    }

    internal static CounterHandle Create(int value)
    {
        nint raw = NativeMethods.CounterCreate(value);
        if (raw == IntPtr.Zero)
        {
            throw new InvalidOperationException("NativeSharp counter allocation returned null.");
        }

        CounterHandle result = new();
        result.SetHandle(raw);
        return result;
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Add(int delta)
    {
        if (IsClosed) throw new ObjectDisposedException(nameof(CounterHandle));
        NativeMethods.CounterAdd(handle, delta);
    }

    internal int Value
    {
        get
        {
            if (IsClosed) throw new ObjectDisposedException(nameof(CounterHandle));
            return NativeMethods.CounterGet(handle);
        }
    }

    protected override bool ReleaseHandle()
    {
        NativeMethods.CounterDestroy(handle);
        return true;
    }
}
