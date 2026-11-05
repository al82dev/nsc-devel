#pragma once
namespace Demo {
struct Ready { Ready(int value) noexcept; ~Ready() noexcept; };
struct Risk { Risk(int value); ~Risk() noexcept(false); };
int check() noexcept;
}
