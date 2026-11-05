extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
namespace tools { int cpp_global = 3; }
extern "C" void mainCRTStartup()
{
    const int result = ns_main();
    ExitProcess(result == 41 && tools::cpp_global == 41 ? 0 : 1);
}
