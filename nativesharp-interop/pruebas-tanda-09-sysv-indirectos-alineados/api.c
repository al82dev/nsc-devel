#include "api.h"
__declspec(selectany) int _fltused=0;
T09SysvAlignedMix __attribute__((sysv_abi)) sysv_aligned_mix(float factor, T09SysvAlignedMix first, int n, T09SysvAlignedMix second) {
    first.x += second.x + n; second.y += 3; first.y=second.y; if (factor != 1.5f) first.x=0; return first;
}
