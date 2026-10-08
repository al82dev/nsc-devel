#include <array>
#include <Windows.h>

#define main nscNativeSharp3T10CoroutineLegacyMain
#include "../NativeSharp3T10CoroutineTests.cpp"
#undef main

int nscNativeSharp3T10CoroutineLegacyEntry()
{
    std::array<char, 32768> executable{};
    const DWORD length = GetModuleFileNameA(nullptr, executable.data(),
        static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size())
    {
        return 1;
    }
    std::array<char*, 1> arguments{executable.data()};
    return nscNativeSharp3T10CoroutineLegacyMain(
        static_cast<int>(arguments.size()), arguments.data());
}
