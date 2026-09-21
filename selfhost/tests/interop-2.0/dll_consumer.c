#include <stdint.h>

typedef struct Pair { int32_t Left; int32_t Right; } Pair;
__declspec(dllimport) int32_t ns_pair_sum(Pair value);
__declspec(dllimport) Pair ns_make_pair(int32_t left, int32_t right);
__declspec(dllimport) __declspec(noreturn) void __stdcall ExitProcess(unsigned int code);

void mainCRTStartup(void)
{
    Pair value = ns_make_pair(19, 23);
    ExitProcess(ns_pair_sum(value) == 42 ? 0u : 9u);
}
