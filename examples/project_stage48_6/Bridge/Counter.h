#pragma once

using namespace System;

namespace NativeSharp::Stage48_6::Managed
{
public ref class Counter sealed
{
public:
    explicit Counter(int value);
    ~Counter();
    !Counter();

    property int Value
    {
        int get();
    }

    void Add(int delta);

private:
    void Release();
    IntPtr handle_;
};
}
