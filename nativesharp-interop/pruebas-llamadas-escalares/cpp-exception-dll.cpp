#include "cpp-exception-type.h"
#ifdef CPP_EXCEPTION_NONTRIVIAL
static int exception_destructs = 0;
InteropFailure::~InteropFailure() { ++exception_destructs; }
extern "C" __declspec(dllexport) int cpp_exception_destructs() { return exception_destructs; }
#endif
extern "C" __declspec(dllexport) void cpp_cross_throw() {
#ifdef CPP_EXCEPTION_OBJECT
    throw InteropFailure{42};
#else
    throw 42;
#endif
}
