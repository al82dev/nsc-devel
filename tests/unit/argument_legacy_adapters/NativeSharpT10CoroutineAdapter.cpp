#include <array>
#include <Windows.h>

#define main nscNativeSharpT10CoroutineLegacyMain
#include "../NativeSharpT10CoroutineTests.cpp"
#undef main

int nscNativeSharpT10CoroutineLegacyEntry()
{
    std::array<char, 32768> executable{};
    const DWORD length = GetModuleFileNameA(nullptr, executable.data(),
        static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size())
    {
        return 1;
    }
    std::array<char*, 1> arguments{executable.data()};
    return nscNativeSharpT10CoroutineLegacyMain(
        static_cast<int>(arguments.size()), arguments.data());
}
