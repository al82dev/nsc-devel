#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

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
}

int main()
{
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_callable_result_merger_ordinals";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    writeFile(
        root / "OrdinalMerge.nsproj",
        "<Project><PropertyGroup><Name>OrdinalMerge</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(
        root / "Main.ns",
        "class OrdinalMerge { "
        "public static int First() { int first = 1; return first; } "
        "public static int Second() { int second = 2; return second; } "
        "public static int Third() { int third = 3; { int nested = 4; "
        "return third + nested; } } }\n");

    ProjectCompilation compilation;
    compilation.setCompilationJobs(1);
    std::string error;
    const bool compiled =
        compilation.compileProjectMethodBodies(root / "OrdinalMerge.nsproj", error);
    tests.expect(compiled, "multiple isolated callable shards compile successfully");
    tests.expect(error.empty(), "callable shard compilation reports no execution error");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "callable shard compilation publishes a method-body model");

    if (compilation.methodBodyModel() != nullptr)
    {
        const auto& model = *compilation.methodBodyModel();
        tests.expect(model.scopes().size() >= 4 && model.locals().size() == 4,
            "fixture exercises several global scope and local bases");
        bool localsPointToOwningScopes = true;
        for (const auto& local : model.locals())
        {
            if (local.scopeOrdinal >= model.scopes().size() ||
                model.scopes()[local.scopeOrdinal].methodMemberOrdinal != local.methodMemberOrdinal)
            {
                localsPointToOwningScopes = false;
                break;
            }
        }
        tests.expect(localsPointToOwningScopes,
            "each merged local keeps the exact global ordinal of its owning scope");

        bool scopesPointToOwnedLocals = true;
        for (std::size_t scopeOrdinal = 0; scopeOrdinal < model.scopes().size(); ++scopeOrdinal)
        {
            const auto& scope = model.scopes()[scopeOrdinal];
            for (const std::size_t localOrdinal : scope.localOrdinals)
            {
                if (localOrdinal >= model.locals().size() ||
                    model.locals()[localOrdinal].scopeOrdinal != scopeOrdinal)
                {
                    scopesPointToOwnedLocals = false;
                    break;
                }
            }
            if (!scopesPointToOwnedLocals) break;
        }
        tests.expect(scopesPointToOwnedLocals,
            "each merged scope and local preserve their bidirectional ordinal invariant");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
