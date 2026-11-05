extern "C" int invoke_ns() __asm__("__nsc_function_Semantic$3A$3A__Main");
extern "C" __declspec(dllimport) void local_set_case(int);
extern "C" __declspec(dllimport) int local_result();
extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern "C" void mainCRTStartup() {
    for (int scenario = 2; scenario <= 6; ++scenario) {
        local_set_case(scenario);
        int caught = 0;
        try { invoke_ns(); } catch (int value) { caught = value; }
        const int expected = scenario == 5 ? 0 : (scenario == 6 ? 43 : 42);
        if (caught != expected || !local_result()) ExitProcess(1);
    }
    ExitProcess(0);
}
