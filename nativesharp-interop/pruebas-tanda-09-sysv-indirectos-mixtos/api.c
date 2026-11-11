#include "api.h"
__declspec(selectany) int _fltused=0;
T09SysvIndirectMix __attribute__((sysv_abi)) sysv_indirect_mix(float factor, T09SysvIndirectMix first, int n, T09SysvIndirectMix second) {
    first.x += second.x + n; second.y += 3; first.y=second.y; if (factor != 1.5f) first.x=0; return first;
}
