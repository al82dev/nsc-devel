#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMemberModelDumper.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::frontend::ClassMemberAccessibility;
using nativesharp::frontend::ClassMemberKind;
using nativesharp::frontend::ClassMemberSymbol;
using nativesharp::frontend::MemberBodyBindingKind;
using nativesharp::frontend::NamedTypeDeclaration;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectMemberModel;
using nativesharp::frontend::ProjectNameLookup;
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
    std::string text = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />";
    text += references;
    text += "</ItemGroup></Project>";
    writeFile(directory / (std::string(name) + ".nsproj"), text);
}

bool hasDiagnostic(const ProjectCompilation& compilation, DiagnosticId id)
{
    for (const nativesharp::diagnostics::Diagnostic& diagnostic :
         compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() == id)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::size_t> findTypeOrdinal(
    const ProjectNameLookup& lookup,
    std::string_view key)
{
    for (std::size_t ordinal = 0; ordinal < lookup.namedTypes().size(); ++ordinal)
    {
        if (lookup.namedTypes()[ordinal].identity.key() == key)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

const ClassMemberSymbol* findMember(
    const ProjectMemberModel& model,
    std::size_t typeOrdinal,
    std::string_view name,
    ClassMemberKind kind,
    std::string_view signature = {})
{
    for (const std::size_t ordinal : model.membersOfType(typeOrdinal))
    {
        const ClassMemberSymbol& member = model.members()[ordinal];
        if (member.name == name && member.kind == kind &&
            (signature.empty() || member.signatureKey == signature))
        {
            return &member;
        }
    }
    return nullptr;
}

void expectMemberConflict(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view a,
    std::string_view b)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName);
    writeFile(directory / "A.ns", a);
    writeFile(directory / "B.ns", b);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(
        directory / (std::string(caseName) + ".nsproj"),
        error);
    tests.expect(!ok, std::string(caseName) + " is rejected during member binding");
    tests.expect(error.empty(), std::string(caseName) + " is a semantic member error, not a loader error");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateSymbol),
        std::string(caseName) + " emits DuplicateSymbol");
    tests.expect(hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
        std::string(caseName) + " points to the previous member declaration");
}

