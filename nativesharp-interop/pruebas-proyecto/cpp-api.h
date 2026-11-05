namespace tools {
extern int shared;
extern volatile int flagged;
extern const int limit;
extern const volatile int status;
extern thread_local int tls;
extern int* pointer;
extern int* const fixed_pointer;
extern int* volatile volatile_pointer;
extern const int* readonly_pointee;
int read_const(const int* value);
int read_mutable(int* value);
int read_pointer_value();
int read_shared();
int read_flagged();
int blend(int value);
double blend(double value);
}
