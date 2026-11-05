__declspec(dllimport) int dll_value;
__declspec(dllimport) int dll_function(int);
extern "C" int use_dll() { return dll_function(dll_value); }
