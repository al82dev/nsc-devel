typedef struct NS_Pair
{
    int Left;
    int Right;
} NS_Pair;

typedef void NS_Counter;

__declspec(dllimport) int NS_Add(int left, int right);
__declspec(dllimport) void NS_SetTransform(int (*callback)(int));
__declspec(dllimport) int NS_ApplyTransform(int value);
__declspec(dllimport) NS_Pair NS_EchoPair(NS_Pair value);
__declspec(dllimport) NS_Counter* NS_Counter_Create(int initialValue);
__declspec(dllimport) int NS_Counter_Get(NS_Counter* counter);
__declspec(dllimport) void NS_Counter_Destroy(NS_Counter* counter);

static int DoubleValue(int value)
{
    return value * 2;
}

__declspec(dllexport) int HostProbe(void)
{
    NS_Pair pair = { 20, 22 };
    NS_Pair echoed = NS_EchoPair(pair);
    NS_Counter* counter = NS_Counter_Create(42);
    NS_SetTransform(DoubleValue);

    const int valid =
        NS_Add(20, 22) == 42 &&
        NS_ApplyTransform(21) == 42 &&
        echoed.Left == 20 &&
        echoed.Right == 22 &&
        NS_Counter_Get(counter) == 42;

    NS_Counter_Destroy(counter);
    return valid ? 0 : 1;
}
