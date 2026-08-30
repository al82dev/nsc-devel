#include "Stage48_4HostApi.h"

static int32_t Twice(int32_t value)
{
    return value * 2;
}

static int32_t SumBuffer(int32_t* values, int32_t count)
{
    int32_t sum = 0;
    for (int32_t i = 0; i < count; ++i)
    {
        sum += values[i];
    }
    return sum;
}

class CounterHandle final
{
public:
    explicit CounterHandle(int32_t value)
        : handle_(NS484_Counter_Create(value))
    {
    }

    ~CounterHandle()
    {
        if (handle_ != nullptr)
        {
            NS484_Counter_Destroy(handle_);
        }
    }

    CounterHandle(const CounterHandle&) = delete;
    CounterHandle& operator=(const CounterHandle&) = delete;

    bool valid() const
    {
        return handle_ != nullptr;
    }

    void add(int32_t delta)
    {
        NS484_Counter_Add(handle_, delta);
    }

    int32_t value() const
    {
        return NS484_Counter_Get(handle_);
    }

private:
    NS_Counter* handle_;
};

static int run_contract()
{
    int32_t values[4] = {1, 2, 3, 4};
    NS_Pair pair{20, 22};
    NS_LayoutProbe layout{1, 42};
    int marker = 7;

    if (NS484_Add(20, 22) != 42) return 1;
    if (NS484_EchoIntPointer(values) != values) return 2;
    if (NS484_InvokeBuffer(SumBuffer, values, 4) != 10) return 3;
    if (NS484_EchoPointer(&marker) != &marker) return 4;

    pair = NS484_EchoPair(pair);
    if (pair.Left != 20 || pair.Right != 22) return 5;
    if (NS484_EchoMode(static_cast<NS_HostMode>(2)) != static_cast<NS_HostMode>(2)) return 6;
    layout = NS484_EchoLayout(layout);
    if (layout.Tag != 1 || layout.Value != 42) return 7;

    if (NS484_Invoke(Twice, 21) != 42) return 8;
    NS484_SetCallback(Twice);
    if (NS484_CallCallback(21) != 42) return 9;

    CounterHandle counter(40);
    if (!counter.valid()) return 10;
    counter.add(2);
    if (counter.value() != 42) return 11;

    return 0;
}

int main()
{
    return run_contract();
}
