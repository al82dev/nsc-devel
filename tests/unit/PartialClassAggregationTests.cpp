#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::NamedTypeAccessibility;
using nativesharp::frontend::NamedTypeDeclaration;
using nativesharp::frontend::NamedTypeDeclarationKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectNameLookup;
using nativesharp::frontend::TypeNameBindingKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view references = {})
{
    std::string project = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup>";
    if (!references.empty())
    {
        project += "<ItemGroup>";
        project += references;
        project += "</ItemGroup>";
    }
    project += "</Project>\n";
    writeFile(directory / (std::string(name) + ".nsproj"), project);
}

bool hasDiagnostic(const ProjectCompilation& compilation, DiagnosticId id)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id)
        {
            return true;
        }
    }
    return false;
}

const NamedTypeDeclaration* findType(
    const ProjectNameLookup& lookup,
    std::string_view key)
{
    for (const NamedTypeDeclaration& declaration : lookup.namedTypes())
    {
        if (declaration.identity.key() == key)
        {
            return &declaration;
        }
    }
    return nullptr;
}

std::size_t countType(const ProjectNameLookup& lookup, std::string_view key)
{
    std::size_t count = 0;
    for (const NamedTypeDeclaration& declaration : lookup.namedTypes())
    {
        if (declaration.identity.key() == key)
        {
            ++count;
        }
    }
    return count;
}

std::size_t resolvedBindingCount(
    const ProjectNameLookup& lookup,
    std::string_view writtenName,
    std::string_view targetKey)
{
    std::size_t count = 0;
    for (const auto& binding : lookup.bindings())
    {
        if (binding.writtenName != writtenName ||
            binding.kind != TypeNameBindingKind::Resolved ||
            !binding.targetTypeOrdinal.has_value())
        {
            continue;
        }
        if (lookup.namedTypes()[*binding.targetTypeOrdinal].identity.key() == targetKey)
        {
            ++count;
        }
    }
    return count;
}

