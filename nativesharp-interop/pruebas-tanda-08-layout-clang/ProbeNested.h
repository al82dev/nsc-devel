typedef struct LGuestHost { struct LGuest { int g; double h; } guest; int k; } LGuestHost;
LGuestHost* probe_nested(void);
struct LGuest* probe_guest(void);
int probe_nested_check(void);
