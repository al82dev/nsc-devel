int project_sum(int left, int right) { return left + right; }
int project_shared = 0;
volatile int project_flag = 0;
const int project_limit = 9;
const volatile int project_status = 4;
static int project_pointer_storage = 10;
int* project_pointer = &project_pointer_storage;
int* const project_fixed_pointer = &project_pointer_storage;
int* volatile project_volatile_pointer = &project_pointer_storage;
static const int project_readonly_storage = 13;
const int* project_readonly_pointer = &project_readonly_storage;
int project_read_pointer_value(void) { return project_pointer_storage; }
int project_read_shared(void) { return project_shared; }
int project_read_flag(void) { return project_flag; }
