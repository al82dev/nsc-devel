#include <algorithm>
#include <array>
#include <iterator>

#define main nscLlvmOptimizationLegacyMain
#include "../LlvmOptimizationTests.cpp"
#undef main

int nscLlvmOptimizationLegacyEntry()
{
    std::array<char, 32> executable{};
    constexpr char name[] = "nsc_unit_tests.exe";
    std::copy(std::begin(name), std::end(name), executable.begin());
    std::array<char*, 1> arguments{executable.data()};
    return nscLlvmOptimizationLegacyMain(static_cast<int>(arguments.size()), arguments.data());
}

int nscLlvmOptimizationLegacyChildEntry(int argumentCount, char** arguments)
{
    return nscLlvmOptimizationLegacyMain(argumentCount, arguments);
}
