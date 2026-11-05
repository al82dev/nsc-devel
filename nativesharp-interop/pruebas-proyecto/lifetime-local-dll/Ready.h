#pragma once
#ifdef BUILDING_API
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif
namespace Demo {
struct Ready { API Ready(int value) noexcept; API ~Ready() noexcept; };
API int check() noexcept;
}
