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
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view extraItems = {})
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>" + std::string(extraItems) +
        "</Project>");
    writeFile(root / "Main.ns", source);
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item330_volatile_bitfield_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto valid = root / "valid";
    writeProject(valid, "VolatileBitFieldsItem330", R"NS(
public struct Registers
{
    public volatile uint Ready : 1;
    public volatile uint Error : 3;
    public uint Neighbor : 4;
}
public class Device
{
    public volatile uint State : 4;
}
public class Program
{
    public static int Main()
    {
        Registers registers = { 1, 2, 10 };
        if (registers.Ready != 1 || registers.Error != 2 || registers.Neighbor != 10)
        {
            return 1;
        }
        registers.Error += 3;
        registers.Ready++;
        if (registers.Ready != 0 || registers.Error != 5 || registers.Neighbor != 10)
        {
            return 2;
        }
        raii Device device = new Device();
        device.State = 9;
        if (device.State != 9) { return 3; }
        return 0;
    }
}
)NS");
    const std::string project = (valid / "VolatileBitFieldsItem330.nsproj").string();
    std::string output;
    std::string error;

    for (const std::string_view optimization : {std::string_view("-O0"), std::string_view("-O2")})
    {
        const std::array<std::string_view, 3> llvmArguments = {
            "--dump-project-oop-llvm", project, optimization};
        const bool lowered = runDriver(llvmArguments, output, error) == ExitCode::Success;
        tests.expect(lowered, std::string(optimization) + " volatile bit fields lower (" + error + ")");
        if (lowered)
        {
            tests.expect(output.find("load volatile i32") != std::string::npos &&
                             output.find("store volatile i32") != std::string::npos,
                std::string(optimization) + " accesses the complete allocation unit as volatile");
            tests.expect(output.find("load atomic") == std::string::npos &&
                             output.find("store atomic") == std::string::npos &&
                             output.find("fence ") == std::string::npos,
                std::string(optimization) + " volatile remains non-atomic");
        }

        const std::string suffix = optimization == "-O0" ? "O0" : "O2";
        const std::string executable = (valid / ("VolatileBitFields" + suffix + ".exe")).string();
        const std::array<std::string_view, 5> executableArguments = {
            optimization, "--emit-project-oop-exe", project, "-o", executable};
        const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
        tests.expect(emitted, suffix + " volatile bit-field executable emits (" + error + ")");
        if (emitted)
        {
            tests.expect(std::system(("\"" + executable + "\"").c_str()) == 0,
                suffix + " volatile updates preserve adjacent fields");
        }
    }

    const auto producer = root / "producer";
    writeFile(producer / "Producer.nsproj",
        "<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Registers.ns", R"NS(
namespace Shared
{
    public struct Registers { public volatile uint Ready : 1; public uint Code : 7; }
}
)NS");
    const std::string producerProject = (producer / "Producer.nsproj").string();
    const std::string producerLibrary = (producer / "Producer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    const bool producerEmitted =
        runDriver(producerArguments, output, error) == ExitCode::Success;
    tests.expect(producerEmitted,
        "volatile bit-field producer emits NSMETA 1.18 (" + error + ")");

    const auto consumer = root / "consumer";
    const std::string references =
        "<ItemGroup><LibraryReference Include=\"" +
        (producer / "Producer.lib").generic_string() + "\" Metadata=\"" +
        (producer / "Producer.nsmeta").generic_string() + "\" /></ItemGroup>";
    writeProject(consumer, "Consumer", R"NS(
using Shared;
public class Consumer
{
    public static uint Touch(Registers* value)
    {
        value->Ready = 1;
        return value->Ready;
    }
}
)NS", references);
    const std::string consumerProject = (consumer / "Consumer.nsproj").string();
    const std::array<std::string_view, 3> consumerArguments = {
        "--dump-project-oop-llvm", consumerProject, "-O0"};
    output.clear();
    error.clear();
    const bool consumed = producerEmitted &&
        runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumed, "LibraryReference reconstructs volatile bit fields (" + error + ")");
    tests.expect(consumed && output.find("load volatile i32") != std::string::npos &&
                     output.find("store volatile i32") != std::string::npos,
        "precompiled volatile bit fields retain unit-level volatile accesses");

    return tests.resultKeeping(root);
}
