#include <array>
#include <Windows.h>

#define main nscNativeSharpE046SmartPointerLegacyMain
#include "../NativeSharpE046SmartPointerTests.cpp"
#undef main

int nscNativeSharpE046SmartPointerLegacyEntry()
{
    std::array<char, 32768> executable{};
    const DWORD length = GetModuleFileNameA(nullptr, executable.data(),
        static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size())
    {
        return 1;
    }
    std::array<char*, 1> arguments{executable.data()};
    return nscNativeSharpE046SmartPointerLegacyMain(
        static_cast<int>(arguments.size()), arguments.data());
}
