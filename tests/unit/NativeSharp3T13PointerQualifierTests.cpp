#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Language/AbiSemantics.h"

#include "../support/TestContext.h"

#include <array>
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

std::string diagnosticsFor(const nativesharp::frontend::ProjectCompilation& compilation)
{
    std::string text;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        text += diagnostic.message() + " | ";
    return text;
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
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode result = driver.run(arguments, output, error);
    diagnostic = error.str() + output.str();
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

bool rejectsWith(
    const std::filesystem::path& project,
    std::string_view expected,
    std::string& diagnostic)
{
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(project, error);
    diagnostic = error + diagnosticsFor(compilation);
    return !built && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    using nativesharp::language::PointerContract;
    const PointerContract nullable{true, true, true, 1, 0};
    const PointerContract nonNull{false, true, true, 1, 0};
    const PointerContract aligned{false, true, true, 16, 0};
    const PointerContract untracked{false, true, false, 16, 0};
    tests.expect(
        nativesharp::language::pointerContractImplicitlyAssignable(nonNull, nullable) &&
        !nativesharp::language::pointerContractImplicitlyAssignable(nullable, nonNull) &&
        nativesharp::language::pointerCastRequiresUnsafe(nullable, nonNull) &&
        nativesharp::language::pointerCastRequiresUnsafe(nonNull, aligned) &&
        nativesharp::language::pointerCastRequiresUnsafe(untracked, aligned),
        "pointer contract lattice preserves and audits stronger guarantees");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f03-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    const std::filesystem::path positive = writeProject(root / "positive", "PointerPositive", R"NS(
namespace PointerPositive
{
    public class Program
    {
        public static unsafe int Read(int*! pointer)
        {
            unsafe("nonnull pointer from caller") { return *pointer; }
        }
        public static int Main()
        {
            int value = 42;
            int*! pointer = &value;
            unsafe("callee requires raw-pointer audit") { return Read(pointer); }
        }
    }
}
)NS");
    std::string positiveDiagnostic;
    tests.expect(compileAndRun(positive, 42, positiveDiagnostic),
        "nonnull pointers execute at O0 through NSLink (" + positiveDiagnostic + ")");

    const std::filesystem::path nullStrengthening = writeProject(
        root / "null-strengthening", "NullStrengthening", R"NS(
namespace NullStrengthening
{
    public class Program
    {
        public static int Main() { int*! pointer = null; return 0; }
    }
}
)NS");
    std::string nullDiagnostic;
    tests.expect(rejectsWith(nullStrengthening, "pointer initializer does not satisfy", nullDiagnostic),
        "null cannot implicitly initialize a nonnull pointer (" + nullDiagnostic + ")");

    const std::filesystem::path alignmentStrengthening = writeProject(
        root / "alignment-strengthening", "AlignmentStrengthening", R"NS(
namespace AlignmentStrengthening
{
    public class Program
    {
        public static int Main()
        {
            int value = 0;
            int*! source = &value;
            int*! align(16) target = source;
            return 0;
        }
    }
}
)NS");
    std::string alignmentDiagnostic;
    tests.expect(rejectsWith(
            alignmentStrengthening, "pointer initializer does not satisfy", alignmentDiagnostic),
        "alignment knowledge cannot be invented implicitly (" + alignmentDiagnostic + ")");

    const std::filesystem::path unsafeCastRequired = writeProject(
        root / "unsafe-cast-required", "UnsafeCastRequired", R"NS(
namespace UnsafeCastRequired
{
    public class Program
    {
        public static int Main()
        {
            int*? source = null;
            int*! target = (int*!)source;
            return 0;
        }
    }
}
)NS");
    std::string castDiagnostic;
    tests.expect(rejectsWith(
            unsafeCastRequired, "pointer cast strengthens nullability", castDiagnostic),
        "casts that invent nonnull guarantees require unsafe (" + castDiagnostic + ")");

    const std::filesystem::path auditedCast = writeProject(
        root / "audited-cast", "AuditedPointerCast", R"NS(
namespace AuditedPointerCast
{
    public class Program
    {
        public static int Main()
        {
            int value = 42;
            int*? source = &value;
            int*! align(4) target;
            unsafe("caller proves liveness and alignment")
            {
                target = (int*! align(4))source;
                return *target;
            }
        }
    }
}
)NS");
    std::string auditedDiagnostic;
    tests.expect(compileAndRun(auditedCast, 42, auditedDiagnostic),
        "audited qualifier strengthening executes at O0 through NSLink (" +
            auditedDiagnostic + ")");

    const std::filesystem::path provenance = writeProject(
        root / "provenance", "PointerProvenance", R"NS(
namespace PointerProvenance
{
    public class Program
    {
        public static unsafe int Consume(int*! provenance(tracked) pointer) { return 0; }
        public static int Main()
        {
            int value = 0;
            int*! provenance(none) raw;
            unsafe("explicitly erase provenance")
            {
                raw = (int*! provenance(none))&value;
                return Consume(raw);
            }
        }
    }
}
)NS");
    std::string provenanceDiagnostic;
    const bool provenanceRejected = rejectsWith(
        provenance, "no viable overload", provenanceDiagnostic);
    tests.expect(provenanceRejected,
        "provenance erased at an FFI-shaped boundary cannot satisfy tracked parameters (" +
            provenanceDiagnostic + ")");

    const std::filesystem::path addressSpace = writeProject(
        root / "address-space", "PointerAddressSpace", R"NS(
namespace PointerAddressSpace
{
    public class Program
    {
        public static int Main()
        {
            int value = 0;
            int*! source = &value;
            int*! addrspace(1) target = source;
            return 0;
        }
    }
}
)NS");
    std::string addressDiagnostic;
    tests.expect(rejectsWith(addressSpace, "pointer initializer does not satisfy", addressDiagnostic),
        "reserved address spaces never convert implicitly (" + addressDiagnostic + ")");

    const std::filesystem::path invalidAlignment = writeProject(
        root / "invalid-alignment", "InvalidPointerAlignment", R"NS(
namespace InvalidPointerAlignment
{
    public class Program
    {
        public static int Main() { int* align(3) pointer = null; return 0; }
    }
}
)NS");
    std::string invalidAlignmentDiagnostic;
    tests.expect(rejectsWith(
            invalidAlignment, "alignment knowledge must be a power of two", invalidAlignmentDiagnostic),
        "invalid pointer alignment is diagnosed at the source contract (" +
            invalidAlignmentDiagnostic + ")");

    const std::filesystem::path arithmetic = writeProject(
        root / "arithmetic", "PointerArithmeticContract", R"NS(
namespace PointerArithmeticContract
{
    public class Program
    {
        public static int Main()
        {
            int value = 42;
            int*! align(4) aligned;
            unsafe("caller proves stack alignment") { aligned = (int*! align(4))&value; }
            int*! align(4) shifted = aligned + 0;
            return 0;
        }
    }
}
)NS");
    std::string arithmeticDiagnostic;
    tests.expect(rejectsWith(arithmetic, "pointer initializer does not satisfy", arithmeticDiagnostic),
        "pointer arithmetic conservatively weakens alignment knowledge (" +
            arithmeticDiagnostic + ")");

    return tests.result();
}
