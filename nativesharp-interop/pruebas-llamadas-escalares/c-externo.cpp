extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" int c_add(int a, int b) { return 100 * a + b; }
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 102 ? 0 : 1); }
