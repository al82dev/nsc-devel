__declspec(dllimport) int dependency(void);
__declspec(dllexport) int c_increment(int x) { return x + dependency(); }
