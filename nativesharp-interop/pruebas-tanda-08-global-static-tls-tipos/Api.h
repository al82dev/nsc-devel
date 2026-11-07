enum StaticTone { ToneLow = 2, ToneHigh = 9 };
static _Thread_local unsigned char byte_value = 11;
static _Thread_local short short_value = -12;
static _Thread_local unsigned long long large_value = 13;
static _Thread_local float float_value = 1.5f;
static _Thread_local double double_value = 2.5;
static _Thread_local _Bool truth = 1;
static _Thread_local enum StaticTone tone = ToneLow;
static _Thread_local _Atomic(int) atomic_value = 15;
static _Thread_local int values[3] = { 16, 17, 18 };
static _Thread_local int* pointer_value = 0;
static int target = 5;
static inline enum StaticTone high_tone(void) { return ToneHigh; }
static inline int* target_address(void) { return &target; }
static inline int inspect(void) {
    return byte_value == 21 && short_value == -22 && large_value == 23 &&
        float_value == 3.5f && double_value == 4.5 && !truth && tone == ToneHigh &&
        atomic_value == 25 && values[2] == 26 && pointer_value == &target && *pointer_value == 27;
}