void testCrossFileMembers(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "CrossFileMembers";
    writeProject(directory, "CrossFileMembers");
    writeFile(
        directory / "B_Operations.ns",
        "namespace Game;\n"
        "public partial class Widget {\n"
        "  protected int Other;\n"
        "  internal int InternalValue;\n"
        "  public int Add(int delta) { return this.Value + delta; }\n"
        "  public int Add(float delta) { return this.Value; }\n"
        "}\n");
    writeFile(
        directory / "A_State.ns",
        "namespace Game;\n"
        "public partial class Widget {\n"
        "  int Value;\n"
        "  private int Secret;\n"
        "  public int Get() { return this.Value; }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(directory / "CrossFileMembers.nsproj", error);
    tests.expect(ok, "cross-file partial class member spaces bind successfully");
    tests.expect(error.empty(), "cross-file member fixture has no loader error");
    tests.expect(compilation.nameLookup() != nullptr && compilation.memberModel() != nullptr,
        "member compilation exposes both name lookup and member model");
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return;
    }

    const ProjectNameLookup& lookup = *compilation.nameLookup();
    const ProjectMemberModel& model = *compilation.memberModel();
    const std::optional<std::size_t> typeOrdinal = findTypeOrdinal(
        lookup,
        "CrossFileMembers::Game.Widget");
    tests.expect(typeOrdinal.has_value(), "aggregated Widget type remains the owner of all members");
    if (!typeOrdinal.has_value())
    {
        return;
    }

    tests.expect(model.membersOfType(*typeOrdinal).size() == 7,
        "two partial class parts contribute one deterministic seven-member declaration space");
    const auto members = model.membersOfType(*typeOrdinal);
    if (members.size() == 7)
    {
        tests.expect(model.members()[members[0]].name == "Value" &&
                model.members()[members[0]].sourcePath.find("A_State.ns") != std::string::npos,
            "canonical member order starts from the lower deterministic source ordinal, not file creation order");
        tests.expect(model.members()[members[3]].name == "Other" &&
                model.members()[members[3]].sourcePath.find("B_Operations.ns") != std::string::npos,
            "members from the second partial part follow the complete first part");
    }

    const ClassMemberSymbol* value = findMember(model, *typeOrdinal, "Value", ClassMemberKind::Field);
    const ClassMemberSymbol* secret = findMember(model, *typeOrdinal, "Secret", ClassMemberKind::Field);
    const ClassMemberSymbol* other = findMember(model, *typeOrdinal, "Other", ClassMemberKind::Field);
    const ClassMemberSymbol* internalValue = findMember(
        model,
        *typeOrdinal,
        "InternalValue",
        ClassMemberKind::Field);
    tests.expect(value != nullptr && value->accessibility == ClassMemberAccessibility::Private,
        "class member default accessibility is private");
    tests.expect(secret != nullptr && secret->accessibility == ClassMemberAccessibility::Private,
        "explicit private field remains private");
    tests.expect(other != nullptr && other->accessibility == ClassMemberAccessibility::Protected,
        "protected member accessibility is represented semantically");
    tests.expect(internalValue != nullptr && internalValue->accessibility == ClassMemberAccessibility::Internal,
        "internal member accessibility is represented semantically");

    const ClassMemberSymbol* get = findMember(model, *typeOrdinal, "Get", ClassMemberKind::Method, "Get()");
    const ClassMemberSymbol* addInt = findMember(
        model,
        *typeOrdinal,
        "Add",
        ClassMemberKind::Method,
        "Add(builtin:int)");
    const ClassMemberSymbol* addFloat = findMember(
        model,
        *typeOrdinal,
        "Add",
        ClassMemberKind::Method,
        "Add(builtin:float)");
    tests.expect(get != nullptr && get->hasImplicitThis,
        "every instance method symbol carries an implicit-this semantic contract");
    tests.expect(addInt != nullptr && addFloat != nullptr,
        "methods with the same name and different parameter types form legal overloads");
    tests.expect(addInt != nullptr && addInt->parameters.size() == 1 &&
            addInt->parameters[0].name == "delta",
        "method parameter symbols are retained by the member model");

    std::size_t thisCount = 0;
    std::size_t valueAccessCount = 0;
    for (const auto& binding : model.bodyBindings())
    {
        if (binding.kind == MemberBodyBindingKind::ThisReference)
        {
            ++thisCount;
        }
        if (binding.kind == MemberBodyBindingKind::ThisMemberAccess &&
            binding.memberName == "Value")
        {
            ++valueAccessCount;
            tests.expect(binding.candidateMemberOrdinals.size() == 1,
                "this.Value resolves to one member candidate even across partial-class source files");
        }
    }
    tests.expect(thisCount == 3, "three explicit this expressions are bound to the declaring class");
    tests.expect(valueAccessCount == 3, "all three this.Value accesses bind through the aggregated member space");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMemberModelDumper().dump(model, lookup, dump);
    tests.expect(dump.str().find("implicit-this=CrossFileMembers::Game.Widget") != std::string::npos,
        "member dump makes the implicit this type observable");
    tests.expect(dump.str().find("signature=Add(builtin:int)") != std::string::npos &&
            dump.str().find("signature=Add(builtin:float)") != std::string::npos,
        "member dump exposes overload identity explicitly");
}

void testConstructorIndex(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "ConstructorIndex";
    writeProject(directory, "ConstructorIndex");
    writeFile(
        directory / "Main.ns",
        "class Box { public Box() { } public Box(int value) { } int Read() { return 0; } }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(directory / "ConstructorIndex.nsproj", error);
    tests.expect(ok, "constructor index fixture binds successfully");
    tests.expect(compilation.nameLookup() != nullptr && compilation.memberModel() != nullptr,
        "constructor index fixture exposes semantic models");
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return;
    }

    const std::optional<std::size_t> typeOrdinal = findTypeOrdinal(
        *compilation.nameLookup(),
        "ConstructorIndex::Box");
    tests.expect(typeOrdinal.has_value(), "constructor owner has a stable type ordinal");
    if (!typeOrdinal.has_value())
    {
        return;
    }

    const auto constructors = compilation.memberModel()->constructorsOfType(*typeOrdinal);
    tests.expect(constructors.size() == 2, "constructor index contains exactly the two declarations");
    tests.expect(constructors.size() == 2 &&
            compilation.memberModel()->members()[constructors[0]].kind == ClassMemberKind::Constructor &&
            compilation.memberModel()->members()[constructors[1]].kind == ClassMemberKind::Constructor,
        "constructor index excludes ordinary methods");
    tests.expect(compilation.memberModel()->constructorsOfType(*typeOrdinal + 1000).empty(),
        "out-of-range constructor lookup returns an empty span");
}

