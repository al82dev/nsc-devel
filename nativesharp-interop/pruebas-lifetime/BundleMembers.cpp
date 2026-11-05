extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);

namespace Demo {
struct CountedMember { int value; CountedMember() noexcept(false); ~CountedMember() noexcept(false); };
struct FailingMember { int value; FailingMember() noexcept(false); ~FailingMember() noexcept(false); };
static int stage = 0;
CountedMember::CountedMember() noexcept(false) { if (stage != 0 && stage != 4) ExitProcess(41); ++stage; }
FailingMember::FailingMember() noexcept(false) { if (stage != 1 && stage != 5) ExitProcess(42); ++stage; }
FailingMember::~FailingMember() noexcept(false) { if (stage != 2 && stage != 6) ExitProcess(43); ++stage; }
CountedMember::~CountedMember() noexcept(false) {
    if (stage != 3 && stage != 7) ExitProcess(44);
    if (++stage == 8) ExitProcess(0);
}
}
