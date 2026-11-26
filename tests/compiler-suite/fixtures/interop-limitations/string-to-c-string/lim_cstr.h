int text_length(const char* text);
int text_length_pair(const char* first, const char* second);
int text_is_null(const char* text);
#ifdef LIM_CSTR_IMPLEMENTATION
static int n_len(const char* t) { int n = 0; while (t[n]) ++n; return n; }
int text_length(const char* text) { return n_len(text); }
int text_length_pair(const char* first, const char* second) { return n_len(first) - n_len(second) + 6; }
int text_is_null(const char* text) { return text == 0; }
#endif
