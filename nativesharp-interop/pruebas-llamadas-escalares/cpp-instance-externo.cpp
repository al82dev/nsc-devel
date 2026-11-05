extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" int ns_main() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" { int _fltused = 0; }
extern "C" void __nativesharp_trap(int) { ExitProcess(2); }
namespace Demo {
class Widget {
public:
    int padding;
    int value;
    const int immutable;
    volatile int changing;
    static int shared;
    static const int limit;
    static volatile int io;
    static thread_local int tls;
    int ping(int) const noexcept;
    int mutate(int) noexcept;
    double factor(double) const noexcept;
    int mixed(int, double, int, double) const noexcept;
    int skip(int) volatile;
    int merge(int);
    static int merge(double);
    static int scale(int) noexcept;
    static int scale(double) noexcept;
};
Widget instance{42, 0, 33, 0};
int Widget::shared = 5;
const int Widget::limit = 17;
volatile int Widget::io = 1;
Widget* create() { return &instance; }
struct Thing { int marker; int compute(int) const noexcept; };
Thing thing{23};
Thing* createThing() { return &thing; }
int Thing::compute(int argument) const noexcept { return this == &thing && marker == 23 && argument == 8 ? 19 : 0; }
int Widget::mutate(int argument) noexcept { value = argument; return this == &instance ? 11 : 0; }
int Widget::ping(int argument) const noexcept { return this == &instance && argument == 5 && padding == 42 && value == 19 && immutable == 33 && changing == 22 && shared == 23 && io == 4 ? 31 : 0; }
double Widget::factor(double argument) const noexcept { return this == &instance && argument == 1.5 ? 13.0 : 0.0; }
int Widget::mixed(int a, double b, int c, double d) const noexcept {
    return this == &instance && a == 3 && b == 2.5 && c == 9 && d == 4.5 ? 17 : 0;
}
int Widget::scale(int value) noexcept { return value == 3 ? 12 : 0; }
int Widget::scale(double value) noexcept { return value == 2.5 ? 13 : 0; }
}
extern "C" void mainCRTStartup() { ExitProcess(ns_main() == 234 ? 0 : 1); }
