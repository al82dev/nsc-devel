#pragma once
typedef struct T09MixedArgs { double x; int y; } T09MixedArgs;
typedef int (__attribute__((sysv_abi)) *T09MixedArgsCallback)(int, T09MixedArgs, int);
T09MixedArgsCallback t09_get_mixed_args_callback(void);
int t09_mixed_args_next(void);
