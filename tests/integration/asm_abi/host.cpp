#include "AssemblyAbi.h"

#include <cstdint>

static std::uintptr_t call_with_rbx_guard(std::int32_t value, std::int32_t* result)
{
    constexpr std::uintptr_t sentinel = UINT64_C(0x02468ACE13579BDF);
    std::uintptr_t observed;
    __asm__ volatile("movq %0, %%rbx" : : "r"(sentinel) : "rbx", "memory");
    *result = NSAsmUseNonvolatile(value);
    __asm__ volatile("movq %%rbx, %0" : "=r"(observed) : : "memory");
    return observed;
}

int main()
{
    std::int32_t memory = 19;
    std::int32_t nonvolatile_result = 0;
    const std::uintptr_t observed = call_with_rbx_guard(42, &nonvolatile_result);
    if (NSAsmAdd(21, 21) != 42) return 1;
    if (NSAsmAddMemory(&memory, 23) != 42 || memory != 42) return 2;
    if (nonvolatile_result != 42 || observed != UINT64_C(0x02468ACE13579BDF)) return 3;
    return 42;
}
