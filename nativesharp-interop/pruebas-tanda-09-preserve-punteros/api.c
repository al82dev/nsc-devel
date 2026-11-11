#include "api.h"
unsigned char* __attribute__((preserve_most)) t09_most(unsigned char* p, int x) { *p+=(unsigned char)x; return p; }
void __attribute__((preserve_most)) t09_most_void(void* p, int x) { *(int*)p+=x; }
short* __attribute__((preserve_all)) t09_all_ptr(short* p, int x) { *p+=(short)x; return p; }
void __attribute__((preserve_all)) t09_all(void* p, int x) { *(int*)p+=x; }
MostPtr t09_get_most(void) { return t09_most; }
MostVoid t09_get_most_void(void) { return t09_most_void; }
AllPtr t09_get_all_ptr(void) { return t09_all_ptr; }
AllVoid t09_get_all(void) { return t09_all; }
