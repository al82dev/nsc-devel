#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"
#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

    ProjectCompilation parallelCompilation;
    parallelCompilation.setCompilationJobs(4);
    std::string parallelError;
    const bool parallelOk = parallelCompilation.compileProjectNames(app / "App.nsproj", parallelError);
    tests.expect(parallelOk && parallelError.empty(),
        "four-worker per-file name indexing succeeds on the same project");
    std::ostringstream parallelDump;
    if (parallelCompilation.nameLookup() != nullptr)
    {
        nativesharp::frontend::ProjectNameLookupDumper().dump(
            *parallelCompilation.nameLookup(), parallelDump);
    }
    tests.expect(parallelDump.str() == dump.str(),
        "parallel declaration and binding indexes exactly match serial name lookup");
}

void testSourceLessFacadePreservesDependencyOwnership(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "FacadePackage";
    const std::filesystem::path facade = root / "Facade";
    const std::filesystem::path consumer = root / "FacadeConsumer";

    writeFile(
        package / "Types.ns",
        "namespace SharedFacade; public struct T { int Value; }\n");
    writeFile(
        package / "A.nsproj",
        "<Project><PropertyGroup><Name>AMod</Name><OutputType>Library</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"Types.ns\" /></ItemGroup></Project>\n");
    writeFile(
        facade / "U.nsproj",
        "<Project><PropertyGroup><Name>UMod</Name><OutputType>Library</OutputType>"
        "</PropertyGroup><ItemGroup><ProjectReference Include=\"../FacadePackage/A.nsproj\" />"
        "</ItemGroup></Project>\n");
    writeFile(
        consumer / "C.nsproj",
        "<Project><PropertyGroup><Name>CMod</Name><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><ProjectReference Include=\"../Facade/U.nsproj\" />"
        "</ItemGroup></Project>\n");
    writeFile(
        consumer / "Main.ns",
        "using SharedFacade; struct Holder { T Value; } int Main() { return 0; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(consumer / "C.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "a source-less facade exposes dependency types without ambiguity");
    if (compilation.projectGraph() != nullptr)
    {
        const auto projects = compilation.projectGraph()->projects();
        tests.expect(projects.size() == 3 && projects[0].moduleName() == "AMod" &&
                projects[0].sourceSet().sources().size() == 1 &&
                projects[1].moduleName() == "UMod" &&
                projects[1].sourceSet().sources().empty(),
            "the facade remains an empty module and the dependency retains source ownership");
    }
    if (compilation.nameLookup() != nullptr)
    {
        const ProjectNameLookup& lookup = *compilation.nameLookup();
        tests.expect(findType(lookup, "AMod::SharedFacade.T") != nullptr &&
                findType(lookup, "UMod::SharedFacade.T") == nullptr,
            "the facade does not redeclare its dependency type");
        tests.expect(resolvedBindingCount(
                lookup, "T", "AMod::SharedFacade.T") == 1,
            "the consumer resolves the dependency type exactly once through the facade");
    }
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

// NSC-LIMIT-009 -- LA IDENTIDAD DE UN TIPO ANIDADO Y QUIEN LO VE.
//
// Dos cosas que se miden aparte a proposito, porque son dos decisiones distintas:
//
//   * La IDENTIDAD canonica lleva `+` --`Ns.Outer+Item`-- y la produce TypeIdentity. El nombre
//     FUENTE lleva punto --`Outer.Item`-- y vive en un indice aparte. Colapsar los dos volveria
//     indistinguibles el anidado `Ns.Outer.Item` y el tipo `Item` del namespace `Ns.Outer`.
//   * La ACCESIBILIDAD por omision depende del sitio: un tipo de namespace sin modificador es
//     internal, uno anidado es private. Convertir el segundo en el primero publicaria dentro del
//     modulo cada tipo interno que alguien escribiera para uso propio.
void testNestedTypeIdentity(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "NestedIdentity";
    writeProject(project, "NestedIdentity");
    writeFile(
        project / "Types.ns",
        "namespace Ns\n"
        "{\n"
        "    public class Outer\n"
        "    {\n"
        "        public struct Item { public int a; }\n"
        "        private struct Secreta { public int b; }\n"
        "        struct SinModificador { public int c; }\n"
        "        public int Usar()\n"
        "        {\n"
        "            Item i; i.a = 1;\n"
        "            Secreta s; s.b = 2;\n"
        "            SinModificador m; m.c = 3;\n"
        "            return i.a + s.b + m.c;\n"
        "        }\n"
        "    }\n"
        "    public class Otro\n"
        "    {\n"
        "        public struct Item { public int d; }\n"
        "    }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(project / "NestedIdentity.nsproj", error);
    tests.expect(ok, "a project with nested types binds successfully");
    const ProjectNameLookup* lookup = compilation.nameLookup();
    tests.expect(lookup != nullptr, "nested type project produces a name lookup");
    if (lookup == nullptr)
    {
        return;
    }

    const NamedTypeDeclaration* item = findType(*lookup, "NestedIdentity::Ns.Outer+Item");
    const NamedTypeDeclaration* secreta = findType(*lookup, "NestedIdentity::Ns.Outer+Secreta");
    const NamedTypeDeclaration* sinModificador =
        findType(*lookup, "NestedIdentity::Ns.Outer+SinModificador");
    const NamedTypeDeclaration* otroItem = findType(*lookup, "NestedIdentity::Ns.Otro+Item");
    const NamedTypeDeclaration* outer = findType(*lookup, "NestedIdentity::Ns.Outer");

    tests.expect(item != nullptr, "a nested type's canonical identity uses '+', not '.'");
    tests.expect(otroItem != nullptr,
        "the same nested name under a second container is a different type");
    tests.expect(findType(*lookup, "NestedIdentity::Ns.Outer.Item") == nullptr,
        "the dotted source spelling is NOT a canonical key");

    if (item != nullptr && outer != nullptr)
    {
        tests.expect(item->containingTypeOrdinal.has_value(),
            "a nested type records the ordinal of its container");
        tests.expect(
            item->identity.containingTypes().size() == 1 &&
                item->identity.containingTypes().front().name == "Outer" &&
                item->identity.containingTypes().front().arity == 0,
            "the containing-type chain carries the container's name and its arity");
        tests.expect(outer->containingTypeOrdinal == std::optional<std::size_t>(),
            "a namespace-scope type has no container");
    }

    tests.expect(item != nullptr &&
            item->accessibility == nativesharp::frontend::NamedTypeAccessibility::Public,
        "an explicit public survives on a nested type");
    tests.expect(secreta != nullptr &&
            secreta->accessibility == nativesharp::frontend::NamedTypeAccessibility::Private,
        "an explicit private survives on a nested type");
    tests.expect(sinModificador != nullptr &&
            sinModificador->accessibility == nativesharp::frontend::NamedTypeAccessibility::Private,
        "a nested type with no accessibility modifier defaults to private, not to module internal");
    tests.expect(outer != nullptr &&
            outer->accessibility == nativesharp::frontend::NamedTypeAccessibility::Public,
        "the containing type keeps its own accessibility");

    // Y el nombre corto escrito DENTRO del contenedor resuelve al anidado: es el escalon de
    // busqueda que se anadio delante del namespace, y sin el habria que escribir `Outer.Item`
    // dentro del propio `Outer`.
    tests.expect(resolvedBindingCount(*lookup, "Item", "NestedIdentity::Ns.Outer+Item") >= 1,
        "the short name inside the container resolves to its own nested type");
}

// La forma fuente de un anidado usa puntos y puede coincidir byte a byte con un tipo de
// namespace: Collision.Outer+Item frente a Collision.Outer.Item. Dentro de Outer, el escalon
// del contenedor tiene prioridad y debe elegir exclusivamente el primero.
void testNestedTypeShadowsHomonymousNamespaceType(
    TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "NestedNamespaceCollision";
    writeProject(project, "NestedNamespaceCollision");
    writeFile(
        project / "Types.ns",
        "namespace Collision.Outer\n"
        "{\n"
        "    public class Item { }\n"
        "}\n"
        "namespace Collision\n"
        "{\n"
        "    public class Outer\n"
        "    {\n"
        "        public class Item { }\n"
        "        public Item Select(Item value)\n"
        "        {\n"
        "            Item local = value;\n"
        "            return local;\n"
        "        }\n"
        "    }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok =
        compilation.compileProjectNames(project / "NestedNamespaceCollision.nsproj", error);
    tests.expect(ok, "a nested type may share its dotted source spelling with a namespace type");
    const ProjectNameLookup* lookup = compilation.nameLookup();
    tests.expect(lookup != nullptr, "the nested/namespace collision produces a name lookup");
    if (lookup == nullptr)
    {
        return;
    }

    tests.expect(
        findType(*lookup, "NestedNamespaceCollision::Collision.Outer+Item") != nullptr &&
            findType(*lookup, "NestedNamespaceCollision::Collision.Outer.Item") != nullptr,
        "the nested and namespace types retain distinct canonical identities");
    tests.expect(
        resolvedBindingCount(
            *lookup, "Item", "NestedNamespaceCollision::Collision.Outer+Item") >= 3,
        "inside Outer, every short Item binding selects Outer+Item");
    tests.expect(
        resolvedBindingCount(
            *lookup, "Item", "NestedNamespaceCollision::Collision.Outer.Item") == 0,
        "the homonymous namespace type cannot steal the enclosing-type lookup rung");
}

// NSC-LIMIT-009 -- QUE CRUZA EL LIMITE DE PROYECTO, que son TODOS los eslabones o ninguno.
void testNestedTypeAcrossProjects(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path library = root / "NestedLib";
    const std::filesystem::path app = root / "NestedApp";
    writeProject(library, "NestedLib");
    writeFile(
        library / "Lib.ns",
        "namespace NestedShared\n"
        "{\n"
        "    public class Publica\n"
        "    {\n"
        "        public struct Visible { public int v; }\n"
        "        internal struct Interna { public int i; }\n"
        "    }\n"
        "    internal class Reservada\n"
        "    {\n"
        "        public struct Dentro { public int d; }\n"
        "    }\n"
        "}\n");

    writeProject(app, "NestedApp", "<ProjectReference Include=\"../NestedLib/NestedLib.nsproj\" />");
    writeFile(
        app / "Main.ns",
        "using NestedShared;\n"
        "namespace NestedConsumer\n"
        "{\n"
        "    public class P\n"
        "    {\n"
        "        public int Usar()\n"
        "        {\n"
        "            Publica.Visible v; v.v = 1;\n"
        "            NestedShared.Publica.Visible w; w.v = 2;\n"
        "            return v.v + w.v;\n"
        "        }\n"
        "    }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectNames(app / "NestedApp.nsproj", error);
    tests.expect(ok, "a public nested type of a public container crosses a ProjectReference");
    const ProjectNameLookup* lookup = compilation.nameLookup();
    if (lookup != nullptr)
    {
        tests.expect(
            resolvedBindingCount(*lookup, "Publica.Visible", "NestedLib::NestedShared.Publica+Visible") >= 1,
            "'using' plus the container-qualified source name reaches the nested type");
        tests.expect(
            resolvedBindingCount(
                *lookup,
                "NestedShared.Publica.Visible",
                "NestedLib::NestedShared.Publica+Visible") >= 1,
            "the fully qualified source name reaches the same nested type");
    }

    // El anidado internal de un contenedor public NO cruza...
    const std::filesystem::path internalApp = root / "NestedInternalApp";
    writeProject(
        internalApp,
        "NestedInternalApp",
        "<ProjectReference Include=\"../NestedLib/NestedLib.nsproj\" />");
    writeFile(
        internalApp / "Main.ns",
        "using NestedShared;\n"
        "namespace NestedConsumer\n"
        "{\n"
        "    public class P { public int Usar() { Publica.Interna v; v.i = 1; return v.i; } }\n"
        "}\n");
    ProjectCompilation internalCompilation;
    tests.expect(!internalCompilation.compileProjectNames(
                     internalApp / "NestedInternalApp.nsproj", error),
        "an internal nested type does not cross a ProjectReference");

    // ...y el anidado PUBLIC de un contenedor internal tampoco, que es el caso que una
    // comprobacion de un solo nivel habria dejado pasar: el `public` del anidado es cierto, y aun
    // asi el tipo no es alcanzable, porque su contenedor no sale del modulo.
    const std::filesystem::path hiddenApp = root / "NestedHiddenApp";
    writeProject(
        hiddenApp,
        "NestedHiddenApp",
        "<ProjectReference Include=\"../NestedLib/NestedLib.nsproj\" />");
    writeFile(
        hiddenApp / "Main.ns",
        "using NestedShared;\n"
        "namespace NestedConsumer\n"
        "{\n"
        "    public class P { public int Usar() { Reservada.Dentro v; v.d = 1; return v.d; } }\n"
        "}\n");
    ProjectCompilation hiddenCompilation;
    tests.expect(!hiddenCompilation.compileProjectNames(hiddenApp / "NestedHiddenApp.nsproj", error),
        "a public nested type of an internal container does not cross a ProjectReference");
}

// NSC-LIMIT-009, segunda vuelta -- `protected` ENTRE PROYECTOS.
//
// Es el caso normal de una biblioteca: `protected class Reservada` existe para que las derivadas
// la usen, y una derivada suele estar en otro proyecto. El filtro de cruce de modulo exigia
// `public` en toda la cadena, de modo que el nombre no llegaba a resolverse y la comprobacion de
// herencia --la unica que sabe decidirlo-- no recibia la oportunidad.
//
// LAS DOS PRUEBAS LLEGAN HASTA `compileProjectInheritance`, no se paran en los nombres: la
// decision depende del grafo de herencia, y pararse antes mediria otra cosa.
void testNestedProtectedAcrossProjects(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path library = root / "ProtLib";
    writeProject(library, "ProtLib");
    writeFile(
        library / "Lib.ns",
        "namespace ProtShared\n"
        "{\n"
        "    public class Base\n"
        "    {\n"
        "        protected class Reservada { public int Codigo() { return 40; } }\n"
        "        public int Propio() { Reservada r = new Reservada(); return r.Codigo(); }\n"
        "    }\n"
        "}\n");

    const std::filesystem::path derived = root / "ProtDerived";
    writeProject(derived, "ProtDerived", "<ProjectReference Include=\"../ProtLib/ProtLib.nsproj\" />");
    writeFile(
        derived / "Main.ns",
        "using ProtShared;\n"
        "namespace ProtApp\n"
        "{\n"
        "    public class Derivada : Base\n"
        "    {\n"
        "        public int Usar() { Base.Reservada r = new Base.Reservada(); return r.Codigo(); }\n"
        "    }\n"
        "}\n");

    ProjectCompilation derivedCompilation;
    std::string error;
    tests.expect(
        derivedCompilation.compileProjectInheritance(derived / "ProtDerived.nsproj", error),
        "a protected nested type is reachable from a derived type in another project");

    const std::filesystem::path stranger = root / "ProtStranger";
    writeProject(
        stranger, "ProtStranger", "<ProjectReference Include=\"../ProtLib/ProtLib.nsproj\" />");
    writeFile(
        stranger / "Main.ns",
        "using ProtShared;\n"
        "namespace ProtApp\n"
        "{\n"
        "    public class Ajena\n"
        "    {\n"
        "        public int Usar() { Base.Reservada r = new Base.Reservada(); return r.Codigo(); }\n"
        "    }\n"
        "}\n");

    ProjectCompilation strangerCompilation;
    tests.expect(
        !strangerCompilation.compileProjectInheritance(stranger / "ProtStranger.nsproj", error),
        "the same protected nested type is NOT reachable from an unrelated type in another project");
    tests.expect(hasDiagnostic(strangerCompilation, DiagnosticId::UnknownTypeName),
        "the cross-project protected rejection is a type-name diagnostic");
}

// NSC-LIMIT-009 -- `private` visto desde fuera del contenedor.
void testNestedPrivateIsHidden(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = root / "NestedPrivate";
    writeProject(project, "NestedPrivate");
    writeFile(
        project / "Types.ns",
        "namespace NestedPriv\n"
        "{\n"
        "    public class Caja { private struct Secreta { public int s; } }\n"
        "    public class Fuera { public int Usar() { Caja.Secreta s; s.s = 1; return s.s; } }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    tests.expect(!compilation.compileProjectNames(project / "NestedPrivate.nsproj", error),
        "a private nested type is not reachable from outside its container");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::UnknownTypeName),
        "the private nested rejection is a type-name diagnostic");
    // Y el mensaje no puede ser «no existe»: el tipo esta escrito dos lineas mas arriba, y decir
    // que no existe manda al lector a buscar una errata que no hay.
    bool saysProtectionLevel = false;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find("is private and is not accessible here") != std::string::npos)
        {
            saysProtectionLevel = true;
        }
    }
    tests.expect(saysProtectionLevel,
        "the diagnostic names the accessibility, not a missing type");
}
}

