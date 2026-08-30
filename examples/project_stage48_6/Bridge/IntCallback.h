#pragma once

using namespace System::Runtime::InteropServices;

namespace NativeSharp::Stage48_6::Managed
{
[UnmanagedFunctionPointer(CallingConvention::Winapi)]
public delegate int IntCallback(int value);
}
