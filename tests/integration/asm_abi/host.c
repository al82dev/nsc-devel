#include "AssemblyAbi.h"

#include <stdint.h>

static uintptr_t call_with_rbx_guard(int32_t value, int32_t* result)
{
    const uintptr_t sentinel = UINT64_C(0x13579BDF2468ACE0);
    uintptr_t observed;
    __asm__ volatile("movq %0, %%rbx" : : "r"(sentinel) : "rbx", "memory");
    *result = NSAsmUseNonvolatile(value);
    __asm__ volatile("movq %%rbx, %0" : "=r"(observed) : : "memory");
    return observed;
}

int main(void)
{
    int32_t memory = 40;
    int32_t nonvolatile_result = 0;
    const uintptr_t observed = call_with_rbx_guard(42, &nonvolatile_result);
    if (NSAsmAdd(20, 22) != 42) return 1;
    if (NSAsmAddMemory(&memory, 2) != 42 || memory != 42) return 2;
    if (nonvolatile_result != 42 || observed != UINT64_C(0x13579BDF2468ACE0)) return 3;
    return 42;
}
