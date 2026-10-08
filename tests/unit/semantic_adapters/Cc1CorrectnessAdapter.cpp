#define main nscCc1CorrectnessLegacyMain
#include "../Cc1CorrectnessTests.cpp"
#undef main

int nscCc1CorrectnessLegacyEntry(int argumentCount, char** arguments)
{
    // The historical executable had fresh globals for every CTest process.
    // Recreate that boundary before each selector inside the consolidated host.
    gSelectedFixture.clear();
    gRestMode = false;
    gSelectedFixtureMatched = false;
    return nscCc1CorrectnessLegacyMain(argumentCount, arguments);
}
