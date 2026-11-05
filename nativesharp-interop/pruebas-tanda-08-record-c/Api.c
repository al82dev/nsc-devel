#include "Api.h"
static Pair pair = { 17 };
Pair* make_pair(void) { return &pair; }
int read_pair(Pair* value) { return value->value; }
static struct Parent parent = {{ 1 }, {{ 2 }, { 3 }}};
struct Parent* make_parent(void) { return &parent; }
int read_parent(struct Parent* value) { return value->child.value + value->children[1].value; }
