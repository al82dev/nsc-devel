namespace Demo { struct __declspec(dllexport) Risky { Risky(int); ~Risky() noexcept(false); }; }
static void* objects[4];
static int failAt;
static int count;
static int bad;
#ifdef CPP_USE_BOUNDARY
extern "C" void cpp_unwind_destructor_boundary(void (*)(void*), void*) noexcept;
using terminate_handler = void (*)();
extern "C" terminate_handler set_terminate(terminate_handler) noexcept;
extern "C" __declspec(dllexport) void local_set_terminate(terminate_handler handler) { set_terminate(handler); }
static void local_throw_again(void*) { throw 43; }
#endif
Demo::Risky::Risky(int value) { objects[value] = this; if (value == failAt) throw 42; }
Demo::Risky::~Risky() noexcept(false) {
    const int completed = failAt > 4 ? 3 : failAt - 1;
    if (count >= completed || this != objects[completed - count]) bad = 1;
    ++count;
    if (failAt == 6 && this == objects[2]) throw 43;
    if (failAt == 7 && this == objects[3]) {
#ifdef CPP_USE_BOUNDARY
        cpp_unwind_destructor_boundary(local_throw_again, this);
#else
        throw 43;
#endif
    }
}
extern "C" __declspec(dllexport) void cpp_local_throw() { if (failAt == 4 || failAt == 7) throw 42; }
extern "C" __declspec(dllexport) void local_set_case(int value) { failAt = value; count = 0; bad = 0; }
extern "C" __declspec(dllexport) int local_result() {
    const int completed = failAt > 4 ? 3 : failAt - 1;
    return !bad && count == completed;
}
