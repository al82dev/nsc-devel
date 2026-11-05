struct InteropFailure {
    int code;
#ifdef CPP_EXCEPTION_NONTRIVIAL
    ~InteropFailure();
#endif
};
