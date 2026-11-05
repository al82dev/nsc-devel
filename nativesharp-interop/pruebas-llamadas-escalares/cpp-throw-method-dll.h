namespace Demo {
struct Thrower {
    __declspec(dllimport) int raise(int value);
    __declspec(dllimport) static int raiseStatic(int value);
};
__declspec(dllimport) Thrower* make() noexcept;
}
extern "C" void cpp_cross_record();
