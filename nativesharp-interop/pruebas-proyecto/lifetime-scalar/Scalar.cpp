#include "Scalar.h"
namespace Demo {
static int stage = 0;
Scalar::Scalar(int count, double factor) noexcept { stage = count == 42 && factor == 2.5 ? 1 : 99; }
Scalar::~Scalar() noexcept { stage = stage == 1 ? 2 : 99; }
Wide::Wide(int a, double b, float c, int d, int e, double f) noexcept {
    stage = a == 42 && b == 2.5 && c == 3.5f && d == 7 && e == 8 && f == 4.5 ? 1 : 99;
}
Wide::~Wide() noexcept { stage = stage == 1 ? 2 : 99; }
Narrow::Narrow(bool a, signed char b, unsigned char c, short d, unsigned short e, unsigned int f, long long g, unsigned long long h) noexcept {
    stage = a && b == -100 && c == 240 && d == -30000 && e == 60000 &&
        f == 4000000000U && g == -1234567890123LL && h == 123456789012345ULL ? 1 : 99;
}
Narrow::~Narrow() noexcept { stage = stage == 1 ? 2 : 99; }
Pointer::Pointer(int* value) noexcept { *value = 42; stage = 1; }
Pointer::~Pointer() noexcept { stage = stage == 1 ? 2 : 99; }
ReadPointer::ReadPointer(const int* value) noexcept { stage = *value == 42 ? 1 : 99; }
ReadPointer::~ReadPointer() noexcept { stage = stage == 1 ? 2 : 99; }
const int* Source::readSource() noexcept { static const int value = 42; return &value; }
static int choice = 0;
Choice::Choice(int* value) noexcept { choice = *value == 42 ? 1 : 0; }
Choice::Choice(const int* value) noexcept { choice = *value == 42 ? 2 : 0; }
Choice::~Choice() noexcept { stage = 2; }
Pair::Pair(int* a, int* b) noexcept { choice = *a == 42 && *b == 42 ? 1 : 0; }
Pair::Pair(const int* a, const int* b) noexcept { choice = *a == 42 && *b == 42 ? 2 : 0; }
Pair::~Pair() noexcept { stage = 2; }
Cross::Cross(int* a, const int* b) noexcept { choice = *a == 42 && *b == 42 ? 3 : 0; }
Cross::Cross(const int* a, int* b) noexcept { choice = *a == 42 && *b == 42 ? 4 : 0; }
Cross::~Cross() noexcept { stage = 2; }
int checkChoice(int expected) noexcept { return choice == expected && stage == 2 ? 67 : 1; }
int check() noexcept { return stage == 2 ? 67 : 1; }
}
