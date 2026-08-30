#include "Stage48_3HostApi.h"

static int32_t Twice(int32_t value)
{
    return value * 2;
}

static int32_t SumBuffer(int32_t* values, int32_t count)
{
    int32_t sum = 0;
    int32_t i;
    for (i = 0; i < count; ++i)
    {
        sum += values[i];
    }
    return sum;
}

static int run_contract(void)
{
    int32_t values[4] = { 1, 2, 3, 4 };
    NS_Pair pair = { 20, 22 };
    NS_Pair echoed;
    NS_LayoutProbe layout = { 1, 42 };
    NS_LayoutProbe echoedLayout;
    NS_Counter* counter;
    int marker = 7;

    if (NS48_Add(20, 22) != 42)
    {
        return 1;
    }

    if (NS48_EchoIntPointer(values) != values)
    {
        return 2;
    }

    if (NS48_InvokeBuffer(SumBuffer, values, 4) != 10)
    {
        return 3;
    }

    echoed = NS48_EchoPair(pair);
    if (echoed.Left != 20 || echoed.Right != 22)
    {
        return 4;
    }

    if (NS48_EchoMode((NS_HostMode)2) != (NS_HostMode)2)
    {
        return 5;
    }

    echoedLayout = NS48_EchoLayout(layout);
    if (echoedLayout.Tag != 1 || echoedLayout.Value != 42)
    {
        return 6;
    }

    if (NS48_EchoPointer(&marker) != &marker)
    {
        return 7;
    }

    if (NS48_Invoke(Twice, 21) != 42)
    {
        return 8;
    }

    NS48_SetCallback(Twice);
    if (NS48_CallCallback(21) != 42)
    {
        return 9;
    }

    counter = NS48_Counter_Create(40);
    if (counter == NULL)
    {
        return 10;
    }

    NS48_Counter_Add(counter, 2);
    if (NS48_Counter_Get(counter) != 42)
    {
        NS48_Counter_Destroy(counter);
        return 11;
    }

    NS48_Counter_Destroy(counter);
    return 0;
}

int main(void)
{
    return run_contract();
}
