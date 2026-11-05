extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
namespace A { namespace B {
struct C { long long data; int other; };
C instance;
C* create() { return &instance; }
int consume(C* p) { return p == &instance ? 19 : 0; }
} }
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 19 ? 0 : 1); }
