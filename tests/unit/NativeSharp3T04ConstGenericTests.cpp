#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
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
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
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

bool compileAndRun(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
    const std::string projectText = project.string();
    const std::string executableText = (directory / (std::string(name) + ".exe")).string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success)
    {
        std::cerr << diagnostic << '\n';
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    std::cerr << diagnostic << '\n';
    return false;
}

bool rejected(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", projectText, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    const bool accepted = result != ExitCode::Success &&
        diagnostic.find(expected) != std::string::npos;
    if (!accepted) std::cerr << diagnostic << '\n';
    return accepted;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_const_generics";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    const bool expressionRuntime = compileAndRun(root / "expression", "ConstExpression", R"NS(
struct Buffer<T, const int N>
{
    public T[N] Data;
    public int Count() { return N; }
}
class Program
{
    public static int Main()
    {
        Buffer<int, (2 + 2)> value;
        value.Data[0] = 38;
        return value.Data[0] + value.Count();
    }
}
)NS", 42, diagnostic);
    tests.expect(expressionRuntime,
        "parenthesized integral const expressions affect layout and runtime (" + diagnostic + ")");

    diagnostic.clear();
    const bool matrixRuntime = compileAndRun(root / "matrix", "ConstMatrix", R"NS(
struct Matrix<T, const int Rows, const int Columns>
{
    public T[Rows][Columns] Data;
}
class Program
{
    public static int Main()
    {
        Matrix<int, 2, 3> value;
        value.Data[2][1] = 42;
        return value.Data[2][1];
    }
}
)NS", 42, diagnostic);
    tests.expect(matrixRuntime,
        "two independent const dimensions determine matrix layout (" + diagnostic + ")");

    diagnostic.clear();
    const bool comparisonRuntime = compileAndRun(root / "comparison", "ComparisonLookahead", R"NS(
class Program
{
    private static long Minimum() { return 1; }
    private static long Maximum() { return 100; }
    public static int Main()
    {
        long wide = 42;
        if (wide < (long)Minimum() || wide > (long)Maximum()) return 1;
        return 42;
    }
}
)NS", 42, diagnostic);
    tests.expect(comparisonRuntime,
        "const-generic lookahead does not consume ordinary chained comparisons (" +
            diagnostic + ")");

    diagnostic.clear();
    const bool negativeRuntime = compileAndRun(root / "negative", "NegativeConst", R"NS(
class Program
{
    private static int Number<const int N>() { return N; }
    public static int Main() { return Number<(-1)>() + 43; }
}
)NS", 42, diagnostic);
    tests.expect(negativeRuntime,
        "negative const values preserve signed identity and body substitution (" + diagnostic + ")");

    diagnostic.clear();
    const bool inferredRuntime = compileAndRun(root / "inference", "ConstInference", R"NS(
struct Value<const int N>
{
    public int Number;
}
class Program
{
    private static Value<N> Make<const int N>()
    {
        Value<N> result;
        result.Number = N;
        return result;
    }
    public static int Main()
    {
        Value<42> value = Make();
        return value.Number;
    }
}
)NS", 42, diagnostic);
    tests.expect(inferredRuntime,
        "an exact expected generic type infers const parameters without explicit arguments (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "inference-conflict", "ConstInferenceConflict", R"NS(
struct Pair<const int Left, const int Right>
{
    public int Value;
}
class Program
{
    private static Pair<N, N> Same<const int N>()
    {
        Pair<N, N> result;
        return result;
    }
    public static int Main()
    {
        Pair<2, 3> conflict = Same();
        return 0;
    }
}
)NS", "cannot be inferred", diagnostic),
        "conflicting expected const arguments reject inference deterministically (" +
            diagnostic + ")");

    diagnostic.clear();
    const bool enumRuntime = compileAndRun(root / "enum", "EnumConst", R"NS(
enum Mode : byte { None = 0, Base = 40, Answer = Base + 2 }
class Program
{
    private static int Number<const Mode M>() { return (int)M; }
    public static int Main() { return Number<Mode.Answer>(); }
}
)NS", 42, diagnostic);
    tests.expect(enumRuntime,
        "enum const values use declaration identity and normalized underlying bits (" +
            diagnostic + ")");

    diagnostic.clear();
    const bool structuralRuntime = compileAndRun(root / "structural", "StructuralConst", R"NS(
class Program
{
    private static int Score<const (int, bool) Key>()
    {
        var (number, enabled) = Key;
        return enabled ? number + 40 : 0;
    }
    public static int Main()
    {
        return Score<(2, true)>();
    }
}
)NS", 42, diagnostic);
    tests.expect(structuralRuntime,
        "approved structural tuple values participate in identity and body substitution (" +
            diagnostic + ")");

    const auto identityProject = writeProject(root / "identity", "ConstIdentity", R"NS(
class Program
{
    private static int Number<const uint N>() { return (int)N; }
    public static int Main() { return Number<4>() + Number<(2 + 2)>() - 4; }
}
)NS");
    nativesharp::frontend::ProjectCompilation firstIdentityCompilation;
    std::string firstIdentityError;
    const bool firstIdentityBuilt = firstIdentityCompilation.compileProjectGenerics(
        identityProject, firstIdentityError);
    const auto* firstIdentityModel = firstIdentityCompilation.genericModel();
    const auto firstSpecializations = firstIdentityModel != nullptr
        ? firstIdentityModel->specializations()
        : std::span<const nativesharp::frontend::GenericSpecializationRecord>{};
    tests.expect(firstIdentityBuilt && firstIdentityModel != nullptr &&
            firstSpecializations.size() == 1U &&
            firstIdentityModel->instantiationReport().cacheHitCount >= 1U,
        "equivalent const expressions reuse exactly one normalized specialization (" +
            firstIdentityError + ")");

    const bool win64SafeName = firstSpecializations.size() == 1U &&
        !firstSpecializations.front().specializedName.empty() &&
        std::ranges::all_of(firstSpecializations.front().specializedName, [](unsigned char value)
        {
            return std::isalnum(value) != 0 || value == '_';
        });
    tests.expect(win64SafeName,
        "const specialization mangling contains only Win64 symbol-safe characters");

    nativesharp::frontend::ProjectCompilation secondIdentityCompilation;
    std::string secondIdentityError;
    const bool secondIdentityBuilt = secondIdentityCompilation.compileProjectGenerics(
        identityProject, secondIdentityError);
    const auto* secondIdentityModel = secondIdentityCompilation.genericModel();
    const auto secondSpecializations = secondIdentityModel != nullptr
        ? secondIdentityModel->specializations()
        : std::span<const nativesharp::frontend::GenericSpecializationRecord>{};
    tests.expect(secondIdentityBuilt && firstSpecializations.size() == 1U &&
            secondSpecializations.size() == 1U &&
            firstSpecializations.front().specializationKey ==
                secondSpecializations.front().specializationKey &&
            firstSpecializations.front().specializedName ==
                secondSpecializations.front().specializedName,
        "const specialization identity and mangling are deterministic across compilations (" +
            secondIdentityError + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "overflow", "ConstOverflow", R"NS(
class Program
{
    private static int Number<const byte N>() { return (int)N; }
    public static int Main() { return Number<(255 + 1)>(); }
}
)NS", "does not fit parameter type 'byte'", diagnostic),
        "const-expression overflow reports the declared parameter type (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "division-zero", "ConstDivisionZero", R"NS(
class Program
{
    private static int Number<const int N>() { return N; }
    public static int Main() { return Number<(1 / 0)>(); }
}
)NS", "requires a compile-time value argument", diagnostic),
        "undefined const arithmetic is rejected deterministically (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "runtime-call", "ConstRuntimeCall", R"NS(
class Program
{
    private static int Runtime() { return 4; }
    private static int Number<const int N>() { return N; }
    public static int Main() { return Number<(Runtime())>(); }
}
)NS", "requires a compile-time value argument", diagnostic),
        "runtime calls cannot enter const-generic identity (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "wrong-enum", "WrongEnumConst", R"NS(
enum Left { Value = 1 }
enum Right { Value = 1 }
class Program
{
    private static int Number<const Left N>() { return (int)N; }
    public static int Main() { return Number<Right.Value>(); }
}
)NS", "requires a value of enum type 'Left'", diagnostic),
        "equal underlying bits from a different enum do not share identity (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "wrong-structural", "WrongStructuralConst", R"NS(
class Shape<const (int, bool) Key> { }
class Program
{
    public static int Main()
    {
        Shape<(2, 3)> shape = new Shape<(2, 3)>();
        return 0;
    }
}
)NS", "requires structural value type", diagnostic),
        "structural const values require an exact approved shape (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
