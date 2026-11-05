#include "cpp-exception-type.h"
static int finally_count = 0;
#ifndef CPP_THROW_DLL
extern "C" void cpp_cross_throw() { throw 42; }
#endif
extern "C" void cpp_cross_record() { ++finally_count; }
extern "C" int ns_main() __asm__("__nsc_function_CppProject$3A$3A__Main");
#ifdef CPP_EXCEPTION_NONTRIVIAL
extern "C" __declspec(dllimport) int cpp_exception_destructs();
#endif
int main() {
#ifdef CPP_EXCEPTION_NONTRIVIAL
    int seen = 0;
    try { ns_main(); }
    catch (const InteropFailure& value) { seen = value.code == 42 && finally_count == 1; }
    return seen && cpp_exception_destructs() == 1 ? 0 : 2;
#else
    try { ns_main(); }
#ifdef CPP_EXCEPTION_OBJECT
    catch (const InteropFailure& value) { return value.code == 42 && finally_count == 1 ? 0 : 2; }
#else
    catch (int value) { return value == 42 && finally_count == 1 ? 0 : 2; }
#endif
    return 3;
#endif
}
