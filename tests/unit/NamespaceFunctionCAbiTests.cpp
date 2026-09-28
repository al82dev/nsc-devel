#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/CHeaderGenerator.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

std::filesystem::path createProject(
    const std::filesystem::path& root,
    std::string_view source)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    writeFile(
        root / "Api.nsproj",
        R"(<Project><PropertyGroup><Name>Api</Name><OutputType>SharedLibrary</OutputType></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Api.ns", source);
    return root / "Api.nsproj";
}
}

int main()
{
    using namespace nativesharp;
    tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_namespace_function_c_abi_test";
    std::error_code fileError;

    frontend::ProjectCompilation compilation;
    std::string error;
    const auto project = createProject(
        root / "valid",
        "namespace NativeApi { public export extern C int ns_add(int left, int right) { return left + right; } }");
    const bool compiled = compilation.compileProjectOopMir(project, error);
    tests.expect(compiled && error.empty() && !compilation.diagnostics().hasErrors(),
        "export extern C namespace function compiles through MIR");
    if (!compiled)
    {
        if (!error.empty()) std::cerr << error << std::endl;
        for (const auto& diagnostic : compilation.diagnostics().diagnostics())
            std::cerr << diagnostic.message() << std::endl;
    }

    if (compiled && compilation.namespaceFunctionModel() != nullptr &&
        compilation.oopMirModel() != nullptr &&
        compilation.nameLookup() != nullptr &&
        compilation.memberModel() != nullptr)
    {
        const auto functions = compilation.namespaceFunctionModel()->functions();
        tests.expect(functions.size() == 1 && functions.front().isExportedC &&
                functions.front().isExternC &&
                functions.front().mangledName != "ns_add",
            "semantic model separates internal native identity from C export name");

        const auto& mir = *compilation.oopMirModel();
        const auto found = std::ranges::find_if(
            mir.functions(),
            [](const auto& function) {
                return function.namespaceFunctionOrdinal.has_value();
            });
        tests.expect(found != mir.functions().end() && found->isExportedC &&
                found->cAbiName == "ns_add",
            "MIR carries the C wrapper contract");

        llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), mir);
        tests.expect(module != nullptr && lowerer.error().empty(),
            "LLVM emits the namespace C wrapper");
        if (module != nullptr)
        {
            const std::string ir = module->text();
            tests.expect(ir.find("@ns_add(") != std::string::npos &&
                    ir.find(functions.front().mangledName) != std::string::npos,
                "IR contains both the public C symbol and internal native implementation");
        }

        interop::CHeaderGenerator headerGenerator;
        const std::filesystem::path header = root / "Api.h";
        error.clear();
        const bool wroteHeader =
            headerGenerator.writeHeader(compilation, header, error);
        const std::string headerText = wroteHeader ? readFile(header) : std::string();
        tests.expect(wroteHeader && error.empty() &&
                headerText.find("NS_API int32_t NS_CALL ns_add(int32_t left, int32_t right);") !=
                    std::string::npos,
            "generated C header publishes the namespace function prototype");

        interop::LibraryMetadataCodec codec;
        interop::LibraryMetadata metadata;
        error.clear();
        const bool wroteMetadata =
            codec.write(compilation, root / "Api.nsmeta", metadata, error);
        tests.expect(wroteMetadata && metadata.namespaceFunctions.size() == 1 &&
                (metadata.namespaceFunctions.front().flags & 8U) != 0U,
            "NSMETA distinguishes a C-exported namespace function");
    }

    frontend::ProjectCompilation duplicate;
    error.clear();
    const auto duplicateProject = createProject(
        root / "duplicate",
        "namespace First { public export extern C int same_name() { return 1; } } "
        "namespace Second { public export extern C int same_name(int value) { return value; } }");
    const bool duplicateOk =
        duplicate.compileProjectMembers(duplicateProject, error);
    tests.expect(!duplicateOk,
        "two namespace functions cannot claim the same flat C symbol");

    std::filesystem::remove_all(root, fileError);
    return tests.result();
}