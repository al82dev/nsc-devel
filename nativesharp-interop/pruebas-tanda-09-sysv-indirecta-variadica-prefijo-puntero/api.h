#pragma once
typedef struct T09IndirectVarRecord { double x; int y; } T09IndirectVarRecord;
typedef int (__attribute__((sysv_abi)) *T09IndirectVarCallback)(int *, ...);
T09IndirectVarCallback t09_get_var_record_callback(void);
