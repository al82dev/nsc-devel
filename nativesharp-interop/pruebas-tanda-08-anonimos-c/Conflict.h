typedef struct CConflict {
    union {
#ifdef NSC_DIFFERENT_MEMBER
        float item;
#else
        int item;
#endif
        int other;
    };
} CConflict;