void testNamedTypeCanonicalSignatures(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path core = root / "CanonicalCore";
    const std::filesystem::path app = root / "CanonicalApp";
    writeProject(core, "CanonicalCore");
    writeFile(core / "Payload.ns", "namespace Shared; public class Payload {}\n");
    writeProject(
        app,
        "CanonicalApp",
        "<ProjectReference Include=\"../CanonicalCore/CanonicalCore.nsproj\" />");
    writeFile(
        app / "Handler.ns",
        "using Shared;\n"
        "using Alias = Shared.Payload;\n"
        "class Handler {\n"
        "  public Payload Field;\n"
        "  public Payload Echo(Payload value) { return value; }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(app / "CanonicalApp.nsproj", error);
    tests.expect(ok, "member type binding reuses project namespace/using/dependency lookup");
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        tests.expect(false, "canonical named-type fixture builds member model");
        return;
    }
    const std::optional<std::size_t> handlerOrdinal = findTypeOrdinal(
        *compilation.nameLookup(),
        "CanonicalApp::Handler");
    tests.expect(handlerOrdinal.has_value(), "Handler type is present in root module");
    if (handlerOrdinal.has_value())
    {
        const ClassMemberSymbol* field = findMember(
            *compilation.memberModel(),
            *handlerOrdinal,
            "Field",
            ClassMemberKind::Field);
        const ClassMemberSymbol* echo = findMember(
            *compilation.memberModel(),
            *handlerOrdinal,
            "Echo",
            ClassMemberKind::Method);
        tests.expect(field != nullptr &&
                field->declaredTypeKey == "type:CanonicalCore::Shared.Payload",
            "field type key uses resolved module-qualified TypeIdentity instead of source spelling");
        tests.expect(echo != nullptr && echo->declaredTypeKey == "type:CanonicalCore::Shared.Payload" &&
                echo->parameters.size() == 1 &&
                echo->parameters[0].typeKey == "type:CanonicalCore::Shared.Payload",
            "method return and parameter types use the same canonical project type identity");
    }

    const std::filesystem::path conflict = root / "AliasSignatureConflict";
    writeProject(
        conflict,
        "AliasSignatureConflict",
        "<ProjectReference Include=\"../CanonicalCore/CanonicalCore.nsproj\" />");
    writeFile(
        conflict / "Handler.ns",
        "using Shared; using Alias = Shared.Payload;\n"
        "class Handler {\n"
        "  int Use(Payload value) { return 0; }\n"
        "  float Use(Alias value) { return 0.0; }\n"
        "}\n");
    ProjectCompilation conflictCompilation;
    const bool conflictOk = conflictCompilation.compileProjectMembers(
        conflict / "AliasSignatureConflict.nsproj",
        error);
    tests.expect(!conflictOk,
        "alias and direct spelling of the same named parameter type cannot create fake overloads");
    tests.expect(hasDiagnostic(conflictCompilation, DiagnosticId::DuplicateSymbol),
        "canonical named-type overload collision emits DuplicateSymbol");
}

void testConflicts(TestContext& tests, const std::filesystem::path& root)
{
    expectMemberConflict(
        tests,
        root,
        "DuplicateField",
        "partial class Box { int Value; }\n",
        "partial class Box { int Value; }\n");
    expectMemberConflict(
        tests,
        root,
        "FieldMethodCollision",
        "partial class Box { int Value; }\n",
        "partial class Box { int Value() { return 0; } }\n");
    expectMemberConflict(
        tests,
        root,
        "DuplicateMethodSignature",
        "partial class Box { int Run(int value) { return value; } }\n",
        "partial class Box { int Run(int other) { return other; } }\n");
    expectMemberConflict(
        tests,
        root,
        "ReturnTypeNotOverloadKey",
        "partial class Box { int Run(int value) { return value; } }\n",
        "partial class Box { float Run(int value) { return 0.0; } }\n");
}

