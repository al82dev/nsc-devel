#pragma once
#ifdef BUILDING_API
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif
namespace Demo {
struct Scalar { API Scalar(int count, double factor) noexcept; API ~Scalar() noexcept; };
struct Wide { API Wide(int a, double b, float c, int d, int e, double f) noexcept; API ~Wide() noexcept; };
struct Narrow { API Narrow(bool a, signed char b, unsigned char c, short d, unsigned short e, unsigned int f, long long g, unsigned long long h) noexcept; API ~Narrow() noexcept; };
struct Pointer { API Pointer(int* value) noexcept; API ~Pointer() noexcept; };
struct ReadPointer { API ReadPointer(const int* value) noexcept; API ~ReadPointer() noexcept; };
struct Source { API static const int* readSource() noexcept; };
struct Choice { API Choice(int* value) noexcept; API Choice(const int* value) noexcept; API ~Choice() noexcept; };
struct Pair { API Pair(int* a, int* b) noexcept; API Pair(const int* a, const int* b) noexcept; API ~Pair() noexcept; };
struct Cross { API Cross(int* a, const int* b) noexcept; API Cross(const int* a, int* b) noexcept; API ~Cross() noexcept; };
API int checkChoice(int expected) noexcept;
API int check() noexcept;
}
