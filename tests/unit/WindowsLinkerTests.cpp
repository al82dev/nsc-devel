#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::linker::WindowsExecutableBuilder;
using nativesharp::linker::WindowsExecutableSubsystem;
using nativesharp::llvm_backend::LlvmLowerer;
using nativesharp::llvm_backend::LlvmModule;

std::unique_ptr<LlvmModule> compileProgram(
    std::string_view source,
    FrontendCompilation& compilation,
    std::unique_ptr<LlvmLowerer>& lowerer)
{
    if (!compilation.compileBuffer(
            std::filesystem::path("windows_linker_test.ns"),
            std::string(source),
            true))
    {
        return nullptr;
    }
    if (compilation.semanticAnalyzer() == nullptr || compilation.optimizationModule() == nullptr)
    {
        return nullptr;
    }

    lowerer = std::make_unique<LlvmLowerer>(compilation.semanticAnalyzer()->globalSymbols());
    return lowerer->lower(compilation.optimizationModule());
}

std::vector<unsigned char> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::vector<unsigned char> bytes;
    char character = 0;
    while (stream.get(character))
    {
        bytes.push_back(static_cast<unsigned char>(character));
    }
    return bytes;
}

void writeBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

std::uint32_t readU32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void setU16(std::vector<unsigned char>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<unsigned char>(value & 0xffU);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8U) & 0xffU);
}

void setU32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = static_cast<unsigned char>(value & 0xffU);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<unsigned char>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<unsigned char>((value >> 24U) & 0xffU);
}

bool containsLibrary(const std::vector<std::string>& libraries, std::string_view expected)
{
    return std::find(libraries.begin(), libraries.end(), expected) != libraries.end();
}

bool importsSymbol(
    const nativesharp::linker::PeExecutableInfo& info,
    std::string_view library,
    std::string_view symbol)
{
    return std::any_of(
        info.importedSymbols.begin(),
        info.importedSymbols.end(),
        [&](const nativesharp::linker::PeImportedSymbol& imported)
        {
            return imported.library == library && imported.name == symbol;
        });
}

#ifdef _WIN32
class ScopedEnvironmentVariable final
{
public:
    ScopedEnvironmentVariable(std::string name, std::string value)
        : name_(std::move(name))
    {
        char* previous = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&previous, &length, name_.c_str()) == 0 && previous != nullptr)
        {
            hadPrevious_ = true;
            previous_ = previous;
        }
        std::free(previous);
        _putenv_s(name_.c_str(), value.c_str());
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;

    ~ScopedEnvironmentVariable()
    {
        _putenv_s(name_.c_str(), hadPrevious_ ? previous_.c_str() : "");
    }

private:
    std::string name_;
    std::string previous_;
    bool hadPrevious_ = false;
};

void testConfiguredLinkerDiscoveryByName(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root)
{
    using nativesharp::linker::LldLinker;
    if (root.root_name() != std::filesystem::path("A:"))
    {
        tests.expect(false, "NSC_LINKER PATH discovery fixture must remain on A:");
        return;
    }

    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::filesystem::path executable = root / "nslink.exe";
    writeBytes(executable, std::vector<unsigned char>{'M', 'Z'});

    {
        ScopedEnvironmentVariable path("PATH", root.string());
        ScopedEnvironmentVariable configured("NSC_LINKER", "nslink.exe");
        std::string error;
        std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
        tests.expect(linker != nullptr && linker->executablePath() == executable,
            "NSC_LINKER resolves nslink.exe by name from the native Windows PATH");
        tests.expect(error.empty(), "named NSC_LINKER discovery keeps its diagnostic empty");
    }

    {
        ScopedEnvironmentVariable path("PATH", (root / "missing").string());
        ScopedEnvironmentVariable configured("NSC_LINKER", executable.string());
        std::string error;
        std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
        tests.expect(linker != nullptr && linker->executablePath() == executable,
            "NSC_LINKER preserves an explicit executable path without consulting PATH");
    }

    {
        ScopedEnvironmentVariable path("PATH", (root / "missing").string());
        ScopedEnvironmentVariable configured("NSC_LINKER", "nslink.exe");
        std::string error;
        std::unique_ptr<LldLinker> linker = LldLinker::discover(error);
        tests.expect(linker == nullptr &&
                error == "NSC_LINKER executable 'nslink.exe' was not found in PATH",
            "a missing named NSC_LINKER reports the exact PATH lookup failure");
    }

    std::filesystem::remove_all(root, filesystemError);
}
#endif
}

