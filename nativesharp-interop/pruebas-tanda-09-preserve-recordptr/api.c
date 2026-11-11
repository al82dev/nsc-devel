#include "api.h"
T09Record* __attribute__((preserve_most)) t09_most(T09Record* p, int x) { p->x+=x; return p; }
void __attribute__((preserve_most)) t09_most_void(int** p, int x) { **p+=x; }
int** __attribute__((preserve_all)) t09_all_ptr(int** p, int x) { **p+=x; return p; }
void __attribute__((preserve_all)) t09_all(T09Record* p, int x) { p->y+=x; }
MostPtr t09_get_most(void) { return t09_most; }
MostVoid t09_get_most_void(void) { return t09_most_void; }
AllPtr t09_get_all_ptr(void) { return t09_all_ptr; }
AllVoid t09_get_all(void) { return t09_all; }
