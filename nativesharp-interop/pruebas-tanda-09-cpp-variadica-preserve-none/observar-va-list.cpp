struct Box { __builtin_va_list list; unsigned long long guard[3]; };

__attribute__((noinline, preserve_none)) int inspect_preserve(int n, ...) {
    Box box;
    box.guard[0] = 0x5555555555555555ULL;
    box.guard[1] = 0x5555555555555555ULL;
    box.guard[2] = 0x5555555555555555ULL;
    __builtin_va_start(box.list, n);
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&box);
    const bool unexpected_pointer = bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0 &&
        bytes[4] == 0x30 && bytes[5] == 0 && bytes[6] == 0 && bytes[7] == 0;
    const bool guard_changed = box.guard[0] != 0x5555555555555555ULL || box.guard[1] != 0x5555555555555555ULL ||
        box.guard[2] != 0x5555555555555555ULL;
    __builtin_va_end(box.list);
    return (sizeof(box.list) == 8 ? 1 : 0) | (unexpected_pointer ? 2 : 0) | (guard_changed ? 4 : 0);
}

__attribute__((noinline)) int inspect_cdecl(int n, ...) {
    Box box;
    box.guard[0] = 0x5555555555555555ULL;
    box.guard[1] = 0x5555555555555555ULL;
    box.guard[2] = 0x5555555555555555ULL;
    __builtin_va_start(box.list, n);
    const bool guard_changed = box.guard[0] != 0x5555555555555555ULL || box.guard[1] != 0x5555555555555555ULL ||
        box.guard[2] != 0x5555555555555555ULL;
    __builtin_va_end(box.list);
    return (sizeof(box.list) == 8 ? 1 : 0) | (guard_changed ? 4 : 0);
}

int main() { int a = inspect_preserve(1, 9); int b = inspect_cdecl(1, 9); return a == 7 && b == 1 ? 0 : (a + 8 * b + 16); }
