extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
namespace A { namespace B { struct C { long long data; int other; }; C instances[2]; C* create() { return instances; } int consume(C* p) { return p == nullptr ? 37 : p == &instances[1] ? 23 : 0; } } }
static_assert(sizeof(A::B::C) == 16 && alignof(A::B::C) == 8, "unexpected C++ record dimensions");
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 8163723 ? 0 : 1); }
