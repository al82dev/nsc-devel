#pragma once

#if defined(T09_EXPORT)
#define T09_STORAGE __declspec(dllexport)
#elif defined(T09_IMPORT)
#define T09_STORAGE __declspec(dllimport)
#else
#define T09_STORAGE
#endif

namespace T09ObjectSize {
using Callback = int (*)(int*);
struct Pair { int first; int second; };
struct ArrayPair { int values[2]; int tail[3]; };
T09_STORAGE int size0(int *p __attribute__((pass_object_size(0))));
T09_STORAGE int size1(int *p __attribute__((pass_object_size(1))));
T09_STORAGE int size2(int *p __attribute__((pass_object_size(2))));
T09_STORAGE int size3(int *p __attribute__((pass_object_size(3))));
T09_STORAGE int dynamic0(int *p __attribute__((pass_dynamic_object_size(0))));
T09_STORAGE int dynamic1(int *p __attribute__((pass_dynamic_object_size(1))));
T09_STORAGE int dynamic2(int *p __attribute__((pass_dynamic_object_size(2))));
T09_STORAGE int dynamic3(int *p __attribute__((pass_dynamic_object_size(3))));
T09_STORAGE int mixed(int first, int *p __attribute__((pass_object_size(0))), int second,
          int *q __attribute__((pass_object_size(2))), int last);
T09_STORAGE int narrow(int *p __attribute__((pass_object_size(0))), signed char a, unsigned short b, bool c);
T09_STORAGE int narrow2(int *p __attribute__((pass_object_size(0))), int *q __attribute__((pass_object_size(2))),
                        signed char a, unsigned short b, bool c);
T09_STORAGE int *next();
T09_STORAGE int calls();
T09_STORAGE int index();
}
