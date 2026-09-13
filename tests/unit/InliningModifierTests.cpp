#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

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

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const auto directory = root / name;
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

bool diagnosticContains(
    const nativesharp::frontend::ProjectCompilation& compilation,
    std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}
}

int main()
{
    using nativesharp::frontend::ClassMemberKind;
    using nativesharp::frontend::InliningPolicy;
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() /
        "nativesharp_inlining_modifier_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const auto positive = writeProject(root, "InlinePositive", R"NS(
public class Math
{
    public static inline int AddOne(int value) { return value + 1; }
    public inline int Twice(int value) { return value * 2; }
    public static int Main() { return AddOne(41) - 42; }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(positive, error);
    tests.expect(ok && error.empty(), "inline methods compile through physical OOP MIR");
    if (ok && compilation.memberModel() != nullptr)
    {
        std::size_t preferred = 0;
        std::size_t automatic = 0;
        for (const auto& member : compilation.memberModel()->members())
        {
            if (member.kind != ClassMemberKind::Method) continue;
            if (member.inliningPolicy == InliningPolicy::Preferred) ++preferred;
            if (member.inliningPolicy == InliningPolicy::Automatic) ++automatic;
        }
        tests.expect(preferred == 2, "both inline spellings become the preferred policy");
        tests.expect(automatic >= 1, "an unmodified method retains automatic policy");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
        tests.expect(module != nullptr && lowerer.error().empty(),
            "inline methods lower to verifier-ready LLVM");
        if (module != nullptr)
        {
            std::string verifyError;
            tests.expect(module->verify(verifyError), "inlinehint module passes LLVM verification");
            tests.expect(module->text().find("inlinehint") != std::string::npos,
                "preferred policy emits LLVM inlinehint on the callable definition");
        }
    }

    for (const auto& [name, source] : {
        std::pair{"InlineField", "public class Bad { public inline int Value; }"},
        std::pair{"InlineProperty", "public class Bad { public inline int Value { get; } }"},
        std::pair{"InlineAbstract", "public abstract class Bad { public inline abstract int Read(); }"}})
    {
        ProjectCompilation invalid;
        std::string invalidError;
        const auto project = writeProject(root, name, source);
        tests.expect(!invalid.compileProjectMembers(project, invalidError),
            "inline is rejected outside a concrete ordinary method");
        tests.expect(diagnosticContains(invalid, "inline"),
            "invalid inline placement names the modifier in its diagnostic");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
