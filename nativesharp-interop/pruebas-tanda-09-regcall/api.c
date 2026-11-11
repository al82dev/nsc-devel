#include "api.h"
int __attribute__((regcall)) t09_regcall(int a, int b, int c) {
    return a * 100 + b * 10 + c;
}
