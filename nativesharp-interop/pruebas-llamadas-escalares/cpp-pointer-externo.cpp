extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
namespace tools { int* pointer_identity(int* value) { return value; } }
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 41 ? 0 : 1); }
