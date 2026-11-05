#pragma once
namespace Demo {
struct Member { int value; Member() noexcept; ~Member() noexcept; };
struct Outer { Member member; };
int check() noexcept;
}
