namespace Demo {
#ifdef METHOD_DLL_BUILD
#define METHOD_API __declspec(dllexport)
#else
#define METHOD_API
#endif
struct Thrower {
    METHOD_API int raise(int value);
    METHOD_API static int raiseStatic(int value);
};
METHOD_API Thrower* make() noexcept;
#undef METHOD_API
}
extern "C" void cpp_cross_record();
