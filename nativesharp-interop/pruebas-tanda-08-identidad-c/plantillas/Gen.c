#include "Gen.h"
int gen_c_counter = 5;
static struct GenRecord gen_record = {21};
int gen_c_number(void) { return 37; }
struct GenRecord* gen_c_record(void) { return &gen_record; }
