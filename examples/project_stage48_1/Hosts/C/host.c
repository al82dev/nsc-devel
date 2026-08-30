#include "Stage48_1Host.h"

static int32_t Twice(int32_t value)
{
    return value * 2;
}

__declspec(dllexport) int Stage48_1CHostProbe(void)
{
    NS_Pair pair = {20, 22};
    NS_Pair echoed = EchoPair(pair);
    NS_Counter* counter = CounterCreate(42);
    int ok = Add(20, 22) == 42 &&
        echoed.X == 20 && echoed.Y == 22 &&
        Invoke(Twice, 21) == 42 &&
        CounterGet(counter) == 42;
    CounterDestroy(counter);
    return ok ? 0 : 1;
}
