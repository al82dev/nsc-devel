#include "Api.h"
static Pair pair = { 17 };
Pair* make_pair(void) { return &pair; }
int read_pair(Pair* value) { return value->value; }
static struct Parent parent = {{ 1 }, {{ 2 }, { 3 }}};
struct Parent* make_parent(void) { return &parent; }
int read_parent(struct Parent* value) { return value->child.value + value->children[1].value; }
void add_distinct(int* restrict destination, const int* restrict source) { *destination += *source; }
static struct Device device = { 7 };
struct Device* get_device(void) { return &device; }
int read_device(struct Device* value) { return value->counter; }
int shared_value = 7;
int read_shared_value(void) { return shared_value; }
