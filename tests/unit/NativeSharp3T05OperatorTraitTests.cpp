#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ClassMemberSymbol;
using nativesharp::frontend::InliningPolicy;
using nativesharp::frontend::ProjectCompilation;
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
    std::string_view outputType = "Exe",
    std::string_view extraItems = {})
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(
        project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>" + std::string(outputType) + "</OutputType>" +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />" +
        std::string(extraItems) +
        "</ItemGroup></Project>");
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

bool compileAndRun(
    const std::filesystem::path& project,
    int expected,
    std::string& diagnostic)
{
    const std::filesystem::path executable = project.parent_path() /
        (project.stem().string() + ".exe");
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success) return false;
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}

bool diagnosticContains(const ProjectCompilation& compilation, std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        if (diagnostic.message().find(text) != std::string::npos) return true;
    return false;
}

std::string diagnosticsText(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

bool containsCallTo(std::string_view llvmIr, std::string_view calleeFragment)
{
    std::istringstream lines{std::string(llvmIr)};
    std::string line;
    while (std::getline(lines, line))
        if (line.find("call ") != std::string::npos &&
            line.find(calleeFragment) != std::string::npos)
            return true;
    return false;
}

std::optional<std::size_t> findType(const ProjectCompilation& compilation, std::string_view suffix)
{
    if (compilation.nameLookup() == nullptr) return std::nullopt;
    for (std::size_t ordinal = 0; ordinal < compilation.nameLookup()->namedTypes().size(); ++ordinal)
        if (compilation.nameLookup()->namedTypes()[ordinal].identity.key().ends_with(suffix))
            return ordinal;
    return std::nullopt;
}
}

