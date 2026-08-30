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
using nativesharp::frontend::NamedTypeDeclaration;
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

void testNamespaceUsingAndAlias(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path core = root / "Core";
    const std::filesystem::path app = root / "App";
    writeProject(core, "Core");
    writeFile(
        core / "Types.ns",
        "namespace Shared.Models\n"
        "{\n"
        "    public struct Point { int X; int Y; }\n"
        "    public enum Mode { A, B }\n"
        "}\n");
    writeProject(
        app,
        "App",
        "<ProjectReference Include=\"../Core/Core.nsproj\" />");
    writeFile(
        app / "Main.ns",
        "using Shared.Models;\n"
        "using Models = Shared.Models;\n"
        "using PointAlias = Shared.Models.Point;\n"
        "namespace App.Main;\n"
        "struct Holder\n"
        "{\n"
        "    Point Value;\n"
        "    Mode ModeValue;\n"
        "    Models.Point QualifiedViaAlias;\n"
        "    PointAlias DirectAlias;\n"
        "}\n"
        "Point Echo(Point value);\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "App.nsproj", error);
    tests.expect(ok, "namespace/import/alias project binds successfully");
    tests.expect(error.empty(), "successful project name binding has no load error");
    tests.expect(!compilation.diagnostics().hasErrors(), "namespace/import/alias project is diagnostic-clean");
    tests.expect(compilation.nameLookup() != nullptr, "Stage 24 exposes a ProjectNameLookup result");
    if (compilation.nameLookup() == nullptr)
    {
        return;
    }

    const ProjectNameLookup& lookup = *compilation.nameLookup();
    tests.expect(findType(lookup, "Core::Shared.Models.Point") != nullptr,
        "dependency struct is predeclared with module-qualified identity");
    tests.expect(findType(lookup, "Core::Shared.Models.Mode") != nullptr,
        "dependency enum is predeclared in the namespace tree");
    tests.expect(findType(lookup, "App::App.Main.Holder") != nullptr,
        "file-scoped namespace contributes to the root module type identity");
    tests.expect(resolvedBindingCount(lookup, "Point", "Core::Shared.Models.Point") >= 3,
        "using namespace resolves unqualified Point across files and modules");
    tests.expect(resolvedBindingCount(lookup, "Mode", "Core::Shared.Models.Mode") >= 1,
        "using namespace resolves enum names");
    tests.expect(resolvedBindingCount(lookup, "Models.Point", "Core::Shared.Models.Point") == 1,
        "namespace alias resolves a qualified named type");
    tests.expect(resolvedBindingCount(lookup, "PointAlias", "Core::Shared.Models.Point") == 1,
        "type alias resolves directly to the dependency type");

    std::ostringstream dump;
    nativesharp::frontend::ProjectNameLookupDumper().dump(lookup, dump);
    tests.expect(dump.str().find("namespace Shared.Models") != std::string::npos,
        "name-lookup dump exposes dependency namespace hierarchy");
    tests.expect(dump.str().find("Core::Shared.Models.Point") != std::string::npos,
        "name-lookup dump exposes module-qualified type identity");
    tests.expect(dump.str().find("status=resolved") != std::string::npos,
        "name-lookup dump exposes resolved type bindings");
}

void testTransitiveVisibility(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path core = root / "TransitiveCore";
    const std::filesystem::path middle = root / "Middle";
    const std::filesystem::path app = root / "TransitiveApp";

    writeProject(core, "TransitiveCore");
    writeFile(core / "Core.ns", "namespace Deep; public struct Token { int Value; }\n");
    writeProject(
        middle,
        "Middle",
        "<ProjectReference Include=\"../TransitiveCore/TransitiveCore.nsproj\" />");
    writeFile(middle / "Middle.ns", "int MiddleValue() { return 1; }\n");
    writeProject(
        app,
        "TransitiveApp",
        "<ProjectReference Include=\"../Middle/Middle.nsproj\" />");
    writeFile(app / "App.ns", "using Deep; struct Holder { Token Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "TransitiveApp.nsproj", error);
    tests.expect(ok, "transitive ProjectReference visibility participates in type lookup");
    tests.expect(error.empty(), "transitive lookup has no loader error");
    if (compilation.nameLookup() != nullptr)
    {
        tests.expect(resolvedBindingCount(
                *compilation.nameLookup(),
                "Token",
                "TransitiveCore::Deep.Token") == 1,
            "root module resolves a type from a transitive dependency");
    }
}

void testSiblingIsolation(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path left = root / "Left";
    const std::filesystem::path right = root / "Right";
    const std::filesystem::path app = root / "SiblingApp";

    writeProject(left, "Left");
    writeFile(left / "Left.ns", "using Shared.Right; struct LeftHolder { RightType Value; }\n");
    writeProject(right, "Right");
    writeFile(right / "Right.ns", "namespace Shared.Right; public struct RightType { int Value; }\n");
    writeProject(
        app,
        "SiblingApp",
        "<ProjectReference Include=\"../Left/Left.nsproj\" />"
        "<ProjectReference Include=\"../Right/Right.nsproj\" />");
    writeFile(app / "App.ns", "int Main() { return 0; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "SiblingApp.nsproj", error);
    tests.expect(!ok, "a dependency cannot see a sibling merely because the root references both");
    tests.expect(error.empty(), "sibling visibility failure is semantic, not a load error");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::UnknownNamespace),
        "sibling-only namespace import is diagnosed as unknown");
}

void testAmbiguousReferencedType(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path left = root / "AmbLeft";
    const std::filesystem::path right = root / "AmbRight";
    const std::filesystem::path app = root / "AmbApp";

    writeProject(left, "AmbLeft");
    writeFile(left / "Widget.ns", "namespace Shared; public struct Widget { int A; }\n");
    writeProject(right, "AmbRight");
    writeFile(right / "Widget.ns", "namespace Shared; public struct Widget { int B; }\n");
    writeProject(
        app,
        "AmbApp",
        "<ProjectReference Include=\"../AmbLeft/AmbLeft.nsproj\" />"
        "<ProjectReference Include=\"../AmbRight/AmbRight.nsproj\" />");
    writeFile(app / "App.ns", "using Shared; struct Holder { Widget Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "AmbApp.nsproj", error);
    tests.expect(!ok, "same fully-qualified type from two referenced modules is ambiguous");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::AmbiguousTypeName),
        "ambiguous referenced type emits AmbiguousTypeName");
    if (compilation.nameLookup() != nullptr)
    {
        bool foundAmbiguous = false;
        for (const auto& binding : compilation.nameLookup()->bindings())
        {
            if (binding.writtenName == "Widget" && binding.kind == TypeNameBindingKind::Ambiguous &&
                binding.candidateTypeOrdinals.size() == 2)
            {
                foundAmbiguous = true;
            }
        }
        tests.expect(foundAmbiguous, "ambiguous binding preserves both deterministic candidates");
    }
}

