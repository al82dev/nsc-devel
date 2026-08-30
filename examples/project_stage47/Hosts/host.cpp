struct NS_Pair
{
    int Left;
    int Right;
};

struct NS_Counter;

extern "C" __declspec(dllimport) int NS_Add(int left, int right);
extern "C" __declspec(dllimport) void NS_SetTransform(int (*callback)(int));
extern "C" __declspec(dllimport) int NS_ApplyTransform(int value);
extern "C" __declspec(dllimport) NS_Pair NS_EchoPair(NS_Pair value);
extern "C" __declspec(dllimport) NS_Counter* NS_Counter_Create(int initialValue);
extern "C" __declspec(dllimport) int NS_Counter_Get(NS_Counter* counter);
extern "C" __declspec(dllimport) void NS_Counter_Destroy(NS_Counter* counter);

static int TripleValue(int value)
{
    return value * 3;
}

extern "C" __declspec(dllexport) int HostProbeCpp(void)
{
    NS_Counter* counter = NS_Counter_Create(42);
    NS_SetTransform(TripleValue);
    const int valid = NS_Add(30, 12) == 42 &&
        NS_ApplyTransform(14) == 42 &&
        NS_Counter_Get(counter) == 42;
    NS_Counter_Destroy(counter);
    return valid ? 0 : 1;
}
