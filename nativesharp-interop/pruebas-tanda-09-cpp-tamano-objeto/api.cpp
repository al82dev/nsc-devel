#include "api.h"

int T09ObjectSize::size0(int *const p __attribute__((pass_object_size(0)))) {
    return static_cast<int>(__builtin_object_size(p, 0));
}
int T09ObjectSize::size1(int *const p __attribute__((pass_object_size(1)))) {
    return static_cast<int>(__builtin_object_size(p, 1));
}
int T09ObjectSize::size2(int *const p __attribute__((pass_object_size(2)))) {
    return static_cast<int>(__builtin_object_size(p, 2));
}
int T09ObjectSize::size3(int *const p __attribute__((pass_object_size(3)))) {
    return static_cast<int>(__builtin_object_size(p, 3));
}
int T09ObjectSize::dynamic0(int *const p __attribute__((pass_dynamic_object_size(0)))) {
    return static_cast<int>(__builtin_dynamic_object_size(p, 0));
}
int T09ObjectSize::dynamic1(int *const p __attribute__((pass_dynamic_object_size(1)))) {
    return static_cast<int>(__builtin_dynamic_object_size(p, 1));
}
int T09ObjectSize::dynamic2(int *const p __attribute__((pass_dynamic_object_size(2)))) {
    return static_cast<int>(__builtin_dynamic_object_size(p, 2));
}
int T09ObjectSize::dynamic3(int *const p __attribute__((pass_dynamic_object_size(3)))) {
    return static_cast<int>(__builtin_dynamic_object_size(p, 3));
}
int T09ObjectSize::mixed(int first, int *const p __attribute__((pass_object_size(0))), int second,
                         int *const q __attribute__((pass_object_size(2))), int last) {
    return first + second + last + static_cast<int>(__builtin_object_size(p, 0)) +
           static_cast<int>(__builtin_object_size(q, 2));
}
int T09ObjectSize::narrow(int *const p __attribute__((pass_object_size(0))), signed char a, unsigned short b, bool c) {
    return static_cast<int>(__builtin_object_size(p, 0)) + a + b + c;
}
int T09ObjectSize::narrow2(int *const p __attribute__((pass_object_size(0))), int *const q __attribute__((pass_object_size(2))),
                           signed char a, unsigned short b, bool c) {
    return static_cast<int>(__builtin_object_size(p, 0) + __builtin_object_size(q, 2)) + a + b + c;
}
namespace { int stored; int callCount; }
int *T09ObjectSize::next() { ++callCount; return &stored; }
int T09ObjectSize::calls() { return callCount; }
int T09ObjectSize::index() { return 1; }
