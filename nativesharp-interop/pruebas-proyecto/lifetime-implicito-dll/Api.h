#pragma once
#ifdef BUILDING_API
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif
namespace Demo {
struct Member { int value; API Member() noexcept; API ~Member() noexcept; };
struct Outer { Member member; };
API int check() noexcept;
}
