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

    // Inspect provenance after callable shards have been merged, not merely
    // the result of analyzing each callable in isolation.
    writeFile(root / "Main.ns",
        "public borrowed struct View<lifetime a> { "
        "private borrowed[a] int* target; "
        "public View(ref[a] int first, ref[a] int second) { unsafe(\"view\") { target = &second; } } "
        "public ref[this] int Get() { unsafe(\"view\") { return ref *target; } } } "
        "public struct Owner { public int Value; ~Owner() { } } "
        "class IdentityMerge { "
        "public static int First(ref int parameter) { Owner owner; owner.Value = 1; "
        "View view = new View(ref owner.Value, ref parameter); return view.Get(); } "
        "public static int Second(ref int parameter) { Owner owner; owner.Value = 2; "
        "View view = new View(ref owner.Value, ref parameter); return view.Get(); } }\n");
    ProjectCompilation identityCompilation;
    identityCompilation.setCompilationJobs(1);
    error.clear();
    const bool identityCompiled = identityCompilation.compileProjectMethodBodies(root / "OrdinalMerge.nsproj", error);
    tests.expect(identityCompiled && error.empty(), "view provenance fixture compiles through callable shards");
    if (identityCompilation.methodBodyModel() != nullptr)
    {
        const auto& locals = identityCompilation.methodBodyModel()->locals();
        std::size_t viewsChecked = 0;
        for (const auto& local : locals)
        {
            if (local.name != "view") continue;
            bool ownerMatches = false;
            bool parameterMatches = false;
            for (const auto& source : local.viewSourceIdentities)
            {
                if (source.name == "owner" && source.kind == nativesharp::frontend::ViewSourceIdentityKind::Local &&
                    source.ordinal < locals.size() && locals[source.ordinal].name == "owner" &&
                    locals[source.ordinal].methodMemberOrdinal == local.methodMemberOrdinal)
                    ownerMatches = true;
                if (source.name == "parameter" &&
                    source.kind == nativesharp::frontend::ViewSourceIdentityKind::Parameter && source.ordinal == 0)
                    parameterMatches = true;
            }
            tests.expect(ownerMatches, "merged view identity refers to its own callable's owner");
            tests.expect(parameterMatches, "merging preserves the callable-relative parameter ordinal");
            ++viewsChecked;
        }
        tests.expect(viewsChecked == 2, "both callable view identities were checked");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
