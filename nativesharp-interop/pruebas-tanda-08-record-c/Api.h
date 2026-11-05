typedef struct Pair { int value; } Pair;
Pair* make_pair(void);
int read_pair(Pair* value);
struct Child { int value; };
struct Parent { struct Child child; struct Child children[2]; };
struct Parent* make_parent(void);
int read_parent(struct Parent* value);
void add_distinct(int* restrict destination, const int* restrict source);
