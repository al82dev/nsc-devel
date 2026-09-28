#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
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
        std::filesystem::current_path() / "_namespace_function_generic_test";
    std::error_code fileError;
    std::filesystem::remove_all(root, fileError);
    std::filesystem::create_directories(root, fileError);

    writeFile(
        root / "Generics.nsproj",
        R"(<Project><PropertyGroup><Name>Generics</Name></PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Functions.ns", R"NS(
namespace Generic
{
    public T Identity<T>(T value) { return value; }
    public T First<T>(T[] values) { return values[0]; }
}
namespace App
{
    int Run()
    {
        int[] values = new int[1];
        values[0] = Generic.Identity<int>(42);
        int result = Generic.First<int>(values);
        delete[] values;
        return result;
    }
}
)NS");

    frontend::ProjectCompilation compilation;
    std::string error;
    const bool compiled = compilation.compileProjectOopMir(root / "Generics.nsproj", error);
    tests.expect(compiled && error.empty() && !compilation.diagnostics().hasErrors(),
        "generic namespace functions reach concrete MIR");

    if (compilation.genericModel() != nullptr)
    {
        const auto templates = compilation.genericModel()->templates();
        tests.expect(std::ranges::count_if(templates, [](const auto& item)
        {
            return item.kind == frontend::GenericTemplateKind::Method &&
                item.ownerName.empty() && item.namespaceName == "Generic";
        }) == 2, "free generic declarations are registered as ownerless callable templates");
        tests.expect(std::ranges::any_of(templates, [](const auto& item)
        {
            return item.name == "Identity" &&
                item.canonicalKey.starts_with("Generics::Generic.Identity") &&
                item.parameters.size() == 1;
        }), "generic namespace function identity has no synthetic declaring type");

        const auto specializations = compilation.genericModel()->specializations();
        tests.expect(std::ranges::any_of(specializations, [](const auto& item)
        {
            return item.specializationKey.find("Generic.Identity") != std::string::npos &&
                item.specializationKey.ends_with("<int>");
        }), "explicit type arguments materialize Identity<int>");
        tests.expect(std::ranges::any_of(specializations, [](const auto& item)
        {
            return item.specializationKey.find("Generic.First") != std::string::npos &&
                item.specializationKey.ends_with("<int>");
        }), "array substitution materializes First<int>");
    }

    if (compilation.namespaceFunctionModel() != nullptr)
    {
        const auto functions = compilation.namespaceFunctionModel()->functions();
        tests.expect(std::ranges::none_of(functions, [](const auto& item)
        {
            return item.name == "Identity" || item.name == "First";
        }), "open generic templates do not leak into ordinary namespace lookup");
        tests.expect(std::ranges::count_if(functions, [](const auto& item)
        {
            return item.namespaceName == "Generic" &&
                item.name.find("__g1__int") != std::string::npos;
        }) == 2, "both concrete namespace function specializations enter semantic binding");
    }

    if (compilation.oopMirModel() != nullptr)
    {
        const auto functions = compilation.oopMirModel()->functions();
        tests.expect(std::ranges::count_if(functions, [](const auto& item)
        {
            return item.namespaceFunctionOrdinal.has_value() &&
                item.debugDisplayName.find("__g1__int") != std::string::npos;
        }) == 2, "only closed generic namespace functions reach MIR");
    }

    interop::LibraryMetadata metadata;
    interop::LibraryMetadataCodec codec;
    error.clear();
    const bool wrote = compiled && codec.write(
        compilation, root / "Generics.nsmeta", metadata, error);
    tests.expect(wrote && error.empty(),
        "public generic namespace function templates publish to NSMETA");
    tests.expect(std::ranges::count_if(metadata.genericTemplates, [](const auto& item)
    {
        return item.kind == static_cast<std::uint8_t>(frontend::GenericTemplateKind::Method) &&
            item.ownerName.empty() && item.namespaceName == "Generic" &&
            !item.sourceText.empty();
    }) == 2, "NSMETA preserves ownerless reusable template source");

    std::filesystem::remove_all(root, fileError);
    return tests.result();
}