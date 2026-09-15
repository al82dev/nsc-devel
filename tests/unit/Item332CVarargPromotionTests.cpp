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

void writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view outputType = {},
    std::string_view extraItems = {})
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name>" +
        (outputType.empty() ? "" : "<OutputType>" + std::string(outputType) + "</OutputType>") +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>" +
        std::string(extraItems) + "</Project>");
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

std::size_t count(std::string_view text, std::string_view needle)
{
    std::size_t result = 0;
    for (std::size_t at = text.find(needle); at != std::string_view::npos;
         at = text.find(needle, at + needle.size()))
    {
        ++result;
    }
    return result;
}

void expectRejected(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view declaration,
    std::string_view setup,
    std::string_view argument,
    std::string_view diagnostic)
{
    const auto projectRoot = root / std::string(name);
    writeProject(projectRoot, name,
        "public class Native { extern C from \"item332.dll\" { public int Probe(int tag, ...); } }\n" +
        std::string(declaration) +
        "public class Program { public static int Main() { " + std::string(setup) +
        " return Native.Probe(0, " + std::string(argument) + "); } }");
    const std::string project = (projectRoot / (std::string(name) + ".nsproj")).string();
    const std::array<std::string_view, 2> arguments = {"--dump-project-oop-hir", project};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports the C-vararg boundary (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item332_c_vararg_promotion_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto direct = root / "direct";
    writeProject(direct, "Item332Direct", R"NS(
public enum Tiny : byte { One = 1 }
public enum Wide : uint { Nine = 9u }
public extern C delegate int Callback(int value);
public class Native
{
    extern C from "item332.dll"
    {
        public int Probe(int tag, ...);
        public int Ordered(int first, int second, ...);
    }
}
public class Program
{
    public static int Identity(int value) { return value; }
    public static int Main()
    {
        Callback callback = Identity;
        int* pointer = null;
        int first = Native.Probe(
            7, (float)1, true, (byte)2, (sbyte)3, (char)4, (short)5, (ushort)6,
            Tiny.One, Wide.Nine, 8, (uint)9, (long)10, (ulong)11, (nint)12, (nuint)13,
            (double)14, pointer, callback);
        int second = Native.Probe(8);
        return first + second + Native.Ordered(second: 2, first: 1, (float)3);
    }
}
)NS");
    const std::string directProject = (direct / "Item332Direct.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 2> hirArguments = {
        "--dump-project-oop-hir", directProject};
    const bool hirAccepted = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hirAccepted, "C variadic calls bind and lower to HIR (" + error + ")");
    tests.expect(hirAccepted && count(output, "conversion type=builtin:int conversion=integral-promotion") == 7U,
        "HIR promotes bool, five small integer types, and a small enum to int");
    tests.expect(hirAccepted &&
                     output.find("conversion type=builtin:uint conversion=integral-promotion") !=
                         std::string::npos &&
                     output.find("static-field-value type=type:Item332Direct::Wide") !=
                         std::string::npos,
        "HIR removes a wide enum's NativeSharp identity in favor of its C-compatible uint");
    tests.expect(hirAccepted && count(output, "conversion type=builtin:double conversion=numeric") == 2U,
        "HIR promotes each float variadic argument to double");
    tests.expect(hirAccepted && output.find("local-value type=builtin:int*") != std::string::npos &&
                     output.find("local-value type=type:Item332Direct::Callback") != std::string::npos,
        "HIR preserves raw pointers and extern C delegates as ABI pointer values");

    for (const std::string_view optimization : {std::string_view("-O0"), std::string_view("-O2")})
    {
        const std::array<std::string_view, 3> llvmArguments = {
            "--dump-project-oop-llvm", directProject, optimization};
        output.clear();
        error.clear();
        const bool llvmAccepted = runDriver(llvmArguments, output, error) == ExitCode::Success;
        tests.expect(llvmAccepted, "LLVM accepts promoted varargs at " + std::string(optimization) +
            " (" + error + ")");
        tests.expect(llvmAccepted && output.find("declare i32 @Probe(i32, ...)") != std::string::npos &&
                         output.find("call i32 (i32, ...) @Probe(i32 7, double ") != std::string::npos,
            "LLVM call uses the variadic prototype and a double first extra at " +
                std::string(optimization));
        tests.expect(llvmAccepted && output.find("@Ordered(i32 1, i32 2, double ") != std::string::npos,
            "named fixed arguments reorder before the positional promoted tail at " +
                std::string(optimization));
    }

    const auto overload = root / "fixed-overload";
    writeProject(overload, "Item332FixedOverload", R"NS(
public class Native
{
    extern C from "item332.dll" { public int Pick(int value, ...); }
    public static int Pick(int value) { return value; }
}
public class Program
{
    public static int Main() { return Native.Pick(42); }
}
)NS");
    const std::string overloadProject =
        (overload / "Item332FixedOverload.nsproj").string();
    const std::array<std::string_view, 3> overloadArguments = {
        "--dump-project-oop-llvm", overloadProject, "-O0"};
    output.clear();
    error.clear();
    const bool overloadAccepted =
        runDriver(overloadArguments, output, error) == ExitCode::Success;
    tests.expect(overloadAccepted &&
                     output.find("call i32 (i32, ...) @Pick") == std::string::npos &&
                     output.find("__nsc_method_Item332FixedOverload$3A$3ANative__Pick") !=
                         std::string::npos,
        "a fixed overload wins over the variadic tail conversion (" + error + ")");

    expectRejected(tests, root, "ManagedString", {}, {}, "\"managed\"",
        "C variadic arguments must be by-value ABI scalars");
    expectRejected(tests, root, "ManagedClass", "public class Box {}\n", "Box box = new Box();",
        "box", "C variadic arguments must be by-value ABI scalars");
    expectRejected(tests, root, "Aggregate", "public struct Pair { public int Value; }\n",
        "Pair pair = { 1 };", "pair", "C variadic arguments must be by-value ABI scalars");
    expectRejected(tests, root, "ManagedDelegate", "public delegate int Callback(int value);\n",
        "Callback callback = null;", "callback", "C variadic arguments must be by-value ABI scalars");
    expectRejected(tests, root, "ByReference", {}, "int value = 1;", "ref value",
        "C variadic arguments must be by-value ABI scalars");

    const auto namedTail = root / "named-tail";
    writeProject(namedTail, "NamedTail", R"NS(
public class Native
{
    extern C from "item332.dll" { public int Probe(int tag, ...); }
}
public class Program
{
    public static int Main() { return Native.Probe(tag: 0, extra: 1); }
}
)NS");
    const std::string namedTailProject = (namedTail / "NamedTail.nsproj").string();
    const std::array<std::string_view, 2> namedTailArguments = {
        "--dump-project-oop-hir", namedTailProject};
    output.clear();
    error.clear();
    tests.expect(runDriver(namedTailArguments, output, error) != ExitCode::Success &&
                     error.find("accepts 2 argument(s)") != std::string::npos,
        "the unnamed ellipsis cannot be targeted by a named argument (" + error + ")");

    const auto producer = root / "producer";
    writeProject(producer, "Item332Producer", R"NS(
public class Native
{
    extern C from "item332.dll" { public int Probe(int tag, ...); }
}
)NS", "StaticLibrary");
    const std::string producerProject = (producer / "Item332Producer.nsproj").string();
    const std::string producerLibrary = (producer / "Item332Producer.lib").string();
    const std::array<std::string_view, 5> emitArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    const bool emitted = runDriver(emitArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "metadata producer for a variadic consumer emits (" + error + ")");

    const auto consumer = root / "consumer";
    const std::string metadata = (producer / "Item332Producer.nsmeta").string();
    writeProject(consumer, "Item332Consumer", R"NS(
public class Program
{
    public static int Main() { return Native.Probe(1, (float)2, (byte)3); }
}
)NS", {}, "<ItemGroup><LibraryReference Include=\"" + producerLibrary +
        "\" Metadata=\"" + metadata + "\" /></ItemGroup>");
    const std::string consumerProject = (consumer / "Item332Consumer.nsproj").string();
    const std::array<std::string_view, 3> consumerArguments = {
        "--dump-project-oop-llvm", consumerProject, "-O0"};
    output.clear();
    error.clear();
    const bool consumerAccepted = emitted &&
        runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerAccepted &&
                     output.find("@Probe(i32 1, double 2.000000e+00, i32 3)") != std::string::npos,
        "LibraryReference calls apply the same default promotions (" + error + ")");

    return tests.resultKeeping(root);
}
