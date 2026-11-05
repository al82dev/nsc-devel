extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" int ptr_read(int* value) { return *value + 1; }
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 42 ? 0 : 1); }
