int gen_c_number(void);
extern int gen_c_counter;
typedef int GenCount;
struct GenRecord { int value; };
struct GenRecord* gen_c_record(void);
enum GenMode { GenModeA = 10, GenModeB = 11 };
enum { GenLoose = 12 };
static inline int gen_c_twice(int value) { return value * 2; }