void testCurrentModuleShadowsDependency(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path dependency = root / "ShadowDependency";
    const std::filesystem::path app = root / "ShadowApp";

    writeProject(dependency, "ShadowDependency");
    writeFile(dependency / "Widget.ns", "namespace Shared; public struct Widget { int A; }\n");
    writeProject(
        app,
        "ShadowApp",
        "<ProjectReference Include=\"../ShadowDependency/ShadowDependency.nsproj\" />");
    writeFile(
        app / "App.ns",
        "namespace Shared\n"
        "{\n"
        "    struct Widget { int B; }\n"
        "    struct Holder { Widget Value; }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "ShadowApp.nsproj", error);
    tests.expect(ok, "current module declaration shadows same namespace/type name in a dependency");
    if (compilation.nameLookup() != nullptr)
    {
        tests.expect(resolvedBindingCount(
                *compilation.nameLookup(),
                "Widget",
                "ShadowApp::Shared.Widget") == 1,
            "lexically matching type in current module wins before referenced modules");
    }
}

void testReferencedInternalTypeIsHidden(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path dependency = root / "VisibilityDependency";
    const std::filesystem::path app = root / "VisibilityApp";

    writeProject(dependency, "VisibilityDependency");
    writeFile(
        dependency / "Types.ns",
        "namespace Shared.Visibility;\n"
        "public struct Visible { int Value; }\n"
        "struct Secret { int Value; }\n");
    writeProject(
        app,
        "VisibilityApp",
        "<ProjectReference Include=\"../VisibilityDependency/VisibilityDependency.nsproj\" />");
    writeFile(
        app / "App.ns",
        "using Shared.Visibility;\n"
        "struct Holder { Visible A; Secret B; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "VisibilityApp.nsproj", error);
    tests.expect(!ok, "referenced internal top-level type is not exported across module boundary");
    tests.expect(error.empty(), "accessibility rejection is a name-binding diagnostic");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::UnknownTypeName),
        "hidden internal dependency type becomes unresolved to the referencing module");
    if (compilation.nameLookup() != nullptr)
    {
        tests.expect(resolvedBindingCount(
                *compilation.nameLookup(),
                "Visible",
                "VisibilityDependency::Shared.Visibility.Visible") == 1,
            "explicit public dependency type remains visible");
        tests.expect(resolvedBindingCount(
                *compilation.nameLookup(),
                "Secret",
                "VisibilityDependency::Shared.Visibility.Secret") == 0,
            "default-internal dependency type is filtered from referenced-module candidates");
    }
}