void testCrossFileAggregation(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "CrossFile";
    writeProject(project, "CrossFile");
    writeFile(project / "Z_State.ns", "namespace Game.Model; partial class Widget {}\n");
    writeFile(project / "A_Public.ns", "namespace Game.Model; public partial class Widget {}\n");
    writeFile(project / "M_Actions.ns", "namespace Game.Model; partial class Widget {}\n");
    writeFile(project / "Use.ns", "using Game.Model; struct Holder { Widget Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(project / "CrossFile.nsproj", error);
    tests.expect(ok, "three compatible partial class files aggregate successfully");
    tests.expect(error.empty(), "cross-file partial aggregation has no project-loader error");
    tests.expect(!compilation.diagnostics().hasErrors(),
        "compatible partial class aggregation is diagnostic-clean");
    if (compilation.nameLookup() == nullptr)
    {
        tests.expect(false, "cross-file aggregation produces a ProjectNameLookup");
        return;
    }

    const ProjectNameLookup& lookup = *compilation.nameLookup();
    const NamedTypeDeclaration* widget = findType(lookup, "CrossFile::Game.Model.Widget");
    tests.expect(widget != nullptr, "aggregated class has one module-qualified TypeIdentity");
    tests.expect(countType(lookup, "CrossFile::Game.Model.Widget") == 1,
        "three partial declarations publish exactly one named type");
    if (widget != nullptr)
    {
        tests.expect(widget->kind == NamedTypeDeclarationKind::Class,
            "aggregated named type retains class kind");
        tests.expect(widget->isPartial, "aggregated type records that it comes from partial declarations");
        tests.expect(widget->accessibility == NamedTypeAccessibility::Public,
            "one explicit public modifier establishes merged public accessibility");
        tests.expect(widget->parts.size() == 3,
            "aggregated class preserves all three declaration parts");
        if (widget->parts.size() == 3)
        {
            tests.expect(widget->parts[0].sourceOrdinal < widget->parts[1].sourceOrdinal &&
                    widget->parts[1].sourceOrdinal < widget->parts[2].sourceOrdinal,
                "partial declaration parts are published in canonical source order");
            tests.expect(widget->parts[0].sourcePath.find("A_Public.ns") != std::string::npos,
                "canonical aggregation order is independent of creation order and starts with A_Public.ns");
        }
    }
    tests.expect(resolvedBindingCount(lookup, "Widget", "CrossFile::Game.Model.Widget") == 1,
        "unqualified using-namespace lookup binds to the aggregated class");

    std::ostringstream dump;
    nativesharp::frontend::ProjectNameLookupDumper().dump(lookup, dump);
    tests.expect(dump.str().find("class Game.Model.Widget") != std::string::npos,
        "name-lookup dump exposes class kind");
    tests.expect(dump.str().find("parts=3") != std::string::npos,
        "name-lookup dump exposes aggregated part count");
    tests.expect(dump.str().find("A_Public.ns") != std::string::npos,
        "name-lookup dump preserves declaration source paths");
}

void testSameFileAggregation(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "SameFile";
    writeProject(project, "SameFile");
    writeFile(
        project / "Parts.ns",
        "namespace Demo;\n"
        "partial class Local {}\n"
        "partial class Local {}\n"
        "struct Holder { Local Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(project / "SameFile.nsproj", error);
    tests.expect(ok, "two compatible partial class declarations in one file aggregate successfully");
    tests.expect(error.empty(), "same-file partial aggregation has no project-loader error");
    if (compilation.nameLookup() == nullptr)
    {
        tests.expect(false, "same-file aggregation produces a ProjectNameLookup");
        return;
    }

    const ProjectNameLookup& lookup = *compilation.nameLookup();
    const NamedTypeDeclaration* local = findType(lookup, "SameFile::Demo.Local");
    tests.expect(local != nullptr, "same-file partial declarations publish one named type identity");
    tests.expect(countType(lookup, "SameFile::Demo.Local") == 1,
        "same-file partial declarations do not publish duplicate named types");
    if (local != nullptr)
    {
        tests.expect(local->isPartial, "same-file aggregate retains partial status");
        tests.expect(local->parts.size() == 2, "same-file aggregate preserves both declaration parts");
        if (local->parts.size() == 2)
        {
            tests.expect(local->parts[0].sourceOrdinal == local->parts[1].sourceOrdinal,
                "same-file declaration parts share one source ordinal");
            tests.expect(local->parts[0].range.begin().offset() < local->parts[1].range.begin().offset(),
                "same-file declaration parts remain distinct by source offset");
        }
    }
    tests.expect(resolvedBindingCount(lookup, "Local", "SameFile::Demo.Local") == 1,
        "a later declaration in the same file binds to the aggregated class");
}

void testUsingAliasAndDependencyVisibility(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path core = root / "ClassCore";
    const std::filesystem::path app = root / "ClassApp";
    writeProject(core, "ClassCore");
    writeFile(
        core / "Types.ns",
        "namespace Shared.Types;\n"
        "public class Visible {}\n"
        "class Hidden {}\n");
    writeProject(
        app,
        "ClassApp",
        "<ProjectReference Include=\"../ClassCore/ClassCore.nsproj\" />");
    writeFile(
        app / "Main.ns",
        "using Shared.Types;\n"
        "using Alias = Shared.Types.Visible;\n"
        "struct Holder { Visible A; Shared.Types.Visible B; Alias AliasValue; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "ClassApp.nsproj", error);
    tests.expect(ok, "public dependency class participates in namespace, qualified and alias lookup");
    if (compilation.nameLookup() != nullptr)
    {
        const ProjectNameLookup& lookup = *compilation.nameLookup();
        tests.expect(resolvedBindingCount(lookup, "Visible", "ClassCore::Shared.Types.Visible") == 1,
            "using namespace resolves public dependency class");
        tests.expect(resolvedBindingCount(
                lookup,
                "Shared.Types.Visible",
                "ClassCore::Shared.Types.Visible") == 1,
            "qualified class lookup crosses ProjectReference");
        tests.expect(resolvedBindingCount(lookup, "Alias", "ClassCore::Shared.Types.Visible") == 1,
            "using alias resolves directly to a class type");
    }

    const std::filesystem::path hiddenApp = root / "HiddenClassApp";
    writeProject(
        hiddenApp,
        "HiddenClassApp",
        "<ProjectReference Include=\"../ClassCore/ClassCore.nsproj\" />");
    writeFile(
        hiddenApp / "Main.ns",
        "using Shared.Types; struct Holder { Hidden Value; }\n");
    ProjectCompilation hiddenCompilation;
    const bool hiddenOk = hiddenCompilation.compileProjectNames(
        hiddenApp / "HiddenClassApp.nsproj",
        error);
    tests.expect(!hiddenOk, "default-internal dependency class is hidden across ProjectReference");
    tests.expect(hasDiagnostic(hiddenCompilation, DiagnosticId::UnknownTypeName),
        "hidden dependency class becomes an unknown type in the referencing module");
}

void testNamespaceAndModuleIdentityIsolation(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path namespaces = root / "NamespaceIsolation";
    writeProject(namespaces, "NamespaceIsolation");
    writeFile(namespaces / "Left.ns", "namespace Left; partial class Widget {}\n");
    writeFile(namespaces / "Right.ns", "namespace Right; partial class Widget {}\n");

    ProjectCompilation namespaceCompilation;
    std::string error;
    const bool namespaceOk = namespaceCompilation.compileProjectNames(
        namespaces / "NamespaceIsolation.nsproj",
        error);
    tests.expect(namespaceOk, "same class spelling in distinct namespaces does not merge");
    if (namespaceCompilation.nameLookup() != nullptr)
    {
        const ProjectNameLookup& lookup = *namespaceCompilation.nameLookup();
        tests.expect(findType(lookup, "NamespaceIsolation::Left.Widget") != nullptr,
            "left namespace receives its own Widget identity");
        tests.expect(findType(lookup, "NamespaceIsolation::Right.Widget") != nullptr,
            "right namespace receives a distinct Widget identity");
    }

    const std::filesystem::path dependency = root / "ModuleA";
    const std::filesystem::path app = root / "ModuleB";
    writeProject(dependency, "ModuleA");
    writeFile(dependency / "Widget.ns", "public partial class Widget {}\n");
    writeProject(app, "ModuleB", "<ProjectReference Include=\"../ModuleA/ModuleA.nsproj\" />");
    writeFile(app / "Widget.ns", "partial class Widget {}\nstruct Holder { Widget Value; }\n");

    ProjectCompilation moduleCompilation;
    const bool moduleOk = moduleCompilation.compileProjectNames(app / "ModuleB.nsproj", error);
    tests.expect(moduleOk, "same partial class spelling in different modules remains two types");
    if (moduleCompilation.nameLookup() != nullptr)
    {
        const ProjectNameLookup& lookup = *moduleCompilation.nameLookup();
        tests.expect(findType(lookup, "ModuleA::Widget") != nullptr,
            "dependency module owns its Widget identity");
        tests.expect(findType(lookup, "ModuleB::Widget") != nullptr,
            "root module owns a separate Widget identity");
        tests.expect(resolvedBindingCount(lookup, "Widget", "ModuleB::Widget") == 1,
            "current-module class shadows an equally named public dependency class");
    }
}

void expectConflict(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view firstSource,
    std::string_view secondSource,
    DiagnosticId expectedDiagnostic)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName);
    writeFile(directory / "A.ns", firstSource);
    writeFile(directory / "B.ns", secondSource);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(
        directory / (std::string(caseName) + ".nsproj"),
        error);
    tests.expect(!ok, std::string(caseName) + " conflict rejects project name binding");
    tests.expect(error.empty(), std::string(caseName) + " conflict is semantic, not a loader failure");
    tests.expect(hasDiagnostic(compilation, expectedDiagnostic),
        std::string(caseName) + " emits the expected primary diagnostic");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
        std::string(caseName) + " preserves a note to the previous declaration");
}

