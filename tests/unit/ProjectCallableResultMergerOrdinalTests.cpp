#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

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
    {
        using nativesharp::frontend::ViewUnknownOrigin;
        using nativesharp::frontend::appendViewUnknownOrigin;
        std::vector<ViewUnknownOrigin> origins;
        for (std::size_t i = 0; i < 8; ++i) appendViewUnknownOrigin(origins, {i, i});
        appendViewUnknownOrigin(origins, {7, 7});
        tests.expect(origins.size() == 8, "duplicate uncertainty observations do not consume the bound");
        appendViewUnknownOrigin(origins, {8, 8});
        appendViewUnknownOrigin(origins, {9, 9});
        tests.expect(origins.size() == 1 && !origins.front().producerScopeOrdinal.has_value() &&
                         !origins.front().observedLocalCount.has_value(),
            "too many uncertainty observations collapse to unlocated Unknown");
    }
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
        "public static void Write(View input, ref View output) { output = input; } "
        "public static void WriteOut(View input, out View output) { output = input; } "
        "public static int First(ref int parameter) { Owner owner; owner.Value = 1; "
        "View view = new View(ref owner.Value, ref parameter); View copy = view; "
        "View assigned = default(View); assigned = copy; "
        "View output = default(View); Write(copy, ref output); "
        "{ ref View indirect = ref output; Write(copy, ref indirect); } "
        "View output2 = default(View); WriteOut(copy, out output2); return output.Get(); } "
        "public static int Second(ref int parameter) { Owner owner; owner.Value = 2; "
        "View view = new View(ref owner.Value, ref parameter); View copy = view; "
        "View assigned = default(View); assigned = copy; "
        "View output = default(View); Write(copy, ref output); "
        "{ ref View indirect = ref output; Write(copy, ref indirect); } "
        "View output2 = default(View); WriteOut(copy, out output2); return output.Get(); } "
        "public static int Many(ref int parameter) { Owner owner; owner.Value = 3; "
        "View many = new View(ref owner.Value, ref parameter); "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "View copiedMany = many; View outputMany = default(View); "
        "Write(copiedMany, ref outputMany); return outputMany.Get(); } }\n");
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
            bool originsMatch = !local.viewUnknownOrigins.empty();
            for (const auto& origin : local.viewUnknownOrigins)
            {
                if (!origin.producerScopeOrdinal.has_value() || !origin.observedLocalCount.has_value() ||
                    *origin.producerScopeOrdinal >= identityCompilation.methodBodyModel()->scopes().size() ||
                    identityCompilation.methodBodyModel()->scopes()[*origin.producerScopeOrdinal].methodMemberOrdinal !=
                        local.methodMemberOrdinal || *origin.observedLocalCount > locals.size())
                    originsMatch = false;
            }
            tests.expect(originsMatch, "unknown producer observations remain in their callable after merge");
            const nativesharp::frontend::MethodLocalSymbol* copy = nullptr;
            const nativesharp::frontend::MethodLocalSymbol* output = nullptr;
            const nativesharp::frontend::MethodLocalSymbol* output2 = nullptr;
            const nativesharp::frontend::MethodLocalSymbol* assigned = nullptr;
            for (const auto& candidate : locals)
                if (candidate.methodMemberOrdinal == local.methodMemberOrdinal)
                {
                    if (candidate.name == "copy") copy = &candidate;
                    if (candidate.name == "output") output = &candidate;
                    if (candidate.name == "output2") output2 = &candidate;
                    if (candidate.name == "assigned") assigned = &candidate;
                }
            tests.expect(copy != nullptr && copy->viewUnknownOrigins == local.viewUnknownOrigins,
                "view copy preserves the producer observations exactly");
            bool outputPreservesInputs = output != nullptr && output->viewUnknownOrigins.size() >= 3;
            if (output != nullptr)
                for (const auto& origin : local.viewUnknownOrigins)
                    if (std::ranges::find(output->viewUnknownOrigins, origin) == output->viewUnknownOrigins.end())
                        outputPreservesInputs = false;
            tests.expect(outputPreservesInputs,
                "ref output and reference-alias output retain input and call uncertainty");
            bool outPreservesInputs = output2 != nullptr && output2->viewUnknownOrigins.size() >= 2;
            bool assignmentPreservesInputs = assigned != nullptr;
            for (const auto& origin : local.viewUnknownOrigins)
            {
                if (output2 != nullptr && std::ranges::find(output2->viewUnknownOrigins, origin) ==
                    output2->viewUnknownOrigins.end()) outPreservesInputs = false;
                if (assigned != nullptr && std::ranges::find(assigned->viewUnknownOrigins, origin) ==
                    assigned->viewUnknownOrigins.end()) assignmentPreservesInputs = false;
            }
            tests.expect(outPreservesInputs, "out output conservatively retains input and call uncertainty");
            tests.expect(assignmentPreservesInputs, "view assignment retains producer uncertainty");
            ++viewsChecked;
        }
        tests.expect(viewsChecked == 2, "both callable view identities were checked");
        bool collapsed = true;
        std::size_t collapsedChecked = 0;
        for (const auto& local : locals)
        {
            if (local.name != "many" && local.name != "copiedMany" && local.name != "outputMany") continue;
            ++collapsedChecked;
            if (local.viewUnknownOrigins.size() != 1 ||
                local.viewUnknownOrigins.front().producerScopeOrdinal.has_value() ||
                local.viewUnknownOrigins.front().observedLocalCount.has_value()) collapsed = false;
        }
        tests.expect(collapsedChecked == 3 && collapsed,
            "collapsed uncertainty survives shard merge, copying and ref output");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
