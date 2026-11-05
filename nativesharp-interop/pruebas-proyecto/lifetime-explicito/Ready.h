#pragma once
namespace Demo {
struct Ready { Ready(int value) noexcept; ~Ready() noexcept; };
int check() noexcept;
}
