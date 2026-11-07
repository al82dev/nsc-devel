static const char* text = "ok";
static unsigned short values[4] = { 1, 2, 3, 4 };
static volatile unsigned int ticks = 5;
static _Thread_local unsigned int local = 6;
static _Thread_local _Atomic(unsigned int) atomic_local = 7;
static inline int inspect(void) {
    return text[0] == 'o' && values[3] == 14 && ticks == 15 && local == 16 && atomic_local == 17;
}
