// Test doubles for the generated entry object; no production runtime is replaced.
extern "C" __declspec(dllimport) void* GetCurrentProcess();
extern "C" __declspec(dllimport) int TerminateProcess(void*, unsigned);
extern "C" __declspec(dllimport) void* GetStdHandle(unsigned);
extern "C" __declspec(dllimport) int WriteFile(void*, const void*, unsigned, unsigned*, void*);
extern "C" __declspec(dllimport) void* SetUnhandledExceptionFilter(void*);
extern "C" __declspec(dllimport) void RaiseException(unsigned, unsigned, unsigned, const unsigned long long*);

static unsigned long long block[16];
static char originals[4];
static char replacement;
static int allocCalls;
static int fillCalls;
static int mainCalls;
static int blockFrees;
static unsigned freedMask;
static int badFree;
static unsigned long long exceptionPayload;

struct ExceptionRecord
{
    unsigned code, flags;
    ExceptionRecord* nested;
    void* address;
    unsigned parameterCount, padding;
    unsigned long long parameters[15];
};
struct ExceptionPointers { ExceptionRecord* record; void* context; };
extern "C" long observeUnhandled(ExceptionPointers* pointers)
{
    auto record = pointers->record;
    char message[] = "00000000";
    message[0] = record->code == 0xE04E5301U ? '1' : '0';
    message[1] = (record->flags & 1U) != 0 ? '1' : '0';
    message[2] = record->parameterCount == 1 ? '1' : '0';
    message[3] = record->parameters[0] == exceptionPayload ? '1' : '0';
    message[4] = exceptionPayload == reinterpret_cast<unsigned long long>(&originals[1]) ? '1' : '0';
    message[5] = blockFrees == 0 ? '1' : '0';
    message[6] = freedMask == 0 ? '1' : '0';
    message[7] = badFree == 0 ? '1' : '0';
    unsigned written = 0;
    WriteFile(GetStdHandle(static_cast<unsigned>(-11)), message, 8, &written, nullptr);
    return 1;
}

extern "C" int __nativesharp_eh_init() { return 1; }
extern "C" int __nativesharp_panic_eh_filter(void*) { return 0; }
extern "C" void __chkstk() { }
extern "C" int __nativesharp_environment_get_command_line_arg_count()
{ return CASE == 0 ? 0 : 4; }
extern "C" void* __nativesharp_alloc(unsigned long long)
{
    ++allocCalls;
    return CASE == 1 ? nullptr : block;
}
extern "C" int __nativesharp_environment_get_command_line_args(void* destination)
{
    ++fillCalls;
    auto slots = static_cast<unsigned long long*>(destination);
    int filled = CASE >= 6 ? 4 : CASE - 2;
    for (int i = 0; i < filled; ++i)
        slots[i + 1] = reinterpret_cast<unsigned long long>(&originals[i]);
    return filled;
}
extern "C" void __nativesharp_free(void* value)
{
    if (value == block) { ++blockFrees; return; }
    for (unsigned i = 0; i < 4; ++i)
        if (value == &originals[i])
        {
            unsigned bit = 1U << i;
            if ((freedMask & bit) != 0) ++badFree;
            freedMask |= bit;
            return;
        }
    ++badFree;
}
extern "C" int test_main(void* argumentArray)
{
    ++mainCalls;
    auto slots = static_cast<unsigned long long*>(argumentArray);
    if (slots[0] != 3 || slots[1] != reinterpret_cast<unsigned long long>(&originals[1]))
        return 99;
    if (CASE == 7)
    {
        exceptionPayload = slots[1];
        SetUnhandledExceptionFilter(reinterpret_cast<void*>(&observeUnhandled));
        RaiseException(0xE04E5301U, 1, 1, &exceptionPayload);
        return 98;
    }
    slots[1] = reinterpret_cast<unsigned long long>(&replacement);
    return 77;
}
extern "C" __declspec(noreturn) void ExitProcess(unsigned code)
{
    int filled = CASE >= 2 && CASE <= 5 ? CASE - 2 : (CASE == 6 ? 4 : 0);
    unsigned expectedMask = (1U << filled) - 1U;
    bool ok = badFree == 0 && freedMask == expectedMask &&
        allocCalls == (CASE == 0 ? 0 : 1) &&
        fillCalls == (CASE >= 2 ? 1 : 0) &&
        blockFrees == (CASE >= 2 ? 1 : 0) &&
        mainCalls == (CASE == 6 ? 1 : 0) &&
        code == (CASE == 6 ? 77U : 253U);
    TerminateProcess(GetCurrentProcess(), ok ? 0U : 1U);
    for (;;) { }
}
