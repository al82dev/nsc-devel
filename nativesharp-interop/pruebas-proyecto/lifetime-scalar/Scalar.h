#pragma once
namespace Demo {
struct Scalar { Scalar(int count, double factor) noexcept; ~Scalar() noexcept; };
struct Wide { Wide(int a, double b, float c, int d, int e, double f) noexcept; ~Wide() noexcept; };
struct Narrow { Narrow(bool a, signed char b, unsigned char c, short d, unsigned short e, unsigned int f, long long g, unsigned long long h) noexcept; ~Narrow() noexcept; };
struct Pointer { Pointer(int* value) noexcept; ~Pointer() noexcept; };
struct ReadPointer { ReadPointer(const int* value) noexcept; ~ReadPointer() noexcept; };
struct Source { static const int* readSource() noexcept; };
struct Choice { Choice(int* value) noexcept; Choice(const int* value) noexcept; ~Choice() noexcept; };
struct Pair { Pair(int* a, int* b) noexcept; Pair(const int* a, const int* b) noexcept; ~Pair() noexcept; };
struct Cross { Cross(int* a, const int* b) noexcept; Cross(const int* a, int* b) noexcept; ~Cross() noexcept; };
int checkChoice(int expected) noexcept;
int check() noexcept;
}
