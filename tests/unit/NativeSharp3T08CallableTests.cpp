#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
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
    std::string_view outputType = "Exe")
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>" + std::string(outputType) +
        "</OutputType>"
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
        diagnostic += "driver exit code " + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path directedRoot =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t08-f01-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(directedRoot, cleanupError);
    const std::string developmentRuntime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", developmentRuntime.c_str());
    using namespace nativesharp::language;
    FunctionType native;
    native.parameters = {"builtin:int"};
    native.result = "builtin:int";
    native.carrier = FunctionCarrier::Pointer;
    native.callingConvention = CallingConvention::NativeSharp;
    FunctionType c = native;
    c.callingConvention = CallingConvention::C;
    tests.expect(functionTypeIdentity(native) != functionTypeIdentity(c) &&
            !functionTypesAssignable(native, c),
        "function identity includes calling convention and ABI-relevant effects");
    FunctionType nullable = native;
    nullable.nullable = true;
    tests.expect(functionTypesAssignable(native, nullable) && !functionTypesAssignable(nullable, native),
        "non-null function pointers widen to nullable pointers but not conversely");

    FunctionType complete = c;
    complete.parameters.push_back("type:Contracts::Value");
    complete.carrier = FunctionCarrier::Reference;
    complete.callingConvention = CallingConvention::NativeSharp;
    complete.errorType = "type:Contracts::Failure";
    complete.lifetime = "request";
    complete.isUnsafe = true;
    complete.isNoHeap = true;
    complete.isAsync = true;
    complete.nullable = true;
    const std::string completeIdentity = functionTypeIdentity(complete);
    const std::optional<FunctionType> decoded = parseFunctionTypeIdentity(completeIdentity);
    tests.expect(decoded.has_value() && decoded->parameters == complete.parameters &&
            decoded->result == complete.result && decoded->carrier == complete.carrier &&
            decoded->callingConvention == complete.callingConvention &&
            decoded->errorType == complete.errorType && decoded->lifetime == complete.lifetime &&
            decoded->isUnsafe && decoded->isNoHeap && decoded->isAsync && decoded->nullable,
        "function identity round-trips every ABI, effect, lifetime, and nullability component");
    tests.expect(isFunctionTypeIdentity(completeIdentity) &&
            !isFunctionTypeIdentity("fn:ptr:c:00000:0#:1:11#builtin:int"),
        "function identity recognition rejects truncated structural keys");

    FunctionType safe = native;
    FunctionType unsafe = native;
    unsafe.isUnsafe = true;
    tests.expect(functionTypesAssignable(safe, unsafe) && !functionTypesAssignable(unsafe, safe),
        "a safe callable can satisfy an unsafe slot but unsafe cannot enter a safe slot");
    FunctionType noHeap = native;
    noHeap.isNoHeap = true;
    tests.expect(functionTypesAssignable(noHeap, native) && !functionTypesAssignable(native, noHeap),
        "a noheap callable satisfies an ordinary slot but the reverse is rejected");
    FunctionType throwing = native;
    throwing.errorType = "type:Contracts::Failure";
    tests.expect(functionTypesAssignable(native, throwing) && !functionTypesAssignable(throwing, native),
        "a nonthrowing callable widens to a typed-throws slot but not conversely");

    ClosureLayout layout;
    std::string error;
    const std::array<Capture, 2> captures = {{{"small", 1, 1, CaptureMode::Copy, true},
        {"resource", 8, 8, CaptureMode::Move, true}}};
    tests.expect(computeClosureLayout(captures, layout, error) && layout.size == 16 && layout.alignment == 8,
        "closure environment is a native aggregate with deterministic Win64 layout");
    tests.expect(callableTrait(captures) == CallableTrait::FnOnce,
        "a move capture makes the closure explicitly FnOnce");
    const std::array<Capture, 1> escaping = {{{"local", 8, 8, CaptureMode::Reference, false}}};
    tests.expect(!validateClosureEscape(escaping, true, error) && validateClosureEscape(escaping, false, error),
        "reference captures cannot escape their source lifetime");
    tests.expect(nonCapturingLambdaConvertsToPointer({}) && !nonCapturingLambdaConvertsToPointer(captures),
        "only non-capturing lambdas convert directly to function pointers");
    tests.expect(!genericLambdaUsesErasedDispatch(),
        "generic lambdas monomorphize and never acquire automatic erased dispatch");
    const auto bound = windowsX64BoundMethodLayout();
    tests.expect(bound.size == 16 && bound.alignment == 8,
        "a bound method exposes receiver plus entry pointer on Windows x64");
    tests.expect(validateMustTail(native, native, false, error) &&
            !validateMustTail(native, c, false, error) &&
            !validateMustTail(native, native, true, error),
        "must_tail is verified against ABI equality and pending cleanup");

    const std::filesystem::path executionProject = writeProject(
        directedRoot / "execution", "FunctionTypes", R"NS(
namespace FunctionTypes
{
    public class Program
    {
        private static int Increment(int value) { return value + 1; }
        private static int ApplyRaw(fn* C(int) -> int callback, int value)
        {
            return callback(value);
        }
        private static int ApplyReference(fn&(int) -> int callback, int value)
        {
            return callback(value);
        }
        private static int Pick(fn* C(int) -> int callback) { return 1; }
        private static int Pick(fn&(int) -> int callback) { return 2; }
        private static T Identity<T>(T value) { return value; }
        public static int Main()
        {
            fn* C(int) -> int raw = Increment;
            fn&(int) -> int reference = Increment;
            fn* C(int) -> int genericRaw = Identity<fn* C(int) -> int>(raw);
            return ApplyRaw(genericRaw, 20) + ApplyReference(reference, 20) +
                Pick(raw) + Pick(reference);
        }
    }
}
)NS");
    std::string executionDiagnostic;
    const bool executionPassed = compileAndRun(executionProject, 45, executionDiagnostic);
    tests.expect(executionPassed,
        "O0/NSLink executes structural function pointers and references (" +
            executionDiagnostic + ")");

    const std::filesystem::path invalidProject = writeProject(
        directedRoot / "invalid", "InvalidFunctionTypes", R"NS(
namespace InvalidFunctionTypes
{
    public class Program
    {
        private static int Dangerous(int value) { return value; }
        public static int Main()
        {
            fn*(int) -> int unsafe callback = Dangerous;
            return callback(42);
        }
    }
}
)NS");
    nativesharp::frontend::ProjectCompilation invalidCompilation;
    std::string invalidError;
    const bool invalidBuilt = invalidCompilation.compileProjectOopMir(
        invalidProject, invalidError);
    std::string invalidDiagnostics;
    for (const auto& diagnostic : invalidCompilation.diagnostics().diagnostics())
        invalidDiagnostics += diagnostic.message() + " | ";
    tests.expect(!invalidBuilt &&
            invalidDiagnostics.find("requires an explicit unsafe") != std::string::npos,
        "unsafe structural function values require an explicit unsafe block (" +
            invalidError + invalidDiagnostics + ")");

    const std::filesystem::path metadataProject = writeProject(
        directedRoot / "metadata", "FunctionContracts", R"NS(
namespace FunctionContracts
{
    public class Api
    {
        private static int Increment(int value) { return value + 1; }
        public static fn* C(int) -> int SelectRaw() { return Increment; }
        public static int Invoke(fn&(int) -> int callback, int value)
        {
            return callback(value);
        }
        public static int AcceptVariadic(fn* C(int, ...) -> int callback) { return 0; }
    }
    public class Win32
    {
        extern C from "user32.dll"
        {
            public int EnumWindows(
                fn* stdcall(nint, nint) -> int callback,
                nint state);
        }
    }
}
)NS", "StaticLibrary");
    const std::filesystem::path metadataLibrary =
        directedRoot / "metadata" / "FunctionContracts.lib";
    const std::filesystem::path metadataPath =
        directedRoot / "metadata" / "FunctionContracts.nsmeta";
    const std::string metadataProjectText = metadataProject.string();
    const std::string metadataLibraryText = metadataLibrary.string();
    const std::array<std::string_view, 5> metadataArguments = {
        "--emit-project-oop-lib", metadataProjectText, "-O0", "-o", metadataLibraryText};
    std::string metadataOutput;
    std::string metadataDriverError;
    const bool metadataWritten = runDriver(
        metadataArguments, metadataOutput, metadataDriverError) == ExitCode::Success;
    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec metadataCodec;
    std::string metadataError;
    const bool metadataRead = metadataWritten &&
        metadataCodec.read(metadataPath, metadata, metadataError);
    const auto selectRaw = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "SelectRaw";
    });
    const auto invoke = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "Invoke";
    });
    const auto acceptVariadic = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "AcceptVariadic";
    });
    const auto enumWindows = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "EnumWindows";
    });
    std::optional<FunctionType> variadicType;
    std::optional<FunctionType> importCallbackType;
    if (acceptVariadic != metadata.members.end() && !acceptVariadic->parameters.empty())
        variadicType = parseFunctionTypeIdentity(acceptVariadic->parameters.front().typeKey);
    if (enumWindows != metadata.members.end() && !enumWindows->parameters.empty())
        importCallbackType = parseFunctionTypeIdentity(enumWindows->parameters.front().typeKey);
    const bool metadataHasStructuralTypes = metadataRead &&
        selectRaw != metadata.members.end() && invoke != metadata.members.end() &&
        parseFunctionTypeIdentity(selectRaw->declaredTypeKey).has_value() &&
        !invoke->parameters.empty() &&
        parseFunctionTypeIdentity(invoke->parameters.front().typeKey).has_value() &&
        variadicType.has_value() && variadicType->variadic &&
        importCallbackType.has_value() &&
        importCallbackType->callingConvention == CallingConvention::StdCall;
    tests.expect(metadataHasStructuralTypes,
        "NSMETA round-trips structural function return and parameter identities (" +
            metadataDriverError + metadataOutput + metadataError + ")");

    std::filesystem::remove_all(directedRoot, cleanupError);
    return tests.result();
}
