extern _Bool* c_bool_pointer;
_Bool* c_bool_get(void);
int c_bool_check(void);
void c_set_through_pointer(_Bool* value, _Bool next);
int c_read_through_pointer(const _Bool* value);