void testInitialOverloadLookup(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "InitialOverloadLookup";
    writeProject(directory, "InitialOverloadLookup");
    writeFile(
        directory / "Box.ns",
        "class Box {\n"
        "  int Add(int value) { return value; }\n"
        "  int Add(float value) { return 0; }\n"
        "  int Call() { return this.Add(1); }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(
        directory / "InitialOverloadLookup.nsproj",
        error);
    tests.expect(ok, "explicit this.Member lookup can expose an overload candidate set");
    if (!ok || compilation.memberModel() == nullptr)
    {
        return;
    }

    bool sawOverloadSet = false;
    for (const auto& binding : compilation.memberModel()->bodyBindings())
    {
        if (binding.kind == MemberBodyBindingKind::ThisMemberAccess &&
            binding.memberName == "Add")
        {
            sawOverloadSet = true;
            tests.expect(binding.candidateMemberOrdinals.size() == 2,
                "this.Add initially binds to both overload candidates before call overload resolution");
        }
    }
    tests.expect(sawOverloadSet, "member-body bindings retain the this.Add overload-set lookup result");
}

void testBodyDiagnostics(TestContext& tests, const std::filesystem::path& root)
{
    {
        const std::filesystem::path directory = root / "UnknownThisMember";
        writeProject(directory, "UnknownThisMember");
        writeFile(directory / "C.ns", "class Box { int Run() { return this.Missing; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(directory / "UnknownThisMember.nsproj", error);
        tests.expect(!ok, "unknown this.Member is rejected during Stage 26 member binding");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::UnknownMember),
            "unknown this.Member uses UnknownMember diagnostic");
    }

    {
        const std::filesystem::path directory = root / "ThisOutsideMethod";
        writeProject(directory, "ThisOutsideMethod");
        writeFile(directory / "Main.ns", "int F() { return this; }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(directory / "ThisOutsideMethod.nsproj", error);
        tests.expect(!ok, "this in a procedural function is rejected by the project member phase");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidMemberAccess),
            "this outside an instance method uses InvalidMemberAccess");
    }

    {
        const std::filesystem::path directory = root / "ThisPointerAccess";
        writeProject(directory, "ThisPointerAccess");
        writeFile(directory / "C.ns", "class Box { int Value; int Run() { return this->Value; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(directory / "ThisPointerAccess.nsproj", error);
        tests.expect(!ok, "this->member is rejected because Stage 26 this is a class reference");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidMemberAccess),
            "this->member produces InvalidMemberAccess");
    }

    {
        const std::filesystem::path directory = root / "DuplicateParameters";
        writeProject(directory, "DuplicateParameters");
        writeFile(directory / "C.ns", "class Box { int Run(int value, float value) { return 0; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(directory / "DuplicateParameters.nsproj", error);
        tests.expect(!ok, "duplicate parameter names are rejected while creating method symbols");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::DuplicateSymbol) &&
                hasDiagnostic(compilation, DiagnosticId::PreviousDeclaration),
            "duplicate parameter diagnostic preserves previous declaration location");
    }

    {
        const std::filesystem::path directory = root / "VoidParameter";
        writeProject(directory, "VoidParameter");
        writeFile(directory / "Box.ns", "class Box { int Run(void value) { return 0; } }\n");
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(directory / "VoidParameter.nsproj", error);
        tests.expect(!ok, "bare void method parameter is rejected by Stage 26 member semantics");
        tests.expect(hasDiagnostic(compilation, DiagnosticId::InvalidType),
            "bare void parameter uses InvalidType while void* remains legal");
    }
}
// NSC-LIMIT-009 -- UN TIPO ANIDADO NO ES UN MIEMBRO EJECUTABLE.
//
// La prueba no es decorativa: `appendExpandedMemberNodes` decide que es miembro por una lista de
// AstNodeKind, y un tipo anidado simplemente no coincidia con ninguna rama. Eso funciona por
// accidente, y el accidente se rompe en cuanto alguien anade una rama nueva -- con un sintoma que
// no es un error sino un miembro fantasma en el layout. Ahora se descarta explicitamente, y esta
// prueba es lo que sostiene esa decision: el contenedor tiene EXACTAMENTE sus miembros, y el
// anidado esta en la tabla de tipos, con sus propios miembros.
void testNestedTypesAreNotMembers(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "NestedNotMembers";
    writeProject(directory, "NestedNotMembers");
    writeFile(
        directory / "Types.ns",
        "namespace Nested;\n"
        "public class Outer {\n"
        "  public int Campo;\n"
        "  public struct Item { public int a; public int Doble() { return this.a * 2; } }\n"
        "  public enum Modo { A, B }\n"
        "  public int Leer() { return this.Campo; }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(directory / "NestedNotMembers.nsproj", error);
    tests.expect(ok, "a class with nested types builds its member model");
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return;
    }

    const ProjectNameLookup& lookup = *compilation.nameLookup();
    const ProjectMemberModel& model = *compilation.memberModel();

    const std::optional<std::size_t> outer =
        findTypeOrdinal(lookup, "NestedNotMembers::Nested.Outer");
    const std::optional<std::size_t> item =
        findTypeOrdinal(lookup, "NestedNotMembers::Nested.Outer+Item");
    const std::optional<std::size_t> modo =
        findTypeOrdinal(lookup, "NestedNotMembers::Nested.Outer+Modo");
    tests.expect(outer.has_value(), "the container is a named type");
    tests.expect(item.has_value(), "the nested struct is a named type of its own");
    tests.expect(modo.has_value(), "the nested enum is a named type of its own");
    if (!outer.has_value() || !item.has_value())
    {
        return;
    }

    // DOS, no cuatro: el campo y el metodo. Ni `Item` ni `Modo` cuentan.
    tests.expect(model.membersOfType(*outer).size() == 2,
        "the container has exactly its field and its method; the two nested types are not members");
    for (const std::size_t memberOrdinal : model.membersOfType(*outer))
    {
        const ClassMemberSymbol& member = model.members()[memberOrdinal];
        tests.expect(member.name != "Item" && member.name != "Modo",
            "no nested type appears among the container's members");
    }

    // Y el anidado tiene los suyos, que es la otra mitad: no aparecer como miembro del contenedor
    // no puede significar haber desaparecido.
    tests.expect(model.membersOfType(*item).size() == 2,
        "the nested struct owns its own field and method");
}

