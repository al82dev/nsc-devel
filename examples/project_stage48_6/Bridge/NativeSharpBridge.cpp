#include "NativeSharpBridge.h"
#include "Stage48_6HostApi.h"

#include <vcclr.h>

using namespace System;
using namespace System::Runtime::InteropServices;

namespace NativeSharp::Stage48_6::Managed
{
IntCallback^ NativeSharpBridge::storedCallback_ = nullptr;

int NativeSharpBridge::Add(int left, int right)
{
    return NS486_Add(left, right);
}

bool NativeSharpBridge::EchoBool(bool value)
{
    return NS486_EchoBool(value ? 1u : 0u) != 0;
}

Byte NativeSharpBridge::EchoByte(Byte value)
{
    return NS486_EchoByte(value);
}

UInt32 NativeSharpBridge::EchoUInt(UInt32 value)
{
    return NS486_EchoUInt(value);
}

Int64 NativeSharpBridge::EchoLong(Int64 value)
{
    return NS486_EchoLong(value);
}

UInt64 NativeSharpBridge::EchoULong(UInt64 value)
{
    return NS486_EchoULong(value);
}

float NativeSharpBridge::EchoFloat(float value)
{
    return NS486_EchoFloat(value);
}

double NativeSharpBridge::EchoDouble(double value)
{
    return NS486_EchoDouble(value);
}

IntPtr NativeSharpBridge::EchoPointer(IntPtr value)
{
    return IntPtr(NS486_EchoPointer(value.ToPointer()));
}

Pair NativeSharpBridge::EchoPair(Pair value)
{
    NS_Pair nativeValue{};
    nativeValue.Left = value.Left;
    nativeValue.Right = value.Right;
    nativeValue = NS486_EchoPair(nativeValue);

    Pair result{};
    result.Left = nativeValue.Left;
    result.Right = nativeValue.Right;
    return result;
}

HostMode NativeSharpBridge::EchoMode(HostMode value)
{
    return static_cast<HostMode>(NS486_EchoMode(static_cast<NS_HostMode>(value)));
}

int NativeSharpBridge::BufferCount(array<int>^ values)
{
    if (values == nullptr) throw gcnew ArgumentNullException("values");
    if (values->Length == 0) return 0;
    pin_ptr<int> pinned = &values[0];
    return NS486_BufferCount(pinned, values->Length);
}

int NativeSharpBridge::Invoke(IntCallback^ callback, int value)
{
    if (callback == nullptr) throw gcnew ArgumentNullException("callback");
    IntPtr function = Marshal::GetFunctionPointerForDelegate(callback);
    auto nativeCallback = reinterpret_cast<NS_IntCallback>(function.ToPointer());
    const int result = NS486_Invoke(nativeCallback, value);
    GC::KeepAlive(callback);
    return result;
}

void NativeSharpBridge::SetCallback(IntCallback^ callback)
{
    if (callback == nullptr) throw gcnew ArgumentNullException("callback");
    storedCallback_ = callback;
    IntPtr function = Marshal::GetFunctionPointerForDelegate(callback);
    NS486_SetCallback(reinterpret_cast<NS_IntCallback>(function.ToPointer()));
    GC::KeepAlive(callback);
}

int NativeSharpBridge::CallCallback(int value)
{
    if (storedCallback_ == nullptr) throw gcnew InvalidOperationException("No callback is registered.");
    const int result = NS486_CallCallback(value);
    GC::KeepAlive(storedCallback_);
    return result;
}
}