// NSC-LIMIT-007 -- LA ESCALA, sola y sin tablas.
//
// `qualifiedNameLadder` es lo unico que este modelo comparte con la fase generica, y por eso tiene
// prueba PROPIA: si el orden cambia aqui, cambia en las dos fases a la vez, que es exactamente la
// propiedad que se busca. Comprobar el orden solo a traves de un programa compilado lo mediria de
// refilon y no diria en que escalon esta el fallo.
void testQualifiedNameLadderOrder(TestContext& tests)
{
    using nativesharp::frontend::QualifiedNameSite;
    using nativesharp::frontend::QualifiedNameTier;
    using nativesharp::frontend::qualifiedNameLadder;

    const auto spellings = [](const auto& rungs)
    {
        std::vector<std::string> flat;
        for (const auto& rung : rungs)
            for (const std::string& spelling : rung.spellings) flat.push_back(spelling);
        return flat;
    };

    // NOMBRE SIMPLE DENTRO DE `A.B`. El namespace mas interno primero, despues el exterior, y el
    // global DESPUES de los imports -- que es la diferencia con la rama calificada.
    {
        const std::vector<std::string> segments{"A", "B"};
        QualifiedNameSite site;
        site.namespaceSegments = segments;
        const std::vector<std::string> flat = spellings(qualifiedNameLadder(site, "C"));
        tests.expect(flat.size() >= 3 && flat[0] == "A.B.C" && flat[1] == "A.C" && flat[2] == "C",
            "ladder: a simple name tries the innermost namespace first, then outward, then global");
    }

    // LOS CONTENEDORES VAN PRIMERO, antes que cualquier namespace: dentro de `A.Outer`, `Helper`
    // es `A.Outer.Helper` antes que `A.Helper`. Es el escalon que faltaba entero en la fase
    // generica, y el que decide el caso 18 de c46.
    {
        const std::vector<std::string> segments{"A"};
        const std::vector<std::string> enclosing{"A.Outer"};
        QualifiedNameSite site;
        site.namespaceSegments = segments;
        site.enclosingTypeNames = enclosing;
        const auto rungs = qualifiedNameLadder(site, "Helper");
        tests.expect(!rungs.empty() && rungs.front().tier == QualifiedNameTier::EnclosingType &&
                rungs.front().spellings.size() == 1 &&
                rungs.front().spellings.front() == "A.Outer.Helper",
            "ladder: an enclosing type outranks every namespace tier");
    }

    // DE DENTRO AFUERA TAMBIEN EN LA CADENA DE CONTENEDORES.
    {
        const std::vector<std::string> enclosing{"A.Outer", "A.Outer.Medio"};
        QualifiedNameSite site;
        site.enclosingTypeNames = enclosing;
        const std::vector<std::string> flat = spellings(qualifiedNameLadder(site, "Item"));
        tests.expect(flat.size() >= 2 && flat[0] == "A.Outer.Medio.Item" &&
                flat[1] == "A.Outer.Item",
            "ladder: the innermost containing type is tried first");
    }

    // UN ALIAS SUSTITUYE Y CIERRA: `X.C` con `using X = P.Q` no vuelve a buscarse como `X.C`.
    {
        const std::unordered_map<std::string, std::string> aliases{{"X", "P.Q"}};
        QualifiedNameSite site;
        site.aliases = &aliases;
        const std::vector<std::string> flat = spellings(qualifiedNameLadder(site, "X.C"));
        tests.expect(!flat.empty() && flat.front() == "P.Q.C",
            "ladder: an alias substitutes its target");
        bool sinSustituir = false;
        for (const std::string& spelling : flat)
            if (spelling == "X.C") sinSustituir = true;
        tests.expect(!sinSustituir,
            "ladder: and the written alias name is never tried on its own");
    }

    // LOS IMPORTS SON UN SOLO ESCALON: dos candidatos ahi son ambiguos, no se elige uno.
    {
        const std::vector<std::string> imports{"P", "Q"};
        QualifiedNameSite site;
        site.importedNamespaces = imports;
        const auto rungs = qualifiedNameLadder(site, "C");
        bool escalonUnico = false;
        for (const auto& rung : rungs)
        {
            if (rung.tier != QualifiedNameTier::ImportedNamespace) continue;
            escalonUnico = rung.spellings.size() == 2 && rung.spellings[0] == "P.C" &&
                rung.spellings[1] == "Q.C";
        }
        tests.expect(escalonUnico,
            "ladder: every imported namespace competes inside ONE tier");
    }

    // UN NOMBRE CON PUNTOS baja hasta el prefijo VACIO dentro del mismo escalon lexico -- no tiene
    // escalon «global» aparte, porque su ultimo prefijo ya lo es.
    {
        const std::vector<std::string> segments{"A", "B"};
        QualifiedNameSite site;
        site.namespaceSegments = segments;
        const std::vector<std::string> flat = spellings(qualifiedNameLadder(site, "N1.C"));
        tests.expect(flat.size() >= 3 && flat[0] == "A.B.N1.C" && flat[1] == "A.N1.C" &&
                flat[2] == "N1.C",
            "ladder: a dotted name is prefixed by each lexical namespace, ending at none");
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
    testSourceLessFacadePreservesDependencyOwnership(tests, root);
    testSiblingIsolation(tests, root);
    testAmbiguousReferencedType(tests, root);
    testCurrentModuleShadowsDependency(tests, root);
    testReferencedInternalTypeIsHidden(tests, root);
    testInvalidUsingAndDuplicates(tests, root);
    testAmbiguousAliasTarget(tests, root);
    testDuplicateTypeAcrossFiles(tests, root);
    testNestedTypeIdentity(tests, root);
    testNestedTypeShadowsHomonymousNamespaceType(tests, root);
    testNestedTypeAcrossProjects(tests, root);
    testNestedPrivateIsHidden(tests, root);
    testNestedProtectedAcrossProjects(tests, root);
    testQualifiedNameLadderOrder(tests);

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
