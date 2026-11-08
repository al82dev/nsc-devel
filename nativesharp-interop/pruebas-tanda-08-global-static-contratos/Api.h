static const char* text = "ok";
static unsigned short values[4] = { 1, 2, 3, 4 };
static volatile unsigned int ticks = 5;
static _Thread_local unsigned int local = 6;
static _Thread_local _Atomic(unsigned int) atomic_local = 7;
struct AtomicBox { int value; };
enum AtomicMode { atomic_low = 11, atomic_high = 29 };
static _Atomic(enum AtomicMode) atomic_mode = atomic_low;
static struct AtomicBox boxes[2] = {{31}, {47}};
static _Bool bits[2] = {0, 1};
static enum AtomicMode modes[2] = {atomic_low, atomic_high};
static _Atomic(struct AtomicBox*) box_ptr = &boxes[0];
static _Thread_local _Atomic(_Bool*) bool_ptr = &bits[0];
static _Atomic(enum AtomicMode*) mode_ptr = &modes[0];
static _Atomic(void*) opaque_ptr = &boxes[0];
static _Atomic(const struct AtomicBox*) const_box_ptr = &boxes[0];
static inline struct AtomicBox* next_box(void) { return &boxes[1]; }
static inline _Bool* next_bool(void) { return &bits[1]; }
static inline enum AtomicMode* next_mode(void) { return &modes[1]; }
static inline void* next_opaque(void) { return &boxes[1]; }
static inline const struct AtomicBox* next_const_box(void) { return &boxes[1]; }
static inline int inspect(void) {
    return text[0] == 'o' && values[3] == 14 && ticks == 15 && local == 16 && atomic_local == 17
        && box_ptr == &boxes[1] && bool_ptr == &bits[1]
        && mode_ptr == &modes[1] && opaque_ptr == &boxes[1] && const_box_ptr == &boxes[1]
        && atomic_mode == atomic_high;
}
