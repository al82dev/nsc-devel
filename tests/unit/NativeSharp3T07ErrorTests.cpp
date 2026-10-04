#include "nativesharp/Language/ErrorSemantics.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;

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
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
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
    if (result != ExitCode::Success)
    {
        diagnostic += "driver exit code " +
            std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}

struct CompilationEvidence
{
    bool succeeded = false;
    std::string error;
    std::string diagnostics;
    std::string mir;
};

CompilationEvidence compileMir(const std::filesystem::path& project)
{
    nativesharp::frontend::ProjectCompilation compilation;
    CompilationEvidence evidence;
    evidence.succeeded = compilation.compileProjectOopMir(project, evidence.error);
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!evidence.diagnostics.empty()) evidence.diagnostics += " | ";
        evidence.diagnostics += diagnostic.message();
    }
    if (evidence.succeeded && compilation.oopMirModel() != nullptr)
        evidence.mir = nativesharp::frontend::ProjectOopMirModelDumper::dump(
            *compilation.oopMirModel());
    return evidence;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    const ResultProtocol result{"type:Packet", "type:IoError"};
    tests.expect(supportsQuestionPropagation(result, error),
        "? accepts a complete static Result protocol without runtime identity");
    ResultProtocol incomplete = result;
    incomplete.hasTakeError = false;
    tests.expect(!supportsQuestionPropagation(incomplete, error),
        "? rejects incomplete protocols before lowering");
    tests.expect(errorConversionIsExplicit("E", "E", false) &&
            errorConversionIsExplicit("E1", "E2", true) &&
            !errorConversionIsExplicit("E1", "E2", false),
        "error propagation never invents an implicit cross-error conversion");

    tests.expect(validateThrowContract({"type:IoError", ThrowAbi::DualReturn, false, false}, error) &&
            !validateThrowContract({"type:IoError", ThrowAbi::DualReturn, true, false}, error),
        "typed throws has a native ABI and requires adapters at C boundaries");
    tests.expect(catchesAllDeclaredErrors(
            std::array<std::string, 2>{"Io", "Parse"},
            std::array<std::string, 2>{"Parse", "Io"}, false) &&
            !catchesAllDeclaredErrors(
                std::array<std::string, 2>{"Io", "Parse"},
                std::array<std::string, 1>{"Io"}, false),
        "typed catch coverage is checked without managed RTTI");
    tests.expect(neverCoercesTo("builtin:int") && neverCoercesTo("type:Packet"),
        "never coerces to every inhabited result type without producing a value");
    const auto neverToInt = nativesharp::frontend::ProjectTypeConversion::classifyImplicit(
        "builtin:never", "builtin:int");
    const auto neverToTuple = nativesharp::frontend::ProjectTypeConversion::classifyImplicit(
        "builtin:never", "tuple:2:11:builtin:int,12:builtin:long");
    tests.expect(
        neverToInt.kind == nativesharp::frontend::ProjectConversionKind::Never &&
            neverToInt.rank == 0 &&
            neverToTuple.kind == nativesharp::frontend::ProjectConversionKind::Never &&
            std::string(nativesharp::frontend::projectConversionKindName(neverToInt.kind)) ==
                "never",
        "the production conversion classifier exposes never as one bottom conversion, not a fabricated structural value");
    tests.expect(unifyLoopBreakTypes(std::array<std::string, 3>{"never", "builtin:int", "builtin:int"}) ==
            "builtin:int" && !unifyLoopBreakTypes(
                std::array<std::string, 2>{"builtin:int", "builtin:long"}).has_value(),
        "loop expressions unify break values while ignoring diverging paths");
    const PanicContract abort = panicContract(PanicProfile::Abort);
    const PanicContract unwind = panicContract(PanicProfile::NativeUnwind);
    tests.expect(!abort.runsCleanup && unwind.runsCleanup && !abort.mayCrossFfi && !unwind.mayCrossFfi,
        "panic profiles define cleanup and forbid implicit unwind across FFI");
    tests.expect(assertionRequiresUnsafe(AssertionKind::Assume) &&
            !assertionRequiresUnsafe(AssertionKind::Assert),
        "assume is unsafe because a false premise may introduce undefined behavior");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t07_result_propagation";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const std::filesystem::path project = writeProject(
        root, "ResultPropagation", R"NS(
