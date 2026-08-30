#include "Stage48_1Host.h"

static int32_t Triple(int32_t value)
{
    return value * 3;
}

extern "C" __declspec(dllexport) int Stage48_1CppHostProbe(void)
{
    NS_Pair pair{30, 12};
    const NS_Pair echoed = EchoPair(pair);
    NS_Counter* counter = CounterCreate(42);
    const bool ok = Add(30, 12) == 42 &&
        echoed.X == 30 && echoed.Y == 12 &&
        Invoke(Triple, 14) == 42 &&
        CounterGet(counter) == 42;
    CounterDestroy(counter);
    return ok ? 0 : 1;
}
