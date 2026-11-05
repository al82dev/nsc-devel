using terminate_handler = void (*)();
extern "C" terminate_handler set_terminate(terminate_handler) noexcept;
extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
static void terminate_marker() { ExitProcess(77); }
#ifdef CPP_SET_DLL_TERMINATE
extern "C" __declspec(dllimport) void local_set_terminate(terminate_handler);
#endif
#ifdef CPP_CONTROL
struct Risky { ~Risky() noexcept(false) { throw 43; } };
static void invoke_ns() { Risky value; throw 42; }
#elif defined(CPP_DLL_CONTROL)
namespace Demo { struct __declspec(dllimport) Risky { Risky(int); ~Risky() noexcept(false); }; }
extern "C" __declspec(dllimport) void cpp_local_throw();
extern "C" __declspec(dllimport) void local_set_case(int);
static void invoke_ns() { Demo::Risky a(1), b(2), c(3); cpp_local_throw(); }
#else
extern "C" int invoke_ns() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" __declspec(dllimport) void local_set_case(int);
#endif
int main() {
    set_terminate(terminate_marker);
#ifdef CPP_SET_DLL_TERMINATE
    local_set_terminate(terminate_marker);
#endif
#ifndef CPP_CONTROL
    local_set_case(7);
#endif
    try { invoke_ns(); } catch (int value) { ExitProcess(value == 42 ? 42 : value == 43 ? 43 : 4); }
    catch (...) { ExitProcess(2); }
    ExitProcess(3);
}
