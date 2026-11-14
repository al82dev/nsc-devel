// Controlled OS and allocation doubles for the real generated argument conversion.
extern "C" __declspec(dllimport) void* GetCurrentProcess();
extern "C" __declspec(dllimport) int TerminateProcess(void*, unsigned);

static unsigned long long block[16];
static unsigned long long strings[4][4];
static void* argvItems[4] = {strings[0], strings[1], strings[2], strings[3]};
static int allocCalls, fillCalls, mainCalls, blockFrees, localFrees, heapCalls;
static int conversionIndex, badFree;
static unsigned freedMask;

extern "C" int __nativesharp_eh_init() { return 1; }
extern "C" int __nativesharp_panic_eh_filter(void*) { return 0; }
extern "C" void __chkstk() { }
extern "C" void* __nativesharp_alloc(unsigned long long)
{ ++allocCalls; return CASE == 1 ? nullptr : block; }
extern "C" void* GetCommandLineW() { return argvItems; }
extern "C" void* CommandLineToArgvW(void*, int* count)
{
    ++fillCalls;
    if (CASE == 0 || (CASE == 15 && fillCalls == 2)) return nullptr;
    *count = 4;
    return argvItems;
}
extern "C" void* GetProcessHeap() { return block; }
extern "C" void* HeapAlloc(void*, unsigned, unsigned long long)
{
    int index = heapCalls++;
    if (CASE >= 2 && CASE <= 5 && index == CASE - 2) return nullptr;
    return strings[index];
}
static void recordFree(void* value)
{
    for (unsigned i = 0; i < 4; ++i)
        if (value == strings[i])
        {
            unsigned bit = 1U << i;
            if ((freedMask & bit) != 0) ++badFree;
            freedMask |= bit;
            return;
        }
    ++badFree;
}
extern "C" int HeapFree(void*, unsigned, void* value) { recordFree(value); return 1; }
extern "C" void* LocalFree(void* value)
{ if (value != argvItems) ++badFree; ++localFrees; return nullptr; }
extern "C" int WideCharToMultiByte(unsigned, unsigned, void*, int, char* output, int, void*, void*)
{
    if (output == nullptr)
    {
        if (CASE >= 11 && CASE <= 14 && conversionIndex == CASE - 11) return 0;
        if (CASE == 16 && conversionIndex == 1) return 1;
        return 2;
    }
    int index = conversionIndex++;
    if (CASE >= 7 && CASE <= 10 && index == CASE - 7) return 0;
    if (CASE == 16 && index == 1) { output[0] = 0; return 1; }
    output[0] = 'x'; output[1] = 0;
    return 2;
}
extern "C" void __nativesharp_free(void* value)
{ if (value == block) ++blockFrees; else recordFree(value); }
extern "C" int test_main(void* argumentArray)
{
    ++mainCalls;
    auto slots = static_cast<unsigned long long*>(argumentArray);
    if (CASE == 16 && (slots[1] != reinterpret_cast<unsigned long long>(strings[1]) || strings[1][0] != 0))
        return 99;
    return slots[0] == 3 ? 77 : 99;
}
extern "C" __declspec(noreturn) void ExitProcess(unsigned code)
{
    int completed = CASE >= 2 && CASE <= 5 ? CASE - 2 :
        (CASE >= 7 && CASE <= 10 ? CASE - 7 :
         (CASE >= 11 && CASE <= 14 ? CASE - 11 : ((CASE == 6 || CASE == 16) ? 4 : 0)));
    int allocated = completed + ((CASE >= 7 && CASE <= 10) ? 1 : 0);
    unsigned expectedMask = (1U << allocated) - 1U;
    bool ok = badFree == 0 && freedMask == expectedMask &&
        allocCalls == (CASE == 0 ? 0 : 1) && fillCalls == (CASE >= 2 ? 2 : 1) &&
        blockFrees == (CASE >= 2 ? 1 : 0) &&
        localFrees == (CASE == 0 ? 0 : ((CASE == 1 || CASE == 15) ? 1 : 2)) &&
        mainCalls == ((CASE == 6 || CASE == 16) ? 1 : 0) &&
        code == ((CASE == 6 || CASE == 16) ? 77U : 253U);
    TerminateProcess(GetCurrentProcess(), ok ? 0U : 1U);
    for (;;) { }
}
