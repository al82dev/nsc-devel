#include "Api.h"
volatile _Bool flag;
int c_read_flag(void) { return flag; }
void c_write_flag(int value) { flag = value; }
