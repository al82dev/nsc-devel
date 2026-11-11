#pragma once
typedef struct T09IndirectVarRecord { int a; int b; int c; int d; int e; } T09IndirectVarRecord;
typedef struct T09IndirectVarMixed { double x; int y; } T09IndirectVarMixed;
typedef int (__attribute__((sysv_abi)) *T09IndirectVarCallback)(int, ...);
T09IndirectVarCallback t09_get_var_record_callback(void);