public variant Result<T, E> { Ok(T), Error(E) }

public class State
{
    public static int Cleanups = 0;
    public static int Observed = 0;
    public static int Conversions = 0;
}

public struct SourceError
{
    public int Code;
    public SourceError(int code) { Code = code; }
}

public struct PublicError
{
    public int Code;
    public PublicError(int code) { Code = code; }
    public static explicit operator PublicError(SourceError source)
    {
        State.Conversions = State.Conversions + 1;
        return new PublicError(source.Code);
    }
}

public class Program
{
    private static Result<int, SourceError> Source(bool fail, int value)
    {
        if (fail) { return Result<int, SourceError>.Error(new SourceError(value)); }
        return Result<int, SourceError>.Ok(value);
    }

    private static Result<int, PublicError> Forward(bool fail, int value)
    {
        defer [] { State.Cleanups = State.Cleanups + 1; }
        int extracted = Source(fail, value)?;
        State.Observed = extracted + 2;
        return Result<int, PublicError>.Ok(extracted + 2);
    }

    public static int Main()
    {
        Result<int, PublicError> successful = Forward(false, 40);
        if (!successful.IsOk()) { return 10; }
        Result<int, PublicError> failed = Forward(true, 7);
        if (!failed.IsError()) { return 20; }
        int conditional = State.Observed == 42 ? 1 : 0;
        if (conditional != 1 || State.Cleanups != 2 || State.Conversions != 1) { return 31; }
        return State.Observed;
    }
}
)NS");

    nativesharp::frontend::ProjectCompilation compilation;
    std::string compilationError;
    const bool mirBuilt = compilation.compileProjectOopMir(project, compilationError);
    std::string semanticDiagnostics;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!semanticDiagnostics.empty()) semanticDiagnostics += " | ";
        semanticDiagnostics += "@" + std::to_string(diagnostic.location().offset()) + ": ";
        semanticDiagnostics += diagnostic.message();
    }
    const std::string mir = mirBuilt && compilation.oopMirModel() != nullptr
        ? nativesharp::frontend::ProjectOopMirModelDumper::dump(*compilation.oopMirModel())
        : std::string();
    tests.expect(
        mirBuilt && compilationError.empty() &&
            mir.find("schema=1.25") != std::string::npos &&
            mir.find("result-propagation") != std::string::npos &&
            mir.find("conversion=__nsc_method_") != std::string::npos &&
            mir.find("continuation=return") != std::string::npos,
        "postfix '?' publishes its branch, explicit error conversion, and cleanup return in MIR (" +
            compilationError + semanticDiagnostics + mir + ")");

    std::string executionDiagnostic;
    const bool executed = compileAndRun(project, 42, executionDiagnostic);
    tests.expect(executed,
        "O0/NSLink executes Ok continuation and converted Error propagation with defer cleanup (" +
            executionDiagnostic + ")");

    const std::filesystem::path typedRoot = root / "typed";
    const std::filesystem::path typedProject = writeProject(
        typedRoot, "TypedThrows", R"NS(
namespace NativeSharp.Core
{
    public class Exception {}
}

using NativeSharp.Core;

namespace TypedThrows
{
    public class Error : Exception {}
    public class ParseError : Error {}

    public class Guard
    {
        public int Digit;
        public Guard(int digit) { Digit = digit; }
        ~Guard() { Program.Order = Program.Order * 10 + Digit; }
    }

    public delegate int Work(bool fail) throws Error;

    public class Base
    {
        public virtual int Run(bool fail) throws Error
        {
            if (fail) { throw new Error(); }
            return 40;
        }
    }

    public class Derived : Base
    {
        public override int Run(bool fail) throws ParseError
        {
            if (fail) { throw new ParseError(); }
            return 41;
        }
    }

    public class Program
    {
        public static int Cleanups = 0;
        public static int Order = 0;

        private static int Parse(bool fail) throws ParseError
        {
            defer [] { Cleanups = Cleanups + 1; }
            if (fail) { throw new ParseError(); }
            return 40;
        }

        private static int Forward(bool fail) throws Error
        {
            return Parse(fail) + 2;
        }

        private static int Invoke(Work work, bool fail) throws Error
        {
            return work(fail);
        }

        private static int Nested(bool accept)
        {
            try
            {
                try
                {
                    Forward(true);
                }
                catch (Error problem) when (problem != null && accept)
                {
                    return 40;
                }
                catch (Error problem)
                {
                    throw;
                }
                finally
                {
                    Cleanups = Cleanups + 10;
                }
            }
            catch (Error problem)
            {
                return Cleanups;
            }
            return 1;
        }

        private static void Guarded() throws Error
        {
            owned Guard first = new Guard(1);
            owned Guard second = new Guard(2);
            throw new Error();
        }

        public static int Main()
        {
            try
            {
                Work work = Forward;
                if (Forward(false) != 42 || Invoke(work, false) != 42) { return 10; }
            }
            catch (Error problem)
            {
                return 20;
            }
            try
            {
                Forward(true);
            }
            catch (Error problem)
            {
                if (Cleanups != 3) { return 30; }
            }
            Cleanups = 0;
            if (Nested(true) != 40 || Cleanups != 11) { return 31; }
            Cleanups = 0;
            if (Nested(false) != 11 || Cleanups != 11) { return 32; }
            try { Guarded(); }
            catch (Error problem) {}
            if (Order != 21) { return 33; }
            return 42;
        }
    }
}
)NS");
    const CompilationEvidence typed = compileMir(typedProject);
    tests.expect(
        typed.succeeded && typed.error.empty() &&
            typed.mir.find("schema=1.25") != std::string::npos &&
            typed.mir.find("throws=type:TypedThrows::TypedThrows.Error") != std::string::npos &&
            typed.mir.find("throws=type:TypedThrows::TypedThrows.ParseError") != std::string::npos &&
            typed.mir.find("pattern=type:TypedThrows::TypedThrows.Error filter=true exhaustive=false") != std::string::npos &&
            typed.mir.find("pattern=type:TypedThrows::TypedThrows.Error filter=false exhaustive=true") != std::string::npos,
        "throws E and source-ordered filtered catches reach MIR 1.25 with exact error identity (" +
            typed.error + typed.diagnostics + typed.mir + ")");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(typedProject, 42, executionDiagnostic),
        "O0/NSLink executes typed propagation through a delegate and runs defer during native unwind (" +
            executionDiagnostic + ")");
    const std::string typedProjectText = typedProject.string();
    const std::array<std::string_view, 2> llvmArguments = {
        "--dump-project-oop-llvm", typedProjectText};
    std::string llvmOutput;
    std::string llvmError;
    const ExitCode llvmResult = runDriver(llvmArguments, llvmOutput, llvmError);
    const std::size_t nestedName = llvmOutput.find("Nested");
    const std::size_t nestedBegin = nestedName == std::string::npos
        ? std::string::npos
        : llvmOutput.rfind("define ", nestedName);
    const std::size_t nestedEnd = nestedBegin == std::string::npos
        ? std::string::npos
        : llvmOutput.find("\n}", nestedBegin);
    const std::string nestedLlvm = nestedBegin == std::string::npos || nestedEnd == std::string::npos
        ? std::string()
        : llvmOutput.substr(nestedBegin, nestedEnd - nestedBegin);
    tests.expect(
        llvmResult == ExitCode::Success &&
            nestedLlvm.find("eh.catch.test.0") != std::string::npos &&
            nestedLlvm.find("__nativesharp_rtti_is_class") != std::string::npos &&
            nestedLlvm.find("__nativesharp_alloc") == std::string::npos &&
            nestedLlvm.find("clr") == std::string::npos,
        "catch filtering lowers to ordered native RTTI branches without hidden heap or CLR machinery (" +
            llvmError + nestedLlvm + ")");

    const std::filesystem::path neverRoot = root / "never";
    const std::filesystem::path neverProject = writeProject(
        neverRoot, "NeverType", R"NS(
public variant Decision { Value, Stop }

public class Program
{
    private static T AcceptCopy<T>(T value) where T : copy { return value; }

    private static int FromConditional(bool stop)
    {
        return stop ? panic() : 40;
    }

    private static int FromGeneric(bool stop)
    {
        return stop ? AcceptCopy<int>(panic()) : 2;
    }

    private static int FromMatch()
    {
        match (Decision.Value())
        {
            Value => { return 0; },
            Stop => { panic(); }
        }
    }

    private static int NeverLocal()
    {
        var impossible = panic();
        return impossible;
    }

    private static int InfiniteWhile()
    {
        while (true) { panic(); }
    }

    private static int InfiniteFor()
    {
        for (;;) { panic(); }
    }

    public static int Main()
    {
        return FromConditional(false) + FromGeneric(false) +
            FromMatch();
    }
}
)NS");
    const CompilationEvidence neverEvidence = compileMir(neverProject);
    tests.expect(
        neverEvidence.succeeded && neverEvidence.error.empty() &&
            neverEvidence.mir.find("builtin:never") != std::string::npos &&
            neverEvidence.mir.find("panic") != std::string::npos,
        "conditional inference, explicit constrained generics, match arms, inferred locals, and proven infinite loops preserve never in MIR (" +
            neverEvidence.error + neverEvidence.diagnostics + neverEvidence.mir + ")");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(neverProject, 42, executionDiagnostic),
        "O0/NSLink executes the reachable sides of conditional, generic, and match joins with never (" +
            executionDiagnostic + ")");

    const std::string neverProjectText = neverProject.string();
    const std::array<std::string_view, 2> neverLlvmArguments = {
        "--dump-project-oop-llvm", neverProjectText};
    std::string neverLlvmOutput;
    std::string neverLlvmError;
    const ExitCode neverLlvmResult = runDriver(
        neverLlvmArguments, neverLlvmOutput, neverLlvmError);
    const std::size_t neverLocalName = neverLlvmOutput.find("NeverLocal");
    const std::size_t neverLocalBegin = neverLocalName == std::string::npos
        ? std::string::npos
        : neverLlvmOutput.rfind("define ", neverLocalName);
    const std::size_t neverLocalEnd = neverLocalBegin == std::string::npos
        ? std::string::npos
        : neverLlvmOutput.find("\n}", neverLocalBegin);
    const std::string neverLocalLlvm =
        neverLocalBegin == std::string::npos || neverLocalEnd == std::string::npos
        ? std::string()
        : neverLlvmOutput.substr(neverLocalBegin, neverLocalEnd - neverLocalBegin);
    tests.expect(
        neverLlvmResult == ExitCode::Success &&
            neverLocalLlvm.find("__nativesharp_trap") != std::string::npos &&
            neverLocalLlvm.find("unreachable") != std::string::npos &&
            neverLocalLlvm.find("alloca") == std::string::npos &&
            neverLocalLlvm.find("store i1 false") == std::string::npos,
        "an inferred never local has no alloca, layout, store, or fabricated runtime value (" +
            neverLlvmError + neverLocalLlvm + ")");

    const auto expectRejected = [&](std::string_view name, std::string_view source,
                                    std::string_view expectedText)
    {
        const std::filesystem::path projectPath = writeProject(root / std::string(name), name, source);
        const CompilationEvidence evidence = compileMir(projectPath);
        tests.expect(
            !evidence.succeeded &&
                (evidence.error + evidence.diagnostics).find(expectedText) != std::string::npos,
            std::string(name) + " is rejected by the typed throws contract (" +
                evidence.error + evidence.diagnostics + ")");
    };
    expectRejected("UnhandledCall", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Program
{
    private static int Risk() throws Error { throw new Error(); }
    public static int Main() { return Risk(); }
}
)NS", "NS3071");
    expectRejected("UnhandledThrow", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Program { public static int Main() { throw new Error(); } }
)NS", "NS3071");
    expectRejected("DelegateMismatch", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public delegate int Work() throws Error;
public class Program
{
    private static int Plain() { return 42; }
    public static int Main() { Work work = Plain; return work(); }
}
)NS", "method group");
    expectRejected("OverrideWidening", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Narrow : Error {}
public class Base { public virtual int Run() throws Narrow { return 1; } }
public class Derived : Base { public override int Run() throws Error { return 2; } }
public class Program { public static int Main() { return 0; } }
)NS", "outside inherited contract");
    expectRejected("ExternBoundary", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public extern C delegate int NativeWork() throws Error;
public class Program { public static int Main() { return 0; } }
)NS", "cannot declare throws");
    expectRejected("UnrelatedCatchPattern", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Other : NativeSharp.Core.Exception {}
public class Program
{
    private static void Risk() throws Error { throw new Error(); }
    public static int Main()
    {
        try { Risk(); }
        catch (Other problem) {}
        return 42;
    }
}
)NS", "cannot match any declared error from the protected region");
    expectRejected("NonExhaustiveCatch", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Narrow : Error {}
public class Program
{
    private static void Risk() throws Error { throw new Error(); }
    public static int Main()
    {
        try { Risk(); }
        catch (Narrow problem) {}
        return 42;
    }
}
)NS", "NS3071");
    expectRejected("CatchFilterType", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Program
{
    private static void Risk() throws Error { throw new Error(); }
    public static int Main() throws Error
    {
        try { Risk(); }
        catch (Error problem) when (1) { return 1; }
        catch (Error problem) { return 42; }
    }
}
)NS", "catch filter expression must have type bool");
    expectRejected("CatchFilterFalse", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Program
{
    private static void Risk() throws Error { throw new Error(); }
    public static int Main()
    {
        try { Risk(); }
        catch (Error problem) when (false) { return 1; }
        catch (Error problem) { return 42; }
    }
}
)NS", "catch filter is unreachable because its condition is always false");
    expectRejected("ReturnFromFinally", R"NS(
public class Program
{
    public static int Main()
    {
        try { return 1; }
        finally { return 42; }
    }
}
)NS", "return cannot leave a finally block");
    expectRejected("NeverOverloadAmbiguity", R"NS(
public class Program
{
    private class Left {}
    private class Right {}
    private static int Pick(Left value) { return 1; }
    private static int Pick(Right value) { return 2; }
    public static int Main() { return Pick(panic()); }
}
)NS", "ambiguous");
    expectRejected("NeverDoesNotHideDeadErrors", R"NS(
public class Program
{
    public static int Main()
    {
        int value;
        panic();
        return value;
    }
}
)NS", "use of unassigned local variable 'value'");

    const std::filesystem::path edition2 = root / "edition2";
    const std::filesystem::path edition2Project = edition2 / "Edition2Identifier.nsproj";
    writeFile(edition2Project,
        "<Project><PropertyGroup><Name>Edition2Identifier</Name><Language>2</Language>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(edition2 / "Main.ns", R"NS(
public class Program
{
    private static int throws() { return 42; }
    public static int Main() { return throws(); }
}
)NS");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(edition2Project, 42, executionDiagnostic),
        "throws remains a contextual keyword and preserves edition-2 identifiers (" +
            executionDiagnostic + ")");

    const std::filesystem::path producer = root / "producer";
    const std::filesystem::path producerProject = producer / "ThrowsApi.nsproj";
    const std::filesystem::path producerLibrary = producer / "ThrowsApi.lib";
    writeFile(producerProject,
        "<Project><PropertyGroup><Name>ThrowsApi</Name><Language>3</Language>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace NativeSharp.Core { public class Exception {} }
namespace ThrowsApi
{
    public class Error : NativeSharp.Core.Exception {}
    public delegate int Operation(bool fail) throws Error;
    public int ExecuteFree(bool fail) throws Error
    {
        if (fail) { throw new Error(); }
        return 42;
    }
    public class Api
    {
        public static int Execute(bool fail) throws Error
        {
            if (fail) { throw new Error(); }
            return 42;
        }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string producerLibraryText = producerLibrary.string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", producerLibraryText};
    std::string producerOutput;
    std::string producerError;
    const ExitCode producerResult = runDriver(producerArguments, producerOutput, producerError);
    nativesharp::interop::LibraryMetadata metadata;
    std::string metadataError;
    const bool metadataRead = producerResult == ExitCode::Success &&
        nativesharp::interop::LibraryMetadataCodec{}.read(
            producer / "ThrowsApi.nsmeta", metadata, metadataError);
    const auto delegate = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName == "ThrowsApi.Operation";
    });
    const auto method = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "Execute";
    });
    const auto freeFunction = std::ranges::find_if(
        metadata.namespaceFunctions, [](const auto& function)
        {
            return function.name == "ExecuteFree";
        });
    tests.expect(
        metadataRead && metadata.formatMinor == 37 && delegate != metadata.types.end() &&
            method != metadata.members.end() && freeFunction != metadata.namespaceFunctions.end() &&
            delegate->delegateErrorTypeKey == method->declaredErrorTypeKey &&
            delegate->delegateErrorTypeKey == freeFunction->declaredErrorTypeKey &&
            delegate->delegateErrorTypeKey.ends_with("::ThrowsApi.Error"),
        "NSMETA 1.37 round-trips typed throws for delegates, methods, and free functions (" +
            metadataError + producerError + producerOutput + ")");
    if (metadataRead)
    {
        nativesharp::interop::LibraryMetadata invalidDowngrade = metadata;
        invalidDowngrade.formatMinor = 36;
        std::string downgradeError;
        const bool downgraded = nativesharp::interop::LibraryMetadataCodec{}.writeModel(
            invalidDowngrade, producer / "invalid-1.36.nsmeta", downgradeError);
        tests.expect(
            !downgraded && downgradeError.find("cannot represent typed throws") != std::string::npos,
            "NSMETA refuses to erase typed throws while downgrading below 1.37 (" +
                downgradeError + ")");
    }
    else
    {
        tests.expect(false, "NSMETA downgrade guard requires readable producer metadata");
    }

    const std::filesystem::path consumer = root / "consumer";
    const std::filesystem::path consumerProject = consumer / "ThrowsConsumer.nsproj";
    writeFile(consumerProject,
        "<Project><PropertyGroup><Name>ThrowsConsumer</Name><Language>3</Language>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /><LibraryReference Include=\"" +
        producerLibrary.generic_string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using ThrowsApi;
public class Program
{
    public static int Main()
    {
        try
        {
            if (Api.Execute(false) != 42) { return 2; }
            ExecuteFree(true);
        }
        catch (Error problem) { return 42; }
        return 1;
    }
}
)NS");
    executionDiagnostic.clear();
    tests.expect(
        producerResult == ExitCode::Success &&
            compileAndRun(consumerProject, 42, executionDiagnostic),
        "a metadata consumer reconstructs throws E and catches the producer's native exception (" +
            producerError + producerOutput + executionDiagnostic + ")");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
