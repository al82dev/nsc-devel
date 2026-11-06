typedef struct CRecord { int value; } CRecord;
CRecord* get_c_record(void);
int get_c_number(void);
extern int shared_c_number;
enum CGlobalKind { CGlobalChoice = 21 };
int get_shared_c_number(void);
