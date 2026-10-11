#include "CompilationTestSupport.h"

#include <limits>

namespace nsc::suite
{
namespace
{
using namespace compilation_support;

void globNormalizationContract(TestContext& context, std::size_t variant)
{
    const std::string pattern = variant == 1 ? "Main.ns" :
        variant == 2 ? "src\\**\\*.ns" :
        variant == 3 ? "" :
        variant == 4 ? "../Shared/*.ns" :
        variant == 5 ? "." :
        variant == 6 ? "Models/*.NS" :
        variant == 7 ? "src/**/*.ns" : std::string(512U, 'a') + ".ns";
    GlobPattern glob(pattern);
    if (variant == 3 || variant == 4 || variant == 5)
    {
        context.expect(!glob.isValid() && !glob.validationError().empty(),
            "CF-COMPILATION-001 invalid glob forms are rejected at construction");
        return;
    }
    const bool normalized = variant != 2 || glob.pattern() == "src/**/*.ns";
    const bool caseFolded = variant != 6 || glob.matches("models/PLAYER.ns");
    context.expect(glob.isValid() && normalized && caseFolded,
        "CF-COMPILATION-001 path separators normalize and validation is stable");
}

void globSegmentContract(TestContext& context, std::size_t variant)
{
    GlobPattern glob(variant == 1 ? "*.ns" : "Models/Player?.n*");
    const std::vector<std::pair<std::filesystem::path, bool>> matrix = {
        {"Player.ns", variant == 1},
        {"Models/Player1.ns", variant != 1},
        {"Models/Player10.ns", false},
        {"Models/playerA.NS", variant != 1},
        {"Models/Nested/Player1.ns", false}};
    bool exact = glob.isValid();
    const std::size_t repetitions = variant == 8 ? 256U : 1U;
    for (std::size_t repeat = 0; repeat < repetitions; ++repeat)
    {
        for (const auto& [path, expected] : matrix)
        {
            exact = exact && glob.matches(path) == expected;
        }
    }
    context.expect(exact, "CF-COMPILATION-002 wildcard segment matching obeys *, ?, and folding");
}

void hierarchicalGlobContract(TestContext& context, std::size_t variant)
{
    const GlobPattern recursive(variant == 4 ? "src/**tail.ns" : "src/**/Item*.ns");
    if (variant == 4)
    {
        context.expect(!recursive.isValid() && !recursive.matches("src/tail.ns"),
            "CF-COMPILATION-003 embedded double-star is invalid and never matches");
        return;
    }
    std::filesystem::path path = variant == 1 ? "src/Item.ns" : "src/A/B/Item42.ns";
    if (variant == 3)
    {
        path = "Item.ns";
    }
    if (variant == 5)
    {
        path = "src/A/B/Other.ns";
    }
    if (variant == 8)
    {
        path = "src";
        for (std::size_t index = 0; index < 96U; ++index)
        {
            path /= "segment" + std::to_string(index);
        }
        path /= "Item.ns";
    }
    const bool expected = variant != 3 && variant != 5;
    context.expect(recursive.isValid() && recursive.matches(path) == expected,
        "CF-COMPILATION-003 double-star spans exactly zero or more complete path segments");
}

void partialAdmissionContract(TestContext& context, std::size_t variant)
{
    PartialTypeAggregator aggregator;
    std::string error;
    TypePartDescriptor first = samplePart(0U);
    TypePartDescriptor second = samplePart(1U);
    if (variant == 3)
    {
        first.sourcePath.clear();
        context.expect(!aggregator.addPart(first, error) && contains(error, "source path"),
            "CF-COMPILATION-004 missing source path is rejected at admission");
        return;
    }
    if (variant == 4)
    {
        first.identity = TypeIdentity("bad-module", {}, {}, "Player");
        context.expect(!aggregator.addPart(first, error) && contains(error, "invalid type identity"),
            "CF-COMPILATION-004 invalid identity is rejected at admission");
        return;
    }
    context.expect(aggregator.addPart(first, error),
        "CF-COMPILATION-004 first valid part is admitted");
    if (variant == 5)
    {
        second = first;
        context.expect(!aggregator.addPart(second, error) && contains(error, "duplicate"),
            "CF-COMPILATION-004 duplicate identity and source location is rejected");
        return;
    }
    if (variant == 6)
    {
        second.sourceOffset = first.sourceOffset;
        second.sourceOrdinal = first.sourceOrdinal;
        second.sourcePath = "Synthetic.ns";
    }
    const std::size_t count = variant == 8 ? 1024U : 2U;
    bool accepted = true;
    for (std::size_t index = 1U; index < count; ++index)
    {
        TypePartDescriptor part = samplePart(index);
        accepted = accepted && aggregator.addPart(std::move(part), error);
    }
    context.expect(accepted, "CF-COMPILATION-004 distinct source locations are admitted deterministically");
}

void partialKindContract(TestContext& context, std::size_t variant)
{
    PartialTypeAggregator aggregator;
    std::string error;
    TypePartDescriptor first = samplePart(0U, variant != 3);
    TypePartDescriptor second = samplePart(1U, true);
    if (variant == 4)
    {
        second.kind = TypeDeclarationKind::Struct;
    }
    if (variant == 5)
    {
        first.isPartial = false;
        second.isPartial = false;
    }
    aggregator.addPart(first, error);
    if (variant != 1)
    {
        aggregator.addPart(second, error);
    }
    const bool result = aggregator.finalize(error);
    const bool expected = variant == 1 || variant == 2 || variant == 6 || variant == 7 || variant == 8;
    context.expect(result == expected && (result || aggregator.types().empty()),
        "CF-COMPILATION-005 declaration kinds and all-parts-partial rule are enforced atomically");
}

void partialMergeContract(TestContext& context, std::size_t variant)
{
    PartialTypeAggregator aggregator;
    std::string error;
    TypePartDescriptor first = samplePart(0U);
    TypePartDescriptor second = samplePart(1U);
    first.accessibility = DeclaredAccessibility::Public;
    first.baseTypeKey = "Game::Base";
    second.isSealed = true;
    if (variant == 3)
    {
        second.accessibility = DeclaredAccessibility::Internal;
    }
    if (variant == 4)
    {
        second.baseTypeKey = "Game::OtherBase";
    }
    if (variant == 5)
    {
        first.isAbstract = true;
    }
    if (variant == 6)
    {
        first.baseTypeKey.reset();
        second.isStatic = true;
    }
    aggregator.addPart(first, error);
    aggregator.addPart(second, error);
    const bool result = aggregator.finalize(error);
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    bool merged = result == expected;
    if (result)
    {
        const auto& type = aggregator.types().front();
        merged = merged && type.accessibility == DeclaredAccessibility::Public && type.isSealed &&
            type.baseTypeKey == std::optional<std::string>("Game::Base");
    }
    context.expect(merged, "CF-COMPILATION-006 accessibility, base, and modifiers merge without partial state");
}

void partialOrderingContract(TestContext& context, std::size_t variant)
{
    PartialTypeAggregator aggregator;
    std::string error;
    const std::size_t count = variant == 8 ? 512U : 3U;
    for (std::size_t reverse = count; reverse > 0U; --reverse)
    {
        aggregator.addPart(samplePart(reverse - 1U), error);
    }
    const bool first = aggregator.finalize(error);
    const bool second = aggregator.finalize(error);
    bool ordered = first && second && aggregator.isFinalized() && aggregator.types().size() == 1U &&
        aggregator.types().front().parts.size() == count;
    for (std::size_t index = 0; ordered && index < count; ++index)
    {
        ordered = aggregator.types().front().parts[index].sourceOrdinal == index;
    }
    if (variant == 3)
    {
        ordered = ordered && !aggregator.addPart(samplePart(count), error) && contains(error, "finalized");
    }
    context.expect(ordered, "CF-COMPILATION-007 finalization is canonical, frozen, and idempotent");
}

void projectAggregateContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("project_aggregate");
    SdkManifest sdk;
    sdk.name = "Windows";
    Project project = makeProject(
        tree.root() / "App.nsproj",
        "App",
        {tree.root() / "Core.nsproj"},
        {{tree.root() / "Core.lib", tree.root() / "Core.nsmeta", tree.root() / "Core.dll"}},
        {sdk},
        variant == 6 ? ProjectOutputKind::SharedLibrary : ProjectOutputKind::Executable,
        variant == 3 ? "" : "Pkg");
    const bool exact = project.projectFile() == tree.root() / "App.nsproj" &&
        project.moduleName() == "App" && project.projectReferences().size() == 1U &&
        project.libraryReferences().size() == 1U && project.sdkReferences().size() == 1U &&
        project.packageName() == (variant == 3 ? "App" : "Pkg") &&
        project.outputKind() == (variant == 6 ? ProjectOutputKind::SharedLibrary : ProjectOutputKind::Executable);
    context.expect(exact, "CF-COMPILATION-008 project aggregate preserves every constructor field and default");
}

void projectDumpContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("project_dump");
    Project project = makeProject(
        tree.root() / "App.nsproj",
        "App",
        {tree.root() / "Core.nsproj"},
        {},
        {},
        variant == 6 ? ProjectOutputKind::Library : ProjectOutputKind::Executable);
    std::ostringstream first;
    std::ostringstream second;
    ProjectDumper{}.dump(project, first);
    ProjectDumper{}.dump(project, second);
    const std::string text = first.str();
    context.expect(text == second.str() && contains(text, "project App\n") &&
            contains(text, variant == 6 ? "output: static-library" : "output: exe") &&
            contains(text, "project-references: 1"),
        "CF-COMPILATION-009 project diagnostic dump has stable labeled structure");
}

void graphRootContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("graph_root");
    const auto rootPath = tree.root() / (variant == 6 ? "APP.nsproj" : "App.nsproj");
    std::vector<Project> projects;
    if (variant != 3)
    {
        projects.push_back(makeProject(rootPath, "App"));
    }
    ProjectGraph graph(rootPath, std::move(projects));
    context.expect(graph.rootProjectFile() == rootPath &&
            ((variant == 3 && graph.rootProject() == nullptr) ||
             (variant != 3 && graph.rootProject() != nullptr && graph.rootProject()->moduleName() == "App")),
        "CF-COMPILATION-010 root identity follows the final graph project or remains absent for empty graph");
}

void graphLibraryClosureContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("graph_library");
    const auto appPath = tree.root() / "App.nsproj";
    ProjectGraph graph(appPath, {makeProject(appPath, "App")});
    std::string error;
    const LibraryReference reference{tree.root() / "Core.lib", tree.root() / "Core.nsmeta", {}};
    const auto owner = variant == 3 ? tree.root() / "Unknown.nsproj" : appPath;
    const std::vector<LibraryReference> references = variant == 5 ?
        std::vector<LibraryReference>{reference, reference} : std::vector<LibraryReference>{reference};
    const bool result = graph.addLibraryReferences(owner, references, error);
    if (variant == 3)
    {
        context.expect(!result && contains(error, "unknown owner"),
            "CF-COMPILATION-011 unknown closure owner is rejected");
        return;
    }
    context.expect(result && graph.rootProject()->libraryReferences().size() == 1U,
        "CF-COMPILATION-011 binary closure insertion deduplicates and preserves root identity");
}

void graphSourceReplacementContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("graph_replace");
    const auto corePath = tree.root() / "Core.nsproj";
    const auto appPath = tree.root() / "App.nsproj";
    const LibraryReference binary{tree.root() / "Core.lib", tree.root() / "Core.nsmeta", {}};
    ProjectGraph graph(appPath, {
        makeProject(corePath, "Core"),
        makeProject(appPath, "App", {}, {binary})});
    std::string error;
    const SourceLibraryReferenceReplacement replacement{
        variant == 3 ? tree.root() / "Unknown.nsproj" : appPath,
        binary.metadataPath,
        variant == 4 ? tree.root() / "Missing.nsproj" : corePath};
    const bool result = graph.replaceLibraryReferencesWithSourceProjects(
        std::span<const SourceLibraryReferenceReplacement>(&replacement, 1U), error);
    if (variant == 3 || variant == 4)
    {
        context.expect(!result && contains(error, "unknown"),
            "CF-COMPILATION-012 unknown owner or source rejects replacement atomically");
        return;
    }
    context.expect(result && equalProjectOrder(graph, {"Core", "App"}) &&
            graph.rootProject()->libraryReferences().empty() &&
            graph.rootProject()->projectReferences().size() == 1U,
        "CF-COMPILATION-012 source replacement removes binary edge and restores dependency order");
}

void graphPrecompiledContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("graph_precompiled");
    const auto appPath = tree.root() / "App.nsproj";
    const auto metadata = tree.write("Core.nsmeta", "metadata");
    ProjectGraph graph(appPath, {makeProject(appPath, "App")});
    std::string error;
    const PrecompiledProjectDescriptor descriptor{
        variant == 3 ? tree.root() / "Unknown.nsproj" : appPath,
        metadata,
        variant == 5 ? "App" : "Core"};
    std::vector<PrecompiledProjectDescriptor> descriptors{descriptor};
    if (variant == 4)
    {
        // ProjectGraph receives descriptors after metadata discovery. Exercise the invalid
        // relation it owns: one metadata path cannot identify two different modules.
        descriptors.push_back({appPath, metadata, "Graphics"});
    }
    const bool result = graph.addPrecompiledProjects(
        std::span<const PrecompiledProjectDescriptor>(descriptors.data(), descriptors.size()), error);
    if (variant == 3 || variant == 4 || variant == 5)
    {
        const std::string_view expectedError = variant == 3 ? "unknown owner project" :
            variant == 4 ? "assigned conflicting module identities" : "duplicate module name";
        context.expect(!result && contains(error, expectedError) &&
                graph.projects().size() == 1U && graph.rootProject() != nullptr &&
                graph.rootProject()->moduleName() == "App",
            "CF-COMPILATION-013 invalid graph relations are rejected without partial mutation");
        return;
    }
    context.expect(result && equalProjectOrder(graph, {"Core", "App"}) &&
            graph.projects().front().projectFile() == metadata,
        "CF-COMPILATION-013 metadata-backed project is inserted before the root with exact identity");
}

void graphDumpContract(TestContext& context, std::size_t)
{
    TemporaryTree tree("graph_dump");
    const auto corePath = tree.root() / "Core.nsproj";
    const auto appPath = tree.root() / "App.nsproj";
    ProjectGraph graph(appPath, {
        makeProject(corePath, "Core"),
        makeProject(appPath, "App", {corePath})});
    std::ostringstream first;
    std::ostringstream second;
    ProjectGraphDumper{}.dump(graph, first);
    ProjectGraphDumper{}.dump(graph, second);
    const std::string text = first.str();
    context.expect(text == second.str() && contains(text, "project-graph\n") &&
            contains(text, "[0] Core") && contains(text, "[1] App (root)") &&
            contains(text, "references: 1"),
        "CF-COMPILATION-014 graph dump preserves stable dependency-first order and root marker");
}
}

void registerCompilationCore001To112Module(TestRegistry& registry)
{
    addCompilationContract(registry, 1U, "unit", globNormalizationContract);
    addCompilationContract(registry, 2U, "unit", globSegmentContract);
    addCompilationContract(registry, 3U, "unit", hierarchicalGlobContract);
    addCompilationContract(registry, 4U, "unit", partialAdmissionContract);
    addCompilationContract(registry, 5U, "unit", partialKindContract);
    addCompilationContract(registry, 6U, "unit", partialMergeContract);
    addCompilationContract(registry, 7U, "unit", partialOrderingContract);
    addCompilationContract(registry, 8U, "unit", projectAggregateContract);
    addCompilationContract(registry, 9U, "unit", projectDumpContract);
    addCompilationContract(registry, 10U, "unit", graphRootContract);
    addCompilationContract(registry, 11U, "unit", graphLibraryClosureContract);
    addCompilationContract(registry, 12U, "unit", graphSourceReplacementContract);
    addCompilationContract(registry, 13U, "unit", graphPrecompiledContract);
    addCompilationContract(registry, 14U, "unit", graphDumpContract);
}
}
