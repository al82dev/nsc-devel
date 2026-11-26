#ifndef LIM_SINGLE_H
#define LIM_SINGLE_H
int lim_bump(void);
#ifdef LIM_IMPLEMENTATION
#include <lim_sys.h>
static int lim_counter;
static int lim_calls;
int lim_bump(void)
{
    lim_calls = lim_sys_add(lim_calls, 1);
    lim_counter = lim_sys_add(lim_counter, 21);
    return lim_counter;
}
#endif
#endif
