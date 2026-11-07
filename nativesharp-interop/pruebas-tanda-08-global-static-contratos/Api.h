static const char* text = "ok";
static unsigned short values[4] = { 1, 2, 3, 4 };
static volatile unsigned int ticks = 5;
static _Thread_local unsigned int local = 6;
static _Thread_local _Atomic(unsigned int) atomic_local = 7;
struct AtomicBox { int value; };
static struct AtomicBox boxes[2] = {{31}, {47}};
static _Bool bits[2] = {0, 1};
static _Atomic(struct AtomicBox*) box_ptr = &boxes[0];
static _Thread_local _Atomic(_Bool*) bool_ptr = &bits[0];
static inline struct AtomicBox* next_box(void) { return &boxes[1]; }
static inline _Bool* next_bool(void) { return &bits[1]; }
static inline int inspect(void) {
    return text[0] == 'o' && values[3] == 14 && ticks == 15 && local == 16 && atomic_local == 17
        && box_ptr == &boxes[1] && bool_ptr == &bits[1];
}