int main()
{
    using namespace nativesharp::language;
    using nativesharp::llvm_backend::LlvmOptimizationLevel;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_operator_traits";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const auto contracts = operatorTraitContracts();
    std::unordered_set<std::string> stableIdentities;
    for (const OperatorTraitContract& contract : contracts)
        stableIdentities.insert(std::string(contract.langItem));
    tests.expect(contracts.size() == 20 && stableIdentities.size() == contracts.size(),
        "every overloadable unary/binary operator has one stable lang-item identity");
    tests.expect(operatorTrait("+", 1) == "lang:UnaryPlus" &&
            operatorTrait("+", 2) == "lang:Add" &&
            operatorMethodName("+", 2) == "op_Addition" &&
            operatorTraitContractForMethod("op_Addition", 2) != nullptr,
        "source spelling, arity, lang-item and ABI method name form one reversible contract");
    tests.expect(!operatorCanBeOverloaded("&&") && !operatorCanBeOverloaded("||") &&
            !operatorCanBeOverloaded("??") && operatorCanBeOverloaded("+") &&
            !operatorCanBeOverloaded("made-up"),
        "short-circuit and unknown operators cannot become eager overload calls");

    const std::string positiveSource = R"NS(
public trait IAdd<T>
{
    public static T operator +(in T left, in T right);
}
public struct Number : IAdd<Number>
{
    public int value;
    public Number(int source) { value = source; }
    public static forceinline Number operator +(in Number left, in Number right)
    {
        return new Number(left.value + right.value);
    }
}
public class Program
{
    public static int Main()
    {
        Number left = new Number(20);
        Number right = new Number(22);
        Number sum = left + right;
        return sum.value;
    }
}
)NS";
    const std::filesystem::path positive =
        writeProject(root / "positive", "TraitOperator", positiveSource);
    ProjectCompilation compilation;
    std::string compilationError;
    const bool compiled = compilation.compileProjectOopMir(positive, compilationError);
    tests.expect(compiled && compilationError.empty() && !compilation.diagnostics().hasErrors(),
        "generic trait operator with implicit in-reference operands reaches physical OOP MIR (" +
            compilationError + diagnosticsText(compilation) + ")");
    if (compiled && compilation.memberModel() != nullptr &&
        compilation.interfaceModel() != nullptr && compilation.methodBodyModel() != nullptr)
    {
        const std::optional<std::size_t> number = findType(compilation, "Number");
        tests.expect(number.has_value(), "operator fixture resolves its concrete Number type");
        if (number.has_value())
        {
            const auto witnesses = compilation.interfaceModel()->operatorWitnessesOfType(*number);
            tests.expect(witnesses.size() == 1 && witnesses.front().langItem == "lang:Add",
                "interface coherence records the concrete lang:Add witness");
        }

        std::optional<std::size_t> selectedOperator;
        for (const auto& call : compilation.methodBodyModel()->calls())
            if (call.selectedMemberOrdinal.has_value() &&
                *call.selectedMemberOrdinal < compilation.memberModel()->members().size() &&
                compilation.memberModel()->members()[*call.selectedMemberOrdinal].isUserOperator)
                selectedOperator = call.selectedMemberOrdinal;
        tests.expect(selectedOperator.has_value() &&
                compilation.memberModel()->members()[*selectedOperator].operatorTraitKey == "lang:Add",
            "operator expression binds the trait witness by stable lang-item identity");
        tests.expect(selectedOperator.has_value() &&
                compilation.memberModel()->members()[*selectedOperator].inliningPolicy ==
                    InliningPolicy::Required,
            "trait operator preserves its required inline policy");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty() &&
                module->text().find("alwaysinline") != std::string::npos,
            "trait operator reaches LLVM as an ordinary alwaysinline static function");
        if (module != nullptr)
        {
            std::string targetError;
            auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
                LlvmOptimizationLevel::O0, targetError);
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            std::string optimizationError;
            const bool optimized = target != nullptr && optimizer.optimize(
                *module, *target, LlvmOptimizationLevel::O0, optimizationError);
            tests.expect(optimized, "O0 mandatory-inlining pipeline accepts the trait witness");
            tests.expect(optimized && !containsCallTo(module->text(), "op_Addition"),
                "forceinline trait operator leaves no runtime call to its witness");
        }
    }

    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "O0/NSLink executes the in-reference trait operator and returns 42 (" + diagnostic + ")");

    diagnostic.clear();
    const std::filesystem::path builtins = writeProject(root / "builtins", "BuiltinOperators", R"NS(
public class Program
{
    public const int Folded = 40 + 2;
    public static int Main()
    {
        byte left = 20;
        short right = 22;
        int promoted = left + right;
        return Folded + promoted - 42;
    }
}
)NS");
    tests.expect(compileAndRun(builtins, 42, diagnostic),
        "built-in numeric promotion and constant evaluation remain intrinsic (" + diagnostic + ")");

    const std::filesystem::path producerDirectory = root / "cross_module" / "Producer";
    const std::filesystem::path producer = writeProject(
        producerDirectory, "OperatorApi", R"NS(
namespace OperatorApi
{
    public trait IAdd<T> { public static T operator +(in T left, in T right); }
    public struct Number : IAdd<Number>
    {
        public int Value;
        public Number(int value) { Value = value; }
        public static Number operator +(in Number left, in Number right)
        { return new Number(left.Value + right.Value); }
    }
}
)NS", "StaticLibrary");
    const std::string producerProjectText = producer.string();
    const std::string libraryText = (producerDirectory / "OperatorApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string producerOutput;
    std::string producerError;
    const ExitCode producerResult = runDriver(producerArguments, producerOutput, producerError);
    tests.expect(producerResult == ExitCode::Success,
        "a public operator-trait witness is emitted with NSMETA 1.32 (" + producerError +
            producerOutput + ")");

    const std::filesystem::path consumer = writeProject(
        root / "cross_module" / "Consumer", "OperatorConsumer", R"NS(
using OperatorApi;
public class Program
{
    public static int Main()
    {
        Number left = new Number(20);
        Number right = new Number(22);
        Number sum = left + right;
        return sum.Value;
    }
}
)NS", "Exe", "<LibraryReference Include=\"../Producer/OperatorApi.lib\" />");
    diagnostic.clear();
    tests.expect(producerResult == ExitCode::Success && compileAndRun(consumer, 42, diagnostic),
        "the stable lang:Add witness is reconstructed and executed across a library boundary (" +
            diagnostic + ")");

    const std::filesystem::path ambiguity = writeProject(root / "ambiguity", "AmbiguousOperators", R"NS(
public trait ILeftAdd { public static Left operator +(Left left, Right right); }
public trait IRightAdd { public static Left operator +(Left left, Right right); }
public class Left : ILeftAdd
{
    public static Left operator +(Left left, Right right) { return left; }
}
public class Right : IRightAdd
{
    public static Left operator +(Left left, Right right) { return left; }
}
public class Program
{
    public static int Main()
    {
        Left left = new Left();
        Right right = new Right();
        Left result = left + right;
        return 0;
    }
}
)NS");
    ProjectCompilation ambiguous;
    std::string ambiguityError;
    tests.expect(!ambiguous.compileProjectMethodBodies(ambiguity, ambiguityError) &&
            diagnosticContains(ambiguous, "ambiguous Stage 38 call to 'op_Addition'"),
        "equally ranked trait witnesses produce an explicit deterministic ambiguity");

    const std::filesystem::path temporary = writeProject(root / "temporary", "ReferenceTemporary", R"NS(
public trait IAddInt<T> { public static T operator +(in T left, in int right); }
public struct Number : IAddInt<Number>
{
    public int value;
    public Number(int source) { value = source; }
    public static Number operator +(in Number left, in int right)
    { return new Number(left.value + right); }
}
public class Program
{
    public static int Main()
    {
        Number left = new Number(20);
        Number sum = left + 22;
        return sum.value;
    }
}
)NS");
    ProjectCompilation temporaryCompilation;
    std::string temporaryError;
    const bool temporaryCompiled =
        temporaryCompilation.compileProjectMethodBodies(temporary, temporaryError);
    tests.expect(!temporaryCompiled &&
            diagnosticContains(temporaryCompilation, "no viable Stage 38 overload"),
        "implicit operator references require real addressable operands and never hide a copy "
        "(compiled=" + std::string(temporaryCompiled ? "true" : "false") + "; " +
            temporaryError + diagnosticsText(temporaryCompilation) + ")");

    const std::array<std::pair<std::string_view, std::string_view>, 3> shortCircuitCases = {{
        {"And", "&&"},
        {"Or", "||"},
        {"Coalesce", "??"},
    }};
    for (const auto& [name, spelling] : shortCircuitCases)
    {
        const std::filesystem::path project = writeProject(
            root / ("short_" + std::string(name)),
            "ShortCircuit" + std::string(name),
            "public trait Bad { public static bool operator " + std::string(spelling) +
                "(bool left, bool right); } public class Program { public static int Main() { return 0; } }");
        ProjectCompilation invalid;
        std::string invalidError;
        tests.expect(!invalid.compileProjectInterfaces(project, invalidError) &&
                diagnosticContains(invalid, "is intrinsic and cannot be overloaded"),
            std::string(spelling) + " remains a non-overloadable short-circuit intrinsic");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
