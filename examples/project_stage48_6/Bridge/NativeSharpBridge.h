#pragma once

#include "IntCallback.h"
#include "ManagedHostMode.h"
#include "ManagedPair.h"

using namespace System;

namespace NativeSharp::Stage48_6::Managed
{
public ref class NativeSharpBridge abstract sealed
{
public:
    static int Add(int left, int right);
    static bool EchoBool(bool value);
    static Byte EchoByte(Byte value);
    static UInt32 EchoUInt(UInt32 value);
    static Int64 EchoLong(Int64 value);
    static UInt64 EchoULong(UInt64 value);
    static float EchoFloat(float value);
    static double EchoDouble(double value);
    static IntPtr EchoPointer(IntPtr value);
    static Pair EchoPair(Pair value);
    static HostMode EchoMode(HostMode value);
    static int BufferCount(array<int>^ values);
    static int Invoke(IntCallback^ callback, int value);
    static void SetCallback(IntCallback^ callback);
    static int CallCallback(int value);

private:
    static IntCallback^ storedCallback_;
};
}
