#include "api.h"
int __attribute__((preserve_most)) t09_most(int a,int b,int c) { return a*100+b*10+c; }
int __attribute__((preserve_all)) t09_all(int a,int b,int c) { return a*1000+b*10+c; }
T09MostCallback t09_get_most(void) { return t09_most; }
T09AllCallback t09_get_all(void) { return t09_all; }
