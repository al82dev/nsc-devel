#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}
}

int main()
{
    using namespace nativesharp;
    tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_metadata_test";
    std::error_code fileError;
    std::filesystem::remove_all(root, fileError);
    std::filesystem::create_directories(root, fileError);

    writeFile(
        root / "Producer.nsproj",
        R"(<Project><PropertyGroup><Name>Producer</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(
        root / "Functions.ns",
        "namespace Numeric { public int Add(int left, int right) { return left + right; } internal int Hidden() { return 0; } }");

    frontend::ProjectCompilation compilation;
    std::string error;
    const bool compiled =
        compilation.compileProjectOopMir(root / "Producer.nsproj", error);
    tests.expect(compiled && error.empty() && !compilation.diagnostics().hasErrors(),
        "producer with public namespace function reaches MIR");

    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata written;
    const std::filesystem::path metadataPath = root / "Producer.nsmeta";
    const bool wrote = compiled &&
        codec.write(compilation, metadataPath, written, error);
    tests.expect(wrote && error.empty(),
        "NSMETA producer writes namespace function records");
    tests.expect(written.formatMinor == 22,
        "namespace function publication uses current NSMETA format");
    tests.expect(written.namespaceFunctions.size() == 1,
        "only public namespace functions are published");
    if (!written.namespaceFunctions.empty())
    {
        const auto& function = written.namespaceFunctions.front();
        tests.expect(function.namespaceName == "Numeric" &&
                function.name == "Add" &&
                function.signatureKey == "Add(builtin:int,builtin:int)",
            "logical namespace function identity survives publication");
        tests.expect(function.declaredTypeKey == "builtin:int" &&
                function.parameters.size() == 2 &&
                !function.mangledName.empty(),
            "signature and physical symbol are published");
    }

    interop::LibraryMetadata decoded;
    error.clear();
    const bool read = wrote && codec.read(metadataPath, decoded, error);
    tests.expect(read && error.empty() && decoded.namespaceFunctions.size() == 1,
        "current NSMETA round trip preserves namespace functions");
    tests.expect(read && codec.verifyCurrentCompiler(decoded, error),
        "current compiler validates namespace function metadata");

    interop::LibraryMetadata previous;
    previous.formatMinor = 20;
    previous.producerVersion = written.producerVersion;
    previous.moduleName = "Previous";
    previous.targetTriple = written.targetTriple;
    const std::filesystem::path previousPath = root / "Previous.nsmeta";
    error.clear();
    const bool wrotePrevious = codec.writeModel(previous, previousPath, error);
    interop::LibraryMetadata decodedPrevious;
    const bool readPrevious = wrotePrevious &&
        codec.read(previousPath, decodedPrevious, error);
    tests.expect(readPrevious && decodedPrevious.namespaceFunctions.empty(),
        "NSMETA 1.20 remains readable without a namespace function section");

    std::filesystem::remove_all(root, fileError);
    return tests.result();
}