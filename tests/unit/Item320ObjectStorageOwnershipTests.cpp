#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
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

nativesharp::ExitCode runDriver(std::span<const std::string_view> arguments, std::string& output,
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

bool lineContains(std::string_view text, std::string_view anchor, std::string_view contract)
{
    const std::size_t position = text.find(anchor);
    if (position == std::string_view::npos) return false;
    const std::size_t lineEnd = text.find('\n', position);
    return text.substr(position, lineEnd - position).find(contract) != std::string_view::npos;
}
} // namespace

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root =
        std::filesystem::path(NSC_BINARY_DIR) / "item320_object_storage_ownership_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    writeFile(root / "Ownership.nsproj",
              "<Project><PropertyGroup><Name>OwnershipItem320</Name></PropertyGroup>"
              "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public class Widget
{
    public int Value;
    public Widget(int value) { Value = value; }
    ~Widget() { Value = 0; }
}

public class Program
{
    public static int Main()
    {
        long* bytes = stackalloc long[16];
        Widget* storage = (Widget*)bytes;
        Widget placed = new (storage) Widget(41);
        destroy placed;
        Widget heap = new Widget(42);
        delete heap;
        return 0;
    }
}
)NS");

    const std::string project = (root / "Ownership.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> bodyArguments = {
        "--dump-project-method-bodies", project, "-O0"};
    const bool body = runDriver(bodyArguments, output, error) == ExitCode::Success;
    tests.expect(body, "object/storage ownership reaches the semantic method model (" + error + ")");
    if (body)
    {
        tests.expect(lineContains(output, "name=bytes",
                                  "object-ownership=none storage-ownership=owned"),
                     "stack allocation owns storage but no object lifetime");
        tests.expect(lineContains(output, "name=storage",
                                  "object-ownership=none storage-ownership=owned"),
                     "pointer casts preserve storage provenance");
        tests.expect(lineContains(output, "name=placed",
                                  "object-ownership=owned storage-ownership=borrowed"),
                     "placement construction owns the object lifetime but borrows storage");
        tests.expect(lineContains(output, "name=heap",
                                  "object-ownership=owned storage-ownership=owned"),
                     "ordinary class construction owns object lifetime and storage");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "ownership axes lower to O0 HIR (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("placement-construction type=type:OwnershipItem320::Widget") !=
                             std::string::npos &&
                         output.find("object-ownership=owned storage-ownership=borrowed") !=
                             std::string::npos,
                     "HIR records placement object ownership separately from storage");
        tests.expect(output.find("object-destruction type=builtin:void") != std::string::npos,
                     "HIR retains the object-only destruction obligation");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "ownership axes lower to O0 MIR (" + error + ")");
    if (mir)
    {
        tests.expect(output.find("object-destroy type=builtin:void") != std::string::npos &&
                         output.find("object-ownership=owned storage-ownership=borrowed") !=
                             std::string::npos,
                     "MIR freezes object-only destruction as borrowed storage");
        tests.expect(output.find("deallocate type=builtin:void") != std::string::npos &&
                         output.find("object-ownership=owned storage-ownership=owned") !=
                             std::string::npos,
                     "MIR freezes delete as discharging both obligations");
    }

    return tests.resultKeeping(root);
}
