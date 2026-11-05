extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" { int _fltused = 0; }
extern "C" int bridge_probe(unsigned long long count, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, count);
    int integer = __builtin_va_arg(args, int);
    double floating = __builtin_va_arg(args, double);
    __builtin_va_end(args);
    return count == 2 && integer == 3 && floating == 1.5 ? 42 : 0;
}
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 42 ? 0 : 1); }
