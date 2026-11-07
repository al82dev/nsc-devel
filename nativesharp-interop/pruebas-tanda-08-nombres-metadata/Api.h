typedef struct NRecord { char tag; int value; double real; } NRecord;
typedef NRecord NAlias;
typedef enum NMood { NSad = -1, NHappy = 4 } NMood;
NRecord* names_record(void);
void* names_raw(void);
int names_check(int value);
long long names_layout(int id);
