#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
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

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view edition = "3")
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::string(edition) +
        "</Language><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool rejectsWith(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view expected,
    std::string& observed,
    std::string_view edition = "3")
{
    std::string projectName(name);
    for (char& character : projectName)
        if (character == '-') character = '_';
    const std::filesystem::path project = writeProject(
        root / std::string(name), projectName, source, edition);
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-method-bodies", projectText, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    observed = error + output;
    return result != ExitCode::Success && observed.find(expected) != std::string::npos;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_pinning";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = writeProject(
        root / "positive", "Pinning", R"NS(
public class PinnedCell
{
    public static int Drops;
    public int Value;
    public nint SelfAddress;

    public PinnedCell(int value, PinnedCell* self)
    {
        Value = value;
        unsafe("the integer is used only to verify the stable pinned address")
        {
            SelfAddress = (nint)self;
        }
    }

    ~PinnedCell()
    {
        Drops = Drops + 1;
    }
}

public class Program
{
    public static int Main()
    {
        unsafe("stack storage remains live for the complete placement lifetime")
        {
            long* raw = stackalloc long[16];
            PinnedCell* storage = (PinnedCell*)raw;
            int observed = 0;
            {
                pin raii PinnedCell stable = new (storage) PinnedCell(40, storage);
                pin ref int projected = ref stable.Value;
                projected += 2;
                if (stable.SelfAddress != (nint)storage) return 1;
                observed = projected;
            }
            if (PinnedCell.Drops != 1) return 2;
            return observed;
        }
    }
}
)NS");

    const std::string projectText = positive.string();
    const std::string executableText = (positive.parent_path() / "Pinning.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const bool emitted = runDriver(emitArguments, output, error) == ExitCode::Success;
    bool ran = false;
    if (emitted) ran = std::system(("\"" + executableText + "\"").c_str()) == 42;
    tests.expect(emitted && ran,
        "pinned placement construction keeps its address and destroys the object in place once (" +
            error + output + ")");

    const std::array<std::string_view, 3> bodyArguments = {
        "--dump-project-method-bodies", projectText, "-O0"};
    output.clear();
    error.clear();
    const bool bodyPasses = runDriver(bodyArguments, output, error) == ExitCode::Success &&
        output.find("name=stable type=type:Pinning::PinnedCell ref=false") != std::string::npos &&
        output.find("raii=true pinned=true") != std::string::npos &&
        output.find("name=projected type=builtin:int ref=true") != std::string::npos &&
        output.find("pinned=true") != std::string::npos &&
        output.find("object-ownership=owned storage-ownership=borrowed") != std::string::npos;
    tests.expect(bodyPasses,
        "method semantics preserve pinned owner, pinned projection, and borrowed placement storage (" +
            error + output + ")");

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", projectText, "-O0"};
    output.clear();
    error.clear();
    const bool mirPasses = runDriver(mirArguments, output, error) == ExitCode::Success &&
        output.find("reference-local=true pinned=true") != std::string::npos &&
        output.find("destruction=scope-bound") != std::string::npos &&
        output.find("placement-construct") != std::string::npos;
    tests.expect(mirPasses,
        "MIR transports the stable-address and in-place destruction contract (" +
            error + output + ")");

    std::string observed;
    tests.expect(!rejectsWith(root, "edition-two", R"NS(
public class Program
{
    public static int Main()
    {
        int value = 42;
        pin ref int stable = ref value;
        return stable;
    }
}
)NS", "pin ref locals requires <Language>3</Language>", observed, "2"),
        "pin ref is independent of historical edition input (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "dynamic-container", R"NS(
public class Program
{
    public static int Main()
    {
        int[] values = new int[2];
        int index = 1;
        pin ref int element = ref values[index];
        return element;
    }
}
)NS", "pin ref requires a statically stable storage place", observed),
        "pin ref rejects a dynamically selected container element (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "move-owner", R"NS(
public class Cell { }
public class Program
{
    public static int Main()
    {
        pin raii Cell stable = new Cell();
        owned Cell moved = move(stable);
        return 42;
    }
}
)NS", "cannot move pinned owner 'stable'", observed),
        "a pinned owner cannot be relocated by move (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "replace-owner", R"NS(
public class Cell { }
public class Program
{
    public static int Main()
    {
        pin raii Cell stable = new Cell();
        owned Cell replacement = new Cell();
        stable = move(replacement);
        return 42;
    }
}
)NS", "cannot replace pinned owner 'stable'", observed),
        "a pinned owner cannot be replaced (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "captured-projection", R"NS(
public delegate int Getter();
public class Program
{
    public static int Main()
    {
        int value = 42;
        pin ref int stable = ref value;
        Getter getter = () => stable;
        return getter();
    }
}
)NS", "cannot be captured by a lambda because the delegate may outlive the referenced storage", observed),
        "a pinned projection cannot escape into a future coroutine/closure frame (" + observed + ")");

    return tests.resultKeeping(root);
}
