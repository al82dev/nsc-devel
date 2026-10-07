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
        std::filesystem::path(NSC_BINARY_DIR) / "item339_com_vtable_production";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    writeFile(root / "ComProducer.nsproj",
        "<Project><PropertyGroup><Name>ComProducer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public extern C delegate int QueryMethod(void* self, int code);
public extern C delegate int RefMethod(void* self);

public struct RawComInterface
{
    public nint* VTable;
    public int State;
}

public class ComFactory
{
    public static int Query(void* self, int code)
    {
        RawComInterface* instance = (RawComInterface*)self;
        return instance->State + code;
    }

    public static int AddRef(void* self)
    {
        RawComInterface* instance = (RawComInterface*)self;
        instance->State = instance->State + 1;
        return instance->State;
    }

    public static int Release(void* self)
    {
        RawComInterface* instance = (RawComInterface*)self;
        instance->State = instance->State - 1;
        return instance->State;
    }

    public static void Initialize(nint* table, RawComInterface* instance)
    {
        QueryMethod query = Query;
        RefMethod addRef = AddRef;
        RefMethod release = Release;
        table[0] = (nint)query;
        table[1] = (nint)addRef;
        table[2] = (nint)release;
        instance->VTable = table;
        instance->State = 40;
    }
}

public class Program
{
    public static int Main()
    {
        nint* table = stackalloc nint[3];
        RawComInterface instance;
        ComFactory.Initialize(table, &instance);

        QueryMethod query = (QueryMethod)instance.VTable[0];
        RefMethod addRef = (RefMethod)instance.VTable[1];
        RefMethod release = (RefMethod)instance.VTable[2];
        if (addRef(&instance) != 41) { return 1; }
        if (release(&instance) != 40) { return 2; }
        return query(&instance, 2);
    }
}
)NS");

    std::string diagnostic;
    const bool o0Passed =
        compileAndRun(root / "ComProducer.nsproj", root / "ComProducerO0.exe", "-O0", diagnostic);
    tests.expect(o0Passed, "NativeSharp-produced COM vtable executes at O0 (" + diagnostic + ")");
    diagnostic.clear();
    const bool o2Passed =
        compileAndRun(root / "ComProducer.nsproj", root / "ComProducerO2.exe", "-O2", diagnostic);
    tests.expect(o2Passed, "NativeSharp-produced COM vtable executes at O2 (" + diagnostic + ")");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