void testInvalidUsingAndDuplicates(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path invalid = root / "InvalidUsing";
    writeProject(invalid, "InvalidUsing");
    writeFile(
        invalid / "Main.ns",
        "using Missing.Namespace;\n"
        "using X = Missing.Type;\n"
        "using X = Missing.Other;\n"
        "struct Holder { UnknownType Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(invalid / "InvalidUsing.nsproj", error);
    tests.expect(!ok, "invalid imports and unresolved named type reject Stage 24 binding");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::UnknownNamespace),
        "unknown using namespace is diagnosed");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidUsingTarget),
        "invalid alias target is diagnosed");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateUsingAlias),
        "duplicate alias in one declaration scope is diagnosed");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::UnknownTypeName),
        "unresolved named type is diagnosed after syntax succeeds");
}

void testAmbiguousAliasTarget(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path left = root / "AliasLeft";
    const std::filesystem::path right = root / "AliasRight";
    const std::filesystem::path app = root / "AliasApp";

    writeProject(left, "AliasLeft");
    writeFile(left / "Types.ns", "namespace Shared; public struct Token { int A; }\n");
    writeProject(right, "AliasRight");
    writeFile(right / "Types.ns", "namespace Shared; public struct Token { int B; }\n");
    writeProject(
        app,
        "AliasApp",
        "<ProjectReference Include=\"../AliasLeft/AliasLeft.nsproj\" />"
        "<ProjectReference Include=\"../AliasRight/AliasRight.nsproj\" />");
    writeFile(
        app / "Main.ns",
        "using TokenAlias = Shared.Token;\n"
        "struct Holder { int Value; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "AliasApp.nsproj", error);
    tests.expect(!ok, "ambiguous using-alias target is rejected even when the alias is unused");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::AmbiguousTypeName),
        "ambiguous using-alias target emits AmbiguousTypeName during alias binding");
}

void testDuplicateTypeAcrossFiles(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "DuplicateType";
    writeProject(project, "DuplicateType");
    writeFile(project / "A.ns", "namespace Models; struct Player { int A; }\n");
    writeFile(project / "B.ns", "namespace Models; struct Player { int B; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(project / "DuplicateType.nsproj", error);
    tests.expect(!ok, "duplicate named type in the same module/namespace is rejected");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateSymbol),
        "duplicate named type emits DuplicateSymbol");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
        "duplicate named type retains previous declaration source location");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage24_project_name_lookup";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);

    testNamespaceUsingAndAlias(tests, root);
    testTransitiveVisibility(tests, root);
    testSiblingIsolation(tests, root);
    testAmbiguousReferencedType(tests, root);
    testCurrentModuleShadowsDependency(tests, root);
    testReferencedInternalTypeIsHidden(tests, root);
    testInvalidUsingAndDuplicates(tests, root);
    testAmbiguousAliasTarget(tests, root);
    testDuplicateTypeAcrossFiles(tests, root);

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
