#pragma once

namespace T09ObjectSize {
using Callback = int (*)(int*);
struct Pair { int first; int second; };
int size0(int *p __attribute__((pass_object_size(0))));
int size1(int *p __attribute__((pass_object_size(1))));
int size2(int *p __attribute__((pass_object_size(2))));
int size3(int *p __attribute__((pass_object_size(3))));
int dynamic0(int *p __attribute__((pass_dynamic_object_size(0))));
int dynamic2(int *p __attribute__((pass_dynamic_object_size(2))));
int mixed(int first, int *p __attribute__((pass_object_size(0))), int second,
          int *q __attribute__((pass_object_size(2))), int last);
int *next();
int calls();
}