void testDeepMethodBodyScan(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "DeepMethodBodyScan";
    writeProject(directory, "DeepMethodBodyScan");
    constexpr std::size_t depth = 16384;
    std::string source = "class Box { int Run() { return Root";
    for (std::size_t index = 0; index < depth; ++index) source += ".Part";
    source += "(); } }\n";
    writeFile(directory / "Main.ns", source);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(
        directory / "DeepMethodBodyScan.nsproj", error);
    tests.expect(ok && error.empty() && !compilation.diagnostics().hasErrors(),
        "member binding scans a 16,384-segment method expression without using the native stack");
}

void testDeepInvalidThisContextScan(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "DeepInvalidThisContextScan";
    writeProject(directory, "DeepInvalidThisContextScan");
    constexpr std::size_t depth = 16384;
    std::string source = "int F() { return Root";
    for (std::size_t index = 0; index < depth; ++index) source += ".Part";
    source += "(this, this); }\n";
    writeFile(directory / "Main.ns", source);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(
        directory / "DeepInvalidThisContextScan.nsproj", error);
    tests.expect(!ok && error.empty(),
        "invalid-this scanning rejects a deep procedural expression as a semantic member error");

    std::size_t invalidThisCount = 0;
    std::uint32_t previousOffset = 0;
    bool exactMessages = true;
    bool sourceOrder = true;
    for (const nativesharp::diagnostics::Diagnostic& diagnostic :
         compilation.diagnostics().diagnostics())
    {
        if (diagnostic.id() != DiagnosticId::InvalidMemberAccess) continue;
        exactMessages = exactMessages &&
            diagnostic.message() == "'this' is available only inside an instance method";
        const std::uint32_t offset = diagnostic.location().offset();
        if (invalidThisCount != 0) sourceOrder = sourceOrder && previousOffset < offset;
        previousOffset = offset;
        ++invalidThisCount;
    }
    tests.expect(invalidThisCount == 2 && exactMessages,
        "deep invalid-this scanning preserves both exact InvalidMemberAccess diagnostics");
    tests.expect(sourceOrder,
        "deep invalid-this scanning preserves source-order diagnostic publication");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage26_project_member_model_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testCrossFileMembers(tests, root);
    testConstructorIndex(tests, root);
    testNamedTypeCanonicalSignatures(tests, root);
    testConflicts(tests, root);
    testInitialOverloadLookup(tests, root);
    testBodyDiagnostics(tests, root);
    testNestedTypesAreNotMembers(tests, root);
    testDeepMethodBodyScan(tests, root);
    testDeepInvalidThisContextScan(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
