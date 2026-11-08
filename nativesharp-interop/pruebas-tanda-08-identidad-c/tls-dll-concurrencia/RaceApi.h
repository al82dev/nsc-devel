typedef int (*TlsRaceCallback)(int id);
__declspec(dllimport) int tls_race_first_access(TlsRaceCallback callback);
