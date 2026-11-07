static int shared_state = 7;
static inline int header_get(void) { return shared_state; }
static inline void header_set(int value) { shared_state = value; }
