int external_double(int value);
extern int external_unused;
static int *unused_pointer = &external_unused;
static int unused_helper(void) { return external_unused; }
static int shared_state;
static int hidden_add(int value) { return value + 3; }
static inline int header_add(int left, int right) { return left + right; }
static inline int header_composed(int value) { return external_double(hidden_add(value)); }
static inline int header_counter(void) { static int count; return ++count; }
static inline void header_set(int value) { shared_state = value; }
static inline int header_get(void) { return shared_state; }
