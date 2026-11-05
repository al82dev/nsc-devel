#pragma once
#ifdef BUILDING_API
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif
namespace Demo {
struct Default { API Default() noexcept; API ~Default() noexcept; };
API int check() noexcept;
}
