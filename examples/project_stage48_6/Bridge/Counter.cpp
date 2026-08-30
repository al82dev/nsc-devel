#include "Counter.h"
#include "Stage48_6HostApi.h"

using namespace System;

namespace NativeSharp::Stage48_6::Managed
{
Counter::Counter(int value)
    : handle_(NS486_Counter_Create(value))
{
    if (handle_ == IntPtr::Zero)
    {
        throw gcnew InvalidOperationException("NativeSharp counter allocation failed.");
    }
}

Counter::~Counter()
{
    Release();
    GC::SuppressFinalize(this);
}

Counter::!Counter()
{
    Release();
}

int Counter::Value::get()
{
    if (handle_ == IntPtr::Zero) throw gcnew ObjectDisposedException("Counter");
    return NS486_Counter_Get(static_cast<NS_Counter*>(handle_.ToPointer()));
}

void Counter::Add(int delta)
{
    if (handle_ == IntPtr::Zero) throw gcnew ObjectDisposedException("Counter");
    NS486_Counter_Add(static_cast<NS_Counter*>(handle_.ToPointer()), delta);
}

void Counter::Release()
{
    if (handle_ == IntPtr::Zero) return;
    NS486_Counter_Destroy(static_cast<NS_Counter*>(handle_.ToPointer()));
    handle_ = IntPtr::Zero;
}
}
