typedef struct Opaque Opaque;
extern Opaque *opaque_current;
Opaque *opaque_get(void);
int opaque_read(Opaque *object);
void opaque_set(Opaque *object, int value);
