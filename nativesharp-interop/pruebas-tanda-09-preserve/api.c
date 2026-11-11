#include "api.h"
int __attribute__((preserve_most)) t09_most(int a, int b, int c) { return a*100+b*10+c; }
int __attribute__((preserve_all)) t09_all(int a, int b, int c) { return a*1000+b*10+c; }
