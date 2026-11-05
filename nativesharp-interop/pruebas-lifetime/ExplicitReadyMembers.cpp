extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int) noexcept;
namespace Demo {
struct Ready { Ready(int) noexcept; ~Ready() noexcept; };
static int stage = 0;
Ready::Ready(int value) noexcept { stage = value == 1 ? 1 : 99; }
Ready::~Ready() noexcept { ExitProcess(stage == 1 ? 0 : 1); }
}
