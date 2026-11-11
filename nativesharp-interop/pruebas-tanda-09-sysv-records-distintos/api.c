#include "api.h"
T09Small __attribute__((sysv_abi)) sysv_hetero(T09Small a, T09Wide b) {
    a.x += 10 + b.x - 1; a.y=b.y; a.w=b.d; return a;
}
