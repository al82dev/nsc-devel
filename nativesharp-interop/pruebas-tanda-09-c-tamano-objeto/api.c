#include "api.h"
#define DEFINE_SIZE(n) int t09_s##n(int *const p T09_SIZE(n)) { return (int)__builtin_object_size(p, n); }
#define DEFINE_DYNAMIC(n) int t09_d##n(int *const p T09_DYNAMIC(n)) { return (int)__builtin_dynamic_object_size(p, n); }
DEFINE_SIZE(0)
DEFINE_SIZE(1)
DEFINE_SIZE(2)
DEFINE_SIZE(3)
DEFINE_DYNAMIC(0)
DEFINE_DYNAMIC(1)
DEFINE_DYNAMIC(2)
DEFINE_DYNAMIC(3)
int t09_narrow(int *const p T09_SIZE(0), signed char a, unsigned short b, _Bool c) {
    return (int)__builtin_object_size(p, 0) + a + b + c;
}
