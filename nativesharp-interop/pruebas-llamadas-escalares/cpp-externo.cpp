extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
namespace tools { int blend(int value) { return value * 7; } }
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 7 ? 0 : 1); }