void testMandatoryConflicts(TestContext& tests, const std::filesystem::path& root)
{
    expectConflict(
        tests,
        root,
        "ClassStructCollision",
        "class Collision {}\n",
        "struct Collision { int X; }\n",
        DiagnosticId::PartialTypeConflict);
    expectConflict(
        tests,
        root,
        "ClassEnumCollision",
        "class Collision {}\n",
        "enum Collision { A }\n",
        DiagnosticId::PartialTypeConflict);
    expectConflict(
        tests,
        root,
        "PartialNonPartial",
        "partial class Mixed {}\n",
        "class Mixed {}\n",
        DiagnosticId::PartialTypeConflict);
    expectConflict(
        tests,
        root,
        "AccessibilityConflict",
        "public partial class Access {}\n",
        "internal partial class Access {}\n",
        DiagnosticId::PartialTypeConflict);
    expectConflict(
        tests,
        root,
        "DuplicateNonPartial",
        "class Duplicate {}\n",
        "class Duplicate {}\n",
        DiagnosticId::DuplicateSymbol);
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage25_partial_classes";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);

    testCrossFileAggregation(tests, root);
    testSameFileAggregation(tests, root);
    testUsingAliasAndDependencyVisibility(tests, root);
    testNamespaceAndModuleIdentityIsolation(tests, root);
    testMandatoryConflicts(tests, root);

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
