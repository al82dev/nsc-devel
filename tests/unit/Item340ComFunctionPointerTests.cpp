#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization,
    std::string& diagnostic)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    if (driver.run(arguments, out, err) != ExitCode::Success)
    {
        diagnostic = err.str() + out.str();
        return false;
    }
    const int result = std::system(("\"" + executableText + "\"").c_str());
    if (result != 42)
    {
        diagnostic = "process result " + std::to_string(result) + ", expected 42";
        return false;
    }
    return true;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "item340_com_function_pointers";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    writeFile(root / "ComPointers.nsproj",
        "<Project><PropertyGroup><Name>ComPointers</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public extern C delegate int QueryInterfaceMethod(void* self, nint iid, void** result);
public extern C delegate uint ReferenceMethod(void* self);
public extern C delegate int ReadPairMethod(void* self, int* left, int* right);

public struct ComObject
{
    public nint* VTable;
    public int Bias;
}

public class Program
{
    public static int QueryInterface(void* self, nint iid, void** result)
    {
        if (iid != 7) { return -1; }
        *result = self;
        return 0;
    }

    public static uint AddRef(void* self)
    {
        ComObject* instance = (ComObject*)self;
        instance->Bias = instance->Bias + 1;
        return (uint)instance->Bias;
    }

    public static int ReadPair(void* self, int* left, int* right)
    {
        ComObject* instance = (ComObject*)self;
        *left = instance->Bias;
        *right = 40;
        return 0;
    }

    public static int Main()
    {
        QueryInterfaceMethod queryAddress = QueryInterface;
        ReferenceMethod addRefAddress = AddRef;
        ReadPairMethod readAddress = ReadPair;

        nint* table = stackalloc nint[3];
        table[0] = (nint)queryAddress;
        table[1] = (nint)addRefAddress;
        table[2] = (nint)readAddress;

        ComObject instance;
        instance.VTable = table;
        instance.Bias = 1;

        QueryInterfaceMethod query = (QueryInterfaceMethod)instance.VTable[0];
        ReferenceMethod addRef = (ReferenceMethod)instance.VTable[1];
        ReadPairMethod read = (ReadPairMethod)instance.VTable[2];
        void* selected = null;
        if (query(&instance, 7, &selected) != 0) { return 1; }
        if (selected != &instance) { return 2; }
        if (addRef(selected) != 2) { return 3; }
        int left = 0;
        int right = 0;
        if (read(selected, &left, &right) != 0) { return 4; }
        return left + right;
    }
}
)NS");

    std::string diagnostic;
    tests.expect(
        compileAndRun(root / "ComPointers.nsproj", root / "ComPointersO0.exe", "-O0", diagnostic),
        "COM function pointers preserve typed delegate signatures at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        compileAndRun(root / "ComPointers.nsproj", root / "ComPointersO2.exe", "-O2", diagnostic),
        "COM function pointers preserve typed delegate signatures at O2 (" + diagnostic + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
