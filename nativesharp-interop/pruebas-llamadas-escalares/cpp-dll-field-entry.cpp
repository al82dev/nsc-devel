extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" [[noreturn]] void __nativesharp_trap(int) { ExitProcess(99); __builtin_unreachable(); }
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 28 ? 0 : 1); }
