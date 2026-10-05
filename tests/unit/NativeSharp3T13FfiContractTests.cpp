#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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

bool compileAndRun(const std::filesystem::path& project, std::string& diagnostic)
{
    const std::filesystem::path executable = project.parent_path() / "FfiPositive.exe";
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
        diagnostic += " driver exit code " + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == 42) return true;
    diagnostic += " process exit code " + std::to_string(observed);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    _putenv_s("NSC_LINKER", "A:\\nslink-devel\\build-debug\\bin\\nslink.exe");
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f06-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    const std::filesystem::path positive = writeProject(root / "positive", "FfiPositive", R"NS(
namespace FfiPositive
{
    public class Program
    {
        extern C from "kernel32.dll"
        {
            public uint GetCurrentProcessId() unwind(forbidden);
        }
        public static int Main()
        {
            unsafe("process id import has a non-unwinding Win32 contract")
            {
                uint process = GetCurrentProcessId();
                return process == 0 ? 1 : 42;
            }
        }
    }
}
)NS");
    std::string diagnostic;
    const bool positivePassed = compileAndRun(positive, diagnostic);
    tests.expect(positivePassed,
        "explicit FFI unwind contract executes at O0 through NSLink (" + diagnostic + ")");

    const std::filesystem::path validShape = writeProject(root / "shape", "FfiShape", R"NS(
namespace FfiShape
{
    public class Program
    {
        extern C from "kernel32.dll"
        {
            public bool ConsumeBuffer(byte*! buffer length(count), nuint count);
            public int Register(fn* C(int) -> int callback callback(scoped));
        }
        public static int Main() { return 0; }
    }
}
)NS");
    nativesharp::frontend::ProjectCompilation shapeCompilation;
    std::string shapeError;
    const bool shapeBuilt = shapeCompilation.compileProjectOopMir(validShape, shapeError);
    tests.expect(shapeBuilt,
        "length-linked buffers and scoped callbacks form valid extern C contracts (" +
            shapeError + diagnosticsFor(shapeCompilation) + ")");

    const std::filesystem::path missingLength = writeProject(root / "missing", "FfiMissing", R"NS(
namespace FfiMissing
{
    public class Program
    {
        extern C { public int Read(byte*! data length(missing), nuint count); }
        public static int Main() { return 0; }
    }
}
)NS");
    tests.expect(rejectsWith(missingLength, "links unknown length parameter", diagnostic),
        "unknown FFI length links are rejected (" + diagnostic + ")");

    const std::filesystem::path escaping = writeProject(root / "escaping", "FfiEscaping", R"NS(
namespace FfiEscaping
{
    public class Program
    {
        extern C { public int Register(fn* C(int) -> int callback callback(escaping)); }
        public static int Main() { return 0; }
    }
}
)NS");
    tests.expect(rejectsWith(escaping, "requires an owned callback lifetime transfer", diagnostic),
        "escaping callbacks require ownership transfer (" + diagnostic + ")");

    nativesharp::interop::LibraryMetadata metadata;
    metadata.moduleName = "FfiContract";
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    nativesharp::interop::LibraryMetadataMember member;
    member.name = "Write";
    member.signatureKey = "Write(byte*,nuint)";
    member.declaredTypeKey = "builtin:bool";
    member.ffiUnwindPolicy = 1;
    nativesharp::interop::LibraryMetadataParameter buffer;
    buffer.name = "buffer";
    buffer.typeKey = "builtin:byte*";
    buffer.ffiLengthParameter = 1;
    buffer.ffiLengthParameterName = "count";
    member.parameters.push_back(buffer);
    nativesharp::interop::LibraryMetadataParameter count;
    count.name = "count";
    count.typeKey = "builtin:nuint";
    member.parameters.push_back(count);
    metadata.members.push_back(member);
    const std::filesystem::path metadataPath = root / "ffi.nsmeta";
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const bool wrote = codec.writeModel(metadata, metadataPath, metadataError);
    nativesharp::interop::LibraryMetadata decoded;
    const bool read = wrote && codec.read(metadataPath, decoded, metadataError);
    tests.expect(read && decoded.members.size() == 1U &&
            decoded.members.front().ffiUnwindPolicy == 1U &&
            decoded.members.front().parameters.front().ffiLengthParameter == 1U &&
            decoded.members.front().parameters.front().ffiLengthParameterName == "count",
        "NSMETA 1.38 round-trips FFI contracts (" + metadataError + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
