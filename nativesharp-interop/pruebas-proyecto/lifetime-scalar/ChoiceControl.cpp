#include "Scalar.h"
extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned);
extern "C" void mainCRTStartup() {
    int value = 42;
    { Demo::Choice object(&value); }
    if (Demo::checkChoice(1) != 67) ExitProcess(1);
    const int* readonly = Demo::Source::readSource();
    { Demo::Choice object(readonly); }
    ExitProcess(Demo::checkChoice(2) == 67 ? 67 : 2);
}