int main()
{
    using nativesharp::linker::LldLinker;
    using nativesharp::tests::TestContext;

    TestContext tests;
    std::error_code cleanupError;

#ifdef _WIN32
    testConfiguredLinkerDiscoveryByName(
        tests,
        std::filesystem::current_path() / "nativesharp_nsc_linker_path_discovery");
#endif

    std::string discoveryError;
    std::unique_ptr<LldLinker> linker = LldLinker::discover(discoveryError);
    tests.expect(linker != nullptr, "Stage 19 discovers NSLink from the configured toolchain");
    tests.expect(discoveryError.empty(), "successful NSLink discovery keeps its diagnostic empty");
    if (linker != nullptr)
    {
        tests.expect(std::filesystem::is_regular_file(linker->executablePath()),
            "discovered NSLink path names a real executable file");
        tests.expect(std::filesystem::is_regular_file(linker->librarianExecutablePath()),
            "static-library generation resolves an independent librarian executable");

        const std::filesystem::path importLibrary =
            std::filesystem::temp_directory_path() / "nativesharp_stage19_test_kernel32.lib";
        const std::string_view exitProcess = "ExitProcess";
        std::string importError;
        tests.expect(linker->createImportLibrary(
                "KERNEL32.dll",
                std::span<const std::string_view>(&exitProcess, 1),
                importLibrary,
                importError),
            "NSLink creates a COFF import library from a module-definition file");
        tests.expect(std::filesystem::is_regular_file(importLibrary) &&
                std::filesystem::file_size(importLibrary) > 0,
            "generated import library exists and is non-empty");
        std::filesystem::remove(importLibrary, cleanupError);

        importError.clear();
        tests.expect(!linker->createImportLibrary("KERNEL32.dll", {}, importLibrary, importError),
            "import-library generation rejects an empty symbol set");
        tests.expect(importError.find("at least one symbol") != std::string::npos,
            "empty import-library input reports a specific diagnostic");
    }

    FrontendCompilation compilation;
    std::unique_ptr<LlvmLowerer> lowerer;
    std::unique_ptr<LlvmModule> module = compileProgram(
        "extern C int puts(char* text);"
        "int DynamicStack(int n) {"
        "    int* values = stackalloc int[n];"
        "    values[0] = n;"
        "    return values[0];"
        "}"
        "int Main() { puts(\"NativeSharp stage 19\"); return DynamicStack(12000); }",
        compilation,
        lowerer);
    tests.expect(module != nullptr, "linker fixture reaches optimized and lowered LLVM IR");

    const std::filesystem::path executablePath =
        std::filesystem::temp_directory_path() / "nativesharp_stage19_linker_test.exe";
    std::filesystem::remove(executablePath, cleanupError);

    WindowsExecutableBuilder executableBuilder;
    std::string buildError;
    if (module != nullptr)
    {
        tests.expect(executableBuilder.build(*module, executablePath, buildError),
            "WindowsExecutableBuilder emits and links a PE32+ executable");
        tests.expect(buildError.empty(), "successful PE construction keeps its diagnostic empty");
        tests.expect(std::filesystem::is_regular_file(executablePath) &&
                std::filesystem::file_size(executablePath) > 512,
            "linked Windows executable exists and is non-trivial");

        PeExecutableVerifier verifier;
        tests.expect(verifier.verify(executablePath),
            "NativeSharp PE verifier accepts the linked executable");
        tests.expect(verifier.info().machine == 0x8664,
            "linked executable declares IMAGE_FILE_MACHINE_AMD64");
        tests.expect(verifier.info().sectionCount > 0,
            "linked executable contains one or more PE sections");
        tests.expect(verifier.info().optionalHeaderMagic == 0x020b,
            "linked executable uses a PE32+ optional header");
        tests.expect(verifier.info().entryPointRva != 0,
            "linked executable has a non-zero native entry point");
        tests.expect(verifier.info().subsystem == 3,
            "linked executable declares the Windows console subsystem");
        tests.expect(verifier.info().importDirectoryRva != 0,
            "linked executable contains a native import directory");
        tests.expect(verifier.info().clrDirectoryRva == 0,
            "linked executable does not contain a CLR runtime header");
        tests.expect(containsLibrary(verifier.info().importedLibraries, "KERNEL32.dll"),
            "linked executable imports the minimal Win32 runtime API from KERNEL32.dll");
        tests.expect(containsLibrary(verifier.info().importedLibraries, "KERNELBASE.dll"),
            "dynamic stack allocation resolves the real x64 __chkstk from KERNELBASE.dll");
        tests.expect(containsLibrary(verifier.info().importedLibraries, "ucrtbase.dll") &&
                importsSymbol(verifier.info(), "ucrtbase.dll", "__C_specific_handler"),
            "Stage 19 executable resolves its Win64 panic personality from the Windows UCRT");
        tests.expect(!containsLibrary(verifier.info().importedLibraries, "vcruntime140.dll"),
            "Stage 19 executable does not require the Visual C++ redistributable DLL");

        const std::vector<unsigned char> original = readBytes(executablePath);
        tests.expect(original.size() > 512, "PE corruption tests start from a complete executable image");
        if (original.size() > 512)
        {
            const std::uint32_t peOffset = readU32(original, 0x3c);
            const std::size_t coffOffset = static_cast<std::size_t>(peOffset) + 4;
            const std::size_t optionalOffset = coffOffset + 20;

            const std::filesystem::path corruptedPath =
                std::filesystem::temp_directory_path() / "nativesharp_stage19_corrupt.exe";

            std::vector<unsigned char> corrupted = original;
            corrupted[0] = 'N';
            writeBytes(corruptedPath, corrupted);
            tests.expect(!verifier.verify(corruptedPath), "PE verifier rejects a broken DOS signature");

            corrupted = original;
            setU16(corrupted, coffOffset, 0x014c);
            writeBytes(corruptedPath, corrupted);
            tests.expect(!verifier.verify(corruptedPath), "PE verifier rejects a non-AMD64 image");

            corrupted = original;
            setU16(corrupted, optionalOffset, 0x010b);
            writeBytes(corruptedPath, corrupted);
            tests.expect(!verifier.verify(corruptedPath), "PE verifier rejects PE32 instead of PE32+");

            corrupted = original;
            setU32(corrupted, optionalOffset + 16, 0);
            writeBytes(corruptedPath, corrupted);
            tests.expect(!verifier.verify(corruptedPath), "PE verifier rejects a zero entry point");

            corrupted = original;
            setU16(corrupted, optionalOffset + 68, 9);
            writeBytes(corruptedPath, corrupted);
            tests.expect(!verifier.verify(corruptedPath), "PE verifier rejects an unsupported subsystem");

            corrupted = original;
            setU32(corrupted, optionalOffset + 112 + (14 * 8), 1);
            writeBytes(corruptedPath, corrupted);
            tests.expect(!verifier.verify(corruptedPath), "PE verifier rejects an unexpected CLR header");

            std::filesystem::remove(corruptedPath, cleanupError);
        }
    }

    {
        FrontendCompilation guiCompilation;
        std::unique_ptr<LlvmLowerer> guiLowerer;
        std::unique_ptr<LlvmModule> guiModule =
            compileProgram("int Main() { return 0; }", guiCompilation, guiLowerer);
        tests.expect(guiModule != nullptr, "GUI subsystem fixture reaches LLVM lowering");
        if (guiModule != nullptr)
        {
            const std::filesystem::path guiPath =
                std::filesystem::temp_directory_path() / "nativesharp_stage19_gui_test.exe";
            std::filesystem::remove(guiPath, cleanupError);
            WindowsExecutableBuilder guiBuilder;
            guiBuilder.setSubsystem(WindowsExecutableSubsystem::Windows);
            std::string guiError;
            tests.expect(guiBuilder.build(*guiModule, guiPath, guiError),
                "WindowsExecutableBuilder links an executable for the Windows GUI subsystem");
            PeExecutableVerifier guiVerifier;
            tests.expect(guiVerifier.verify(guiPath), "PE verifier accepts a Windows GUI executable");
            tests.expect(guiVerifier.info().subsystem == 2,
                "GUI executable declares IMAGE_SUBSYSTEM_WINDOWS_GUI");
            std::filesystem::remove(guiPath, cleanupError);
        }
    }

    {
        const std::filesystem::path tinyPath =
            std::filesystem::temp_directory_path() / "nativesharp_stage19_tiny.exe";
        writeBytes(tinyPath, std::vector<unsigned char>{'M', 'Z'});
        PeExecutableVerifier verifier;
        tests.expect(!verifier.verify(tinyPath), "PE verifier rejects a truncated executable");
        tests.expect(verifier.error().find("DOS header") != std::string::npos,
            "truncated PE diagnostic identifies the incomplete DOS header");
        std::filesystem::remove(tinyPath, cleanupError);
    }

    if (module != nullptr)
    {
        const std::filesystem::path missingDirectory =
            std::filesystem::temp_directory_path() / "nativesharp_stage19_missing_dir" / "bad.exe";
        std::filesystem::remove_all(missingDirectory.parent_path(), cleanupError);
        WindowsExecutableBuilder invalidBuilder;
        std::string invalidError;
        tests.expect(!invalidBuilder.build(*module, missingDirectory, invalidError),
            "WindowsExecutableBuilder rejects an output path in a missing directory");
        tests.expect(invalidError.find("does not exist") != std::string::npos,
            "missing output directory reports a specific diagnostic");
    }

    {
        FrontendCompilation unresolvedCompilation;
        std::unique_ptr<LlvmLowerer> unresolvedLowerer;
        std::unique_ptr<LlvmModule> unresolvedModule = compileProgram(
            "extern C int MissingNativeSymbol();"
            "int Main() { return MissingNativeSymbol(); }",
            unresolvedCompilation,
            unresolvedLowerer);
        tests.expect(unresolvedModule != nullptr,
            "unresolved extern C fixture remains valid through LLVM lowering");
        if (unresolvedModule != nullptr)
        {
            const std::filesystem::path unresolvedPath =
                std::filesystem::temp_directory_path() / "nativesharp_stage19_unresolved_test.exe";
            std::filesystem::remove(unresolvedPath, cleanupError);
            WindowsExecutableBuilder unresolvedBuilder;
            std::string unresolvedError;
            tests.expect(!unresolvedBuilder.build(*unresolvedModule, unresolvedPath, unresolvedError),
                "NSLink rejects an unresolved external NativeSharp program symbol");
            tests.expect(unresolvedError.find("MissingNativeSymbol") != std::string::npos,
                "unresolved-link diagnostic preserves the missing symbol name");
            tests.expect(!std::filesystem::exists(unresolvedPath),
                "failed linking does not leave a stale executable artifact");
        }
    }

    if (module != nullptr)
    {
        const std::filesystem::path spacedDirectory =
            std::filesystem::temp_directory_path() / "nativesharp stage19 paths";
        std::filesystem::remove_all(spacedDirectory, cleanupError);
        std::filesystem::create_directory(spacedDirectory, cleanupError);
        const std::filesystem::path spacedPath = spacedDirectory / "native sharp program.exe";
        WindowsExecutableBuilder spacedBuilder;
        std::string spacedError;
        tests.expect(spacedBuilder.build(*module, spacedPath, spacedError),
            "response-file linking supports output paths containing spaces");
        tests.expect(std::filesystem::is_regular_file(spacedPath),
            "space-containing output path receives the linked executable");
        PeExecutableVerifier spacedVerifier;
        tests.expect(spacedVerifier.verify(spacedPath),
            "PE verifier accepts the executable produced in a path containing spaces");
        std::filesystem::remove_all(spacedDirectory, cleanupError);
    }

    std::filesystem::remove(executablePath, cleanupError);
    return tests.result();
}
