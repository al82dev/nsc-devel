int external_double(int value);
static inline int header_add(int left, int right) { return left + right; }
static inline int header_composed(int value) { return external_double(header_add(value, 3)); }
static inline int header_counter(void) { static int count; return ++count; }
