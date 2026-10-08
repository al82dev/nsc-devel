#include <array>
#include <Windows.h>

#define wmain nscStage46NativeLibrariesLegacyMain
#include "../Stage46NativeLibrariesTests.cpp"
#undef wmain

int nscStage46NativeLibrariesLegacyEntry()
{
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
        static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size())
    {
        return 1;
    }
    std::array<wchar_t*, 1> arguments{executable.data()};
    return nscStage46NativeLibrariesLegacyMain(
        static_cast<int>(arguments.size()), arguments.data());
}
