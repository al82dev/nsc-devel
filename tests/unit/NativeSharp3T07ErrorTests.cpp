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
    std::string_view source,
    std::string_view panicProfile = {},
    std::string_view panicHandler = {})
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    const std::string panicProperties =
        (panicProfile.empty() ? std::string{} :
            "<PanicProfile>" + std::string(panicProfile) + "</PanicProfile>") +
        (panicHandler.empty() ? std::string{} :
            "<PanicHandler>" + std::string(panicHandler) + "</PanicHandler>");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Exe</OutputType>"
        + panicProperties +
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
    const std::string developmentRuntime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", developmentRuntime.c_str());
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
            mir.find("schema=1.30") != std::string::npos &&
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
            typed.mir.find("schema=1.30") != std::string::npos &&
            typed.mir.find("throws=type:TypedThrows::TypedThrows.Error") != std::string::npos &&
            typed.mir.find("throws=type:TypedThrows::TypedThrows.ParseError") != std::string::npos &&
            typed.mir.find("pattern=type:TypedThrows::TypedThrows.Error filter=true exhaustive=false") != std::string::npos &&
            typed.mir.find("pattern=type:TypedThrows::TypedThrows.Error filter=false") != std::string::npos,
        "throws E and source-ordered filtered catches retain exact error identity in MIR (" +
            typed.error + typed.diagnostics + typed.mir + ")");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(typedProject, 42, executionDiagnostic),
        "O0/NSLink executes typed propagation through a delegate and runs defer during native unwind (" +
            executionDiagnostic + ")");

    const std::filesystem::path accessorProject = writeProject(
        root / "accessors", "AccessorThrows", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Accessors
{
    private int stored;

    public int Value
    {
        get throws Error
        {
            if (stored < 0) { throw new Error(); }
            return stored;
        }
        set throws Error
        {
            if (value < 0) { throw new Error(); }
            stored = value;
        }
    }

    public int this[int index]
    {
        get throws Error
        {
            if (index < 0) { throw new Error(); }
            return stored + index;
        }
        set throws Error
        {
            if (index < 0) { throw new Error(); }
            stored = value + index;
        }
    }
}
public class Program
{
    public static int Main()
    {
        owned Accessors values = new Accessors();
        try
        {
            try { values.Value = -1; }
            catch (Error problem) {}
            values.Value = 40;
            try { values[-1] = 1; }
            catch (Error problem) {}
            if (values.Value != 40 || values[2] != 42) { return 1; }
            return 42;
        }
        catch (Error problem) { return 2; }
    }
}
)NS");
    const CompilationEvidence accessorEvidence = compileMir(accessorProject);
    tests.expect(
        accessorEvidence.succeeded && accessorEvidence.error.empty() &&
            accessorEvidence.mir.find("throws=type:AccessorThrows::Error") != std::string::npos,
        "property and indexer accessors retain typed throws in MIR (" +
            accessorEvidence.error + accessorEvidence.diagnostics + accessorEvidence.mir + ")");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(accessorProject, 42, executionDiagnostic),
        "O0/NSLink executes checked property and indexer get/set accessors (" +
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

    const std::filesystem::path loopRoot = root / "loop-expression";
    const std::filesystem::path loopProject = writeProject(
        loopRoot, "LoopExpression", R"NS(
public class State
{
    public static int Cleanups = 0;
    public static int Drops = 0;
}

public class Resource
{
    ~Resource() { State.Drops = State.Drops + 1; }
}

public class Program
{
    private static int Simple()
    {
        return loop { break 42; };
    }

    private static int Continued()
    {
        int index = 0;
        return loop retry
        {
            index = index + 1;
            if (index < 3) { continue retry; }
            break 40 + index - 1;
        };
    }

    private static int Labelled()
    {
        return loop outer
        {
            int unreachable = loop inner
            {
                break outer: 42;
            };
        };
    }

    private static int Cleaned()
    {
        int value = loop
        {
            defer [] { State.Cleanups = State.Cleanups + 1; }
            break 42;
        };
        return value;
    }

    private static int Moved()
    {
        {
            owned Resource result = loop
            {
                owned Resource source = new Resource();
                break move(source);
            };
        }
        return State.Drops;
    }

    public static int Main()
    {
        if (Simple() != 42 || Continued() != 42 || Labelled() != 42) { return 10; }
        if (Cleaned() != 42 || State.Cleanups != 1) { return 20; }
        if (Moved() != 1) { return 30; }
        return 42;
    }
}
)NS");
    const CompilationEvidence loopEvidence = compileMir(loopProject);
    tests.expect(
        loopEvidence.succeeded && loopEvidence.error.empty() &&
            loopEvidence.mir.find("schema=1.30") != std::string::npos &&
            loopEvidence.mir.find("loop-expression type=builtin:int") != std::string::npos &&
            loopEvidence.mir.find("label=outer break-result=builtin:int") != std::string::npos &&
            loopEvidence.mir.find("target-label=outer") != std::string::npos &&
            loopEvidence.mir.find("target-label=retry") != std::string::npos &&
            loopEvidence.mir.find("continuation=break") != std::string::npos,
        "loop expressions publish typed values, labelled exits, and cleanup edges in MIR 1.29 (" +
            loopEvidence.error + loopEvidence.diagnostics + loopEvidence.mir + ")");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(loopProject, 42, executionDiagnostic),
        "O0/NSLink executes loop values, labelled break/continue, never inner loops, and defer cleanup (" +
            executionDiagnostic + ")");

    const auto expectRejected = [&](std::string_view name, std::string_view source,
                                    std::string_view expectedText)
    {
        const std::filesystem::path projectPath = writeProject(root / std::string(name), name, source);
        const CompilationEvidence evidence = compileMir(projectPath);
        tests.expect(
            !evidence.succeeded &&
                (evidence.error + evidence.diagnostics).find(expectedText) != std::string::npos,
            std::string(name) + " is rejected by the T07 contract (" +
                evidence.error + evidence.diagnostics + ")");
    };
    const auto expectAccepted = [&](std::string_view name, std::string_view source)
    {
        const std::filesystem::path projectPath = writeProject(root / std::string(name), name, source);
        const CompilationEvidence evidence = compileMir(projectPath);
        tests.expect(
            evidence.succeeded,
            std::string(name) + " compiles without forcing typed-exception handling (" +
                evidence.error + evidence.diagnostics + ")");
    };
    // EN: Freeze 3.0 C-13: a call or an accessor access that reaches `throws E` must be caught or
    //     propagated; a written `throw` stays unchecked, as UnhandledThrow shows.
    // ES: Congelamiento 3.0 C-13: una llamada o un acceso a un accesor que llega a `throws E` debe
    //     atraparse o propagarse; un `throw` escrito sigue sin chequeo, como muestra UnhandledThrow.
    expectRejected("UnhandledCall", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Program
{
    private static int Risk() throws Error { throw new Error(); }
    public static int Main() { return Risk(); }
}
)NS", "which is neither caught here nor declared");
    expectAccepted("UnhandledThrow", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Program { public static int Main() { throw new Error(); } }
)NS");
    expectRejected("UnhandledPropertyGet", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Source
{
    public int Value { get throws Error { throw new Error(); } }
}
public class Program
{
    public static int Main() { owned Source source = new Source(); return source.Value; }
}
)NS", "which is neither caught here nor declared");
    expectRejected("UnhandledIndexerSet", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public class Source
{
    public int this[int index]
    {
        get { return 0; }
        set throws Error { throw new Error(); }
    }
}
public class Program
{
    public static int Main() { owned Source source = new Source(); source[0] = 1; return 42; }
}
)NS", "which is neither caught here nor declared");
    expectRejected("DelegateMismatch", R"NS(
namespace NativeSharp.Core { public class Exception {} }
public class Error : NativeSharp.Core.Exception {}
public delegate int Work() throws Error;
public class Program
{
    private static int Plain() { return 42; }
    public static int Main() throws Error { Work work = Plain; return work(); }
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
)NS", "which is neither caught here nor declared");
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
)NS", "which is neither caught here nor declared");
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
    expectRejected("BreakValueOutsideLoopExpression", R"NS(
public class Program
{
    public static int Main() { while (true) { break 42; } return 0; }
}
)NS", "break with a value may target only a loop expression");
    expectRejected("UnknownLoopLabel", R"NS(
public class Program
{
    public static int Main() { return loop { break missing: 42; }; }
}
)NS", "targets unknown loop label 'missing'");
    expectRejected("MixedLoopBreakTypes", R"NS(
public class Program
{
    public static int Main()
    {
        return loop { if (true) { break true; } break 42; };
    }
}
)NS", "loop expression break values have no common type");
    expectRejected("DuplicateLoopLabel", R"NS(
public class Program
{
    public static int Main()
    {
        return loop same { int value = loop same { break 1; }; break value; };
    }
}
)NS", "nested loop label 'same' is already active");
    expectRejected("LoopBorrowEscape", R"NS(
public class Resource {}
public class Program
{
    public static int Main()
    {
        borrowed Resource escaped = loop
        {
            owned Resource owner = new Resource();
            borrowed Resource view = owner;
            break view;
        };
        return escaped == null ? 1 : 42;
    }
}
)NS", "whose lifetime ends when the loop expression exits");

    const std::filesystem::path contextualIdentifiers = root / "contextual-identifiers";
    const std::filesystem::path contextualProject =
        contextualIdentifiers / "ContextualIdentifiers.nsproj";
    writeFile(contextualProject,
        "<Project><PropertyGroup><Name>ContextualIdentifiers</Name>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(contextualIdentifiers / "Main.ns", R"NS(
public class Program
{
    private static int throws() { return 42; }
    private static int match(int value) { return value; }
    public static int Main() { return match(throws()); }
}
)NS");
    executionDiagnostic.clear();
    tests.expect(
        compileAndRun(contextualProject, 42, executionDiagnostic),
        "throws and match remain contextual identifiers without an edition gate (" +
            executionDiagnostic + ")");

    const std::filesystem::path producer = root / "producer";
    const std::filesystem::path producerProject = producer / "ThrowsApi.nsproj";
    const std::filesystem::path producerLibrary = producer / "ThrowsApi.lib";
    writeFile(producerProject,
        "<Project><PropertyGroup><Name>ThrowsApi</Name>"
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
    public class AccessorApi
    {
        public int Value
        {
            get throws Error { return 42; }
            set throws Error { }
        }
        public int this[int index]
        {
            get throws Error { return index; }
            set throws Error { }
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
    const auto accessor = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "get_Item";
    });
    const auto freeFunction = std::ranges::find_if(
        metadata.namespaceFunctions, [](const auto& function)
        {
            return function.name == "ExecuteFree";
        });
    tests.expect(
        metadataRead &&
            metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            delegate != metadata.types.end() &&
            method != metadata.members.end() && accessor != metadata.members.end() &&
            freeFunction != metadata.namespaceFunctions.end() &&
            delegate->delegateErrorTypeKey == method->declaredErrorTypeKey &&
            delegate->delegateErrorTypeKey == accessor->declaredErrorTypeKey &&
            delegate->delegateErrorTypeKey == freeFunction->declaredErrorTypeKey &&
            delegate->delegateErrorTypeKey.ends_with("::ThrowsApi.Error"),
        "current NSMETA round-trips typed throws for delegates, methods, accessors, and free functions (" +
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
        "<Project><PropertyGroup><Name>ThrowsConsumer</Name>"
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
            owned AccessorApi accessors = new AccessorApi();
            accessors.Value = 42;
            if (accessors.Value != 42 || accessors[42] != 42) { return 3; }
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

    const std::filesystem::path assertionsRoot = root / "assertions";
    const std::filesystem::path assertionsProject = writeProject(
        assertionsRoot, "Assertions", R"NS(
public class Program
{
    private static int Impossible()
    {
        unreachable();
    }

    public static int Main()
    {
        assert(true);
        assume(true);
        return 42;
    }
}
)NS");
    const CompilationEvidence assertionsEvidence = compileMir(assertionsProject);
    tests.expect(
        assertionsEvidence.succeeded && assertionsEvidence.error.empty() &&
            assertionsEvidence.mir.find("assert type=builtin:void") != std::string::npos &&
            assertionsEvidence.mir.find("assume type=builtin:void") != std::string::npos &&
            assertionsEvidence.mir.find("unreachable type=builtin:never") != std::string::npos,
        "assert, assume, and unreachable are explicit productive MIR operations (" +
            assertionsEvidence.error + assertionsEvidence.diagnostics + assertionsEvidence.mir + ")");
    std::string assertionExecutionDiagnostic;
    tests.expect(
        compileAndRun(assertionsProject, 42, assertionExecutionDiagnostic),
        "assert(true) and assume(true) preserve O0/NSLink execution (" +
            assertionExecutionDiagnostic + ")");

    const std::filesystem::path falseAssertProject = writeProject(
        root / "assert-false", "AssertFalse", R"NS(
public class Program
{
    public static int Main()
    {
        assert(false);
        return 0;
    }
}
)NS");
    std::string falseAssertDiagnostic;
    tests.expect(
        compileAndRun(falseAssertProject, 143, falseAssertDiagnostic),
        "assert(false) terminates through the dedicated assertion trap reason (" +
            falseAssertDiagnostic + ")");

    const std::filesystem::path unwindProject = writeProject(
        root / "panic-unwind", "PanicUnwind", R"NS(
using NativeSharp.Core;
namespace NativeSharp.Core
{
    public class Environment
    {
        public static void Exit(int exitCode) { }
    }
}
public class Program
{
    private static void Trigger()
    {
        defer [] { Environment.Exit(42); }
        panic("cleanup");
    }
    public static int Main() { Trigger(); return 1; }
}
)NS", "unwind-native");
    const CompilationEvidence unwindEvidence = compileMir(unwindProject);
    tests.expect(
        unwindEvidence.succeeded && unwindEvidence.error.empty() &&
            unwindEvidence.mir.find("panic-profile=unwind-native") != std::string::npos &&
            unwindEvidence.mir.find("nonlocal-exit kind=panic") != std::string::npos &&
            unwindEvidence.mir.find("cleanup-path=#") != std::string::npos,
        "unwind-native publishes panic and its cleanup path in MIR (" +
            unwindEvidence.error + unwindEvidence.diagnostics + unwindEvidence.mir + ")");
    std::string unwindDiagnostic;
    tests.expect(
        compileAndRun(unwindProject, 42, unwindDiagnostic),
        "unwind-native executes defer cleanup before the top-level panic boundary (" +
            unwindDiagnostic + ")");

    const std::filesystem::path handlerProject = writeProject(
        root / "panic-handler", "PanicHandler", R"NS(
using NativeSharp.Core;
namespace NativeSharp.Core
{
    public class Environment
    {
        public static void Exit(int exitCode) { }
    }
}
namespace Hooks
{
    public void OnPanic(string message, string file, int line, int column)
    {
        if (message == "handled" && file != null && line > 0 && column > 0)
        {
            Environment.Exit(42);
        }
        Environment.Exit(41);
    }
}
public class Program
{
    public static int Main() { panic("handled"); }
}
)NS", "abort", "Hooks.OnPanic");
    const CompilationEvidence handlerEvidence = compileMir(handlerProject);
    tests.expect(
        handlerEvidence.succeeded && handlerEvidence.error.empty() &&
            handlerEvidence.mir.find("panic-handler=Hooks.OnPanic") != std::string::npos,
        "a project resolves and publishes its typed custom panic handler (" +
            handlerEvidence.error + handlerEvidence.diagnostics + handlerEvidence.mir + ")");
    std::string handlerDiagnostic;
    const bool handlerRan = compileAndRun(handlerProject, 42, handlerDiagnostic);
    tests.expect(
        handlerRan,
        "the custom panic handler receives payload and source location before abort (" +
            handlerDiagnostic + ")");

    const std::filesystem::path assumeProject = writeProject(
        root / "AssumeWithoutPermission", "AssumeWithoutPermission", R"NS(
public class Program
{
    public static int Main() { assume(true); return 0; }
}
)NS");
    const CompilationEvidence assumeEvidence = compileMir(assumeProject);
    tests.expect(assumeEvidence.succeeded && assumeEvidence.error.empty(),
        "assume is available without an unsafe permission wrapper (" +
            assumeEvidence.error + assumeEvidence.diagnostics + ")");
    expectRejected("AssertionConditionMustBeBool", R"NS(
public class Program
{
    public static int Main() { assert(42); return 0; }
}
)NS", "assert() condition must have type bool");
    expectRejected("UnreachableHasNoArguments", R"NS(
public class Program
{
    public static int Main() { unreachable(true); }
}
)NS", "unreachable() requires exactly 0 value arguments");
    expectRejected("PanicPayloadMustBeString", R"NS(
public class Program
{
    public static int Main() { panic(42); }
}
)NS", "panic() payload must have type string");

    const std::filesystem::path invalidProfileProject = writeProject(
        root / "invalid-panic-profile", "InvalidPanicProfile",
        "public class Program { public static int Main() { return 0; } }",
        "managed-unwind");
    const CompilationEvidence invalidProfile = compileMir(invalidProfileProject);
    tests.expect(
        !invalidProfile.succeeded &&
            invalidProfile.error.find("PanicProfile must be 'abort' or 'unwind-native'") !=
                std::string::npos,
        "project loading rejects unknown panic profiles (" + invalidProfile.error + ")");

    const std::filesystem::path invalidHandlerProject = writeProject(
        root / "invalid-panic-handler", "InvalidPanicHandler", R"NS(
namespace Hooks { public int Wrong(string message) { return 0; } }
public class Program { public static int Main() { panic(); } }
)NS", "abort", "Hooks.Wrong");
    const CompilationEvidence invalidHandler = compileMir(invalidHandlerProject);
    tests.expect(
        !invalidHandler.succeeded &&
            (invalidHandler.error + invalidHandler.diagnostics).find(
                "must be a non-throwing free function with signature") != std::string::npos,
        "project compilation rejects an invalid custom panic-handler signature (" +
            invalidHandler.error + invalidHandler.diagnostics + ")");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
