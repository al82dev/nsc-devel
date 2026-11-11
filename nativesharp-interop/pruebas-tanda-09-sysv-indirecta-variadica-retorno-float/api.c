#include "api.h"
extern float __attribute__((sysv_abi)) t09_var_record(int, ...);
T09IndirectVarCallback t09_get_var_record_callback(void) { return t09_var_record; }
