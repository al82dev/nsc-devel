#include "nativesharp/AST/AstArena.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"
#include "nativesharp/Source/SourceManager.h"
#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Sema/OwnershipKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeContext.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::ClassMemberKind;
using nativesharp::frontend::ClassMemberSymbol;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectMemberModel;
using nativesharp::frontend::ProjectNameLookup;
using nativesharp::sema::OwnershipKind;
using nativesharp::sema::Type;
using nativesharp::sema::TypeContext;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory)
{
    writeFile(
        directory / "OwnershipTypes.nsproj",
        "<Project><PropertyGroup><Name>OwnershipTypes</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

std::optional<std::size_t> findTypeOrdinal(const ProjectNameLookup& lookup, std::string_view key)
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
    std::string_view signature)
{
    for (std::size_t ordinal : model.membersOfType(typeOrdinal))
    {
        const ClassMemberSymbol& member = model.members()[ordinal];
        if (member.kind == ClassMemberKind::Method && member.name == name && member.signatureKey == signature)
        {
            return &member;
        }
    }
    return nullptr;
}

void testCanonicalQualifiedTypes(TestContext& tests)
{
    TypeContext types;
    Type* resource = types.createStruct("Resource", nullptr);
    resource->setComplete(true);

    Type* owned = types.qualified(resource, OwnershipKind::Owned);
    Type* borrowed = types.qualified(resource, OwnershipKind::Borrowed);

    tests.expect(owned != resource && borrowed != resource && owned != borrowed,
        "owned and borrowed are distinct semantic type identities");
    tests.expect(types.qualified(resource, OwnershipKind::Owned) == owned,
        "owned type qualification is canonicalized");
    tests.expect(types.qualified(owned, OwnershipKind::Owned) == owned,
        "requalifying an owned type does not create a second canonical wrapper");
    tests.expect(types.qualified(borrowed, OwnershipKind::Owned) == owned,
        "changing ownership reuses the canonical wrapper for the same underlying type");
    tests.expect(types.qualified(owned, OwnershipKind::None) == resource,
        "removing ownership returns the physical underlying type");

    tests.expect(owned->ownership() == OwnershipKind::Owned &&
            borrowed->ownership() == OwnershipKind::Borrowed,
        "semantic ownership kind is retained explicitly");
    tests.expect(owned->unqualifiedType() == resource && borrowed->unqualifiedType() == resource,
        "qualified types retain their physical underlying type");
    tests.expect(owned->kind() == resource->kind() && owned->isComplete() == resource->isComplete(),
        "qualified types delegate physical type properties to the underlying type");
    tests.expect(owned->displayName() == "owned Resource" &&
            borrowed->displayName() == "borrowed Resource",
        "qualified type formatting is deterministic and pedagogical");
}

void testQualifiedFunctionSignatures(TestContext& tests)
{
    TypeContext types;
    Type* resource = types.createStruct("Resource", nullptr);
    resource->setComplete(true);
    Type* owned = types.qualified(resource, OwnershipKind::Owned);
    Type* borrowed = types.qualified(resource, OwnershipKind::Borrowed);
    Type* voidType = types.builtin(nativesharp::sema::TypeKind::Void);

    Type* consume = types.functionType(voidType, std::vector<Type*>{owned});
    Type* inspect = types.functionType(voidType, std::vector<Type*>{borrowed});
    Type* consumeAgain = types.functionType(voidType, std::vector<Type*>{owned});
    Type* create = types.functionType(owned, {});

    tests.expect(consume != inspect,
        "function semantic identity distinguishes owned from borrowed parameter contracts");
    tests.expect(consumeAgain == consume,
        "function types with identical ownership-qualified signatures are canonicalized");
    tests.expect(consume->displayName() == "(owned Resource) -> void",
        "owned parameter appears in function type formatting");
    tests.expect(inspect->displayName() == "(borrowed Resource) -> void",
        "borrowed parameter appears in function type formatting");
    tests.expect(create->displayName() == "() -> owned Resource",
        "owned return contract appears in function type formatting");

    Type* rawPointer = types.pointerTo(resource);
    Type* ownedPointer = types.qualified(rawPointer, OwnershipKind::Owned);
    tests.expect(types.pointerTo(resource) == rawPointer && types.pointerTo(resource) != ownedPointer,
        "physical pointer interning never aliases an ownership-qualified wrapper");
}


void testSourceToSemaOwnershipTypes(TestContext& tests)
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::ast::AstArena arena;
    TypeContext types;
    const nativesharp::target::TargetDataLayout layout =
        nativesharp::target::TargetDataLayout::WindowsX64();

    const nativesharp::FileId fileId = sources.addBuffer(
        "stage50_2_sema.ns",
        "struct Resource { int Value; } "
        "owned Resource Create(); "
        "void Consume(owned Resource value); "
        "void Inspect(borrowed Resource value); ");
    const nativesharp::source::SourceFile* source = sources.getFile(fileId);
    tests.expect(source != nullptr, "source-to-sema fixture is registered");
    if (source == nullptr)
    {
        return;
    }

    nativesharp::lexer::Lexer lexer(*source, diagnostics);
    std::vector<nativesharp::lexer::Token> tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(tokens, arena, diagnostics);
    nativesharp::ast::AstNode* root = parser.parseCompilationUnit();
    nativesharp::sema::SemanticAnalyzer analyzer(types, layout, diagnostics);
    const bool ok = root != nullptr && analyzer.analyze(root);
    tests.expect(ok && !diagnostics.hasErrors(),
        "source ownership qualifiers survive parser-to-sema binding");
    if (!ok)
    {
        return;
    }

    const nativesharp::sema::Symbol* create = nullptr;
    const nativesharp::sema::Symbol* consume = nullptr;
    const nativesharp::sema::Symbol* inspect = nullptr;
    for (const nativesharp::sema::Symbol* symbol : analyzer.globalSymbols())
    {
        if (symbol->name() == "Create") create = symbol;
        if (symbol->name() == "Consume") consume = symbol;
        if (symbol->name() == "Inspect") inspect = symbol;
    }

    tests.expect(create != nullptr && create->type()->returnType()->ownership() == OwnershipKind::Owned &&
            create->type()->returnType()->displayName() == "owned Resource",
        "owned return is represented in the semantic function type");
    tests.expect(consume != nullptr && consume->type()->parameterTypes().size() == 1 &&
            consume->type()->parameterTypes()[0]->ownership() == OwnershipKind::Owned,
        "owned parameter is represented in the semantic function type");
    tests.expect(inspect != nullptr && inspect->type()->parameterTypes().size() == 1 &&
            inspect->type()->parameterTypes()[0]->ownership() == OwnershipKind::Borrowed,
        "borrowed parameter is represented in the semantic function type");
}

void testProjectMemberOwnershipContracts(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "ProjectContracts";
    writeProject(directory);
    writeFile(
        directory / "Ownership.ns",
        "namespace Stage50;\n"
        "public class Resource { public int Value; }\n"
        "public class Api {\n"
        "  public static owned Resource Create() { }\n"
        "  public static void Consume(owned Resource value) { }\n"
        "  public static void Inspect(borrowed Resource value) { }\n"
        "  public static void Route(owned Resource value) { }\n"
        "  public static void Route(borrowed Resource value) { }\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(directory / "OwnershipTypes.nsproj", error);
    tests.expect(ok, "project member binding accepts ownership-qualified API contracts");
    tests.expect(error.empty(), "ownership project has no loader error");
    tests.expect(compilation.nameLookup() != nullptr && compilation.memberModel() != nullptr,
        "ownership project exposes name lookup and member model");
    if (!ok || compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return;
    }

    const ProjectNameLookup& lookup = *compilation.nameLookup();
    const ProjectMemberModel& model = *compilation.memberModel();
    const std::optional<std::size_t> apiOrdinal = findTypeOrdinal(lookup, "OwnershipTypes::Stage50.Api");
    tests.expect(apiOrdinal.has_value(), "Api type is available in the project semantic model");
    if (!apiOrdinal.has_value())
    {
        return;
    }

    const std::string resourceKey = "type:OwnershipTypes::Stage50.Resource";
    const ClassMemberSymbol* create = findMember(model, *apiOrdinal, "Create", "Create()");
    const ClassMemberSymbol* consume = findMember(
        model,
        *apiOrdinal,
        "Consume",
        "Consume(owned " + resourceKey + ")");
    const ClassMemberSymbol* inspect = findMember(
        model,
        *apiOrdinal,
        "Inspect",
        "Inspect(borrowed " + resourceKey + ")");
    const ClassMemberSymbol* routeOwned = findMember(
        model,
        *apiOrdinal,
        "Route",
        "Route(owned " + resourceKey + ")");
    const ClassMemberSymbol* routeBorrowed = findMember(
        model,
        *apiOrdinal,
        "Route",
        "Route(borrowed " + resourceKey + ")");

    tests.expect(create != nullptr && create->declaredTypeKey == resourceKey &&
            create->declaredOwnership == OwnershipKind::Owned,
        "owned return keeps physical typeKey separate from ownership contract");
    tests.expect(consume != nullptr && consume->parameters.size() == 1 &&
            consume->parameters[0].typeKey == resourceKey &&
            consume->parameters[0].ownership == OwnershipKind::Owned,
        "owned parameter keeps physical typeKey and semantic ownership separately");
    tests.expect(inspect != nullptr && inspect->parameters.size() == 1 &&
            inspect->parameters[0].typeKey == resourceKey &&
            inspect->parameters[0].ownership == OwnershipKind::Borrowed,
        "borrowed parameter keeps physical typeKey and semantic ownership separately");
    tests.expect(routeOwned != nullptr && routeBorrowed != nullptr && routeOwned != routeBorrowed,
        "ownership qualifier participates in method semantic signature identity");

    if (consume != nullptr && inspect != nullptr)
    {
        tests.expect(
            model.canonicalTypeKey(consume->parameters[0].typeSyntax, lookup) ==
                model.canonicalTypeKey(inspect->parameters[0].typeSyntax, lookup),
            "owned and borrowed share one physical canonical type key for ABI/layout purposes");
    }
}

void testRawUnionRejectsOwnedField(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "RawUnionOwnedField";
    writeProject(directory);
    writeFile(
        directory / "Ownership.ns",
        "namespace Stage50;\n"
        "public class Payload { public int Value; }\n"
        "public union Slot { public owned Payload Object; public int Code; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(directory / "OwnershipTypes.nsproj", error);
    tests.expect(!ok, "raw union with an owned field is rejected before lowering");

    bool foundDiagnostic = false;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find("not permitted in a raw union") != std::string::npos)
        {
            foundDiagnostic = true;
            break;
        }
    }
    tests.expect(foundDiagnostic,
        "raw union ownership rejection explains that no active-member discriminant exists");
}

void testRawUnionRejectsTransitivelyOwnedField(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "RawUnionTransitivelyOwnedField";
    writeProject(directory);
    writeFile(
        directory / "Ownership.ns",
        "namespace Stage50;\n"
        "public class Payload { public int Value; }\n"
        "public struct Wrapper { public owned Payload Object; }\n"
        "public union Slot { public Wrapper Wrapped; public int Code; }\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMembers(directory / "OwnershipTypes.nsproj", error);
    tests.expect(!ok,
        "raw union rejects ownership hidden inside a by-value struct alternative");

    bool foundDiagnostic = false;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find("transitively contains owned storage") != std::string::npos)
        {
            foundDiagnostic = true;
            break;
        }
    }
    tests.expect(foundDiagnostic,
        "transitive raw-union diagnostic identifies nested owned storage");
}

void testRawUnionRejectsTransitiveOwnershipShapes(
    TestContext& tests,
    const std::filesystem::path& root)
{
    struct Case final
    {
        const char* name;
        const char* source;
        const char* expectation;
    };
    const Case cases[] = {
        {
            "AnonymousOwnedField",
            "namespace Stage50;\n"
            "public class Payload { public int Value; }\n"
            "public union Slot {\n"
            "  public struct { public owned Payload Object; } Wrapped;\n"
            "  public int Code;\n"
            "}\n",
            "raw union rejects ownership hidden in a synthesized anonymous struct",
        },
        {
            "AnonymousUnionOwnedField",
            "namespace Stage50;\n"
            "public class Payload { public int Value; }\n"
            "public struct Envelope {\n"
            "  public union { public owned Payload Object; public int Code; } Storage;\n"
            "}\n",
            "a synthesized anonymous union enforces the same ownership rule",
        },
        {
            "DirectOwnedResourceKinds",
            "namespace Stage50;\n"
            "public delegate void Callback();\n"
            "public union Resources {\n"
            "  public owned string Text;\n"
            "  public owned int[] Buffer;\n"
            "  public owned Callback Handler;\n"
            "}\n",
            "raw union rejects every directly owned reference-like resource kind",
        },
        {
            "FixedArrayOfOwnedStruct",
            "namespace Stage50;\n"
            "public class Payload { public int Value; }\n"
            "public struct Wrapper { public owned Payload Object; }\n"
            "public union Slot { public Wrapper[2] Items; public int Code; }\n",
            "raw union rejects a fixed array whose inline element transitively owns storage",
        },
        {
            "ClosedGenericOwnedStruct",
            "namespace Stage50;\n"
            "public class Payload { public int Value; }\n"
            "public struct Wrapper<T> { public owned T Object; }\n"
            "public union Slot { public Wrapper<Payload> Wrapped; public int Code; }\n",
            "raw union rejects ownership exposed by a closed generic value type",
        },
        {
            "NestedOwnedUnion",
            "namespace Stage50;\n"
            "public class Payload { public int Value; }\n"
            "public struct Wrapper { public owned Payload Object; }\n"
            "public union Inner { public Wrapper Wrapped; public int Code; }\n"
            "public union Outer { public Inner Nested; public long Bits; }\n",
            "raw unions cannot hide owning storage through another nested union",
        },
    };

    for (const Case& item : cases)
    {
        const std::filesystem::path directory = root / item.name;
        writeProject(directory);
        writeFile(directory / "Ownership.ns", item.source);

        ProjectCompilation compilation;
        std::string error;
        const bool ok = compilation.compileProjectMembers(
            directory / "OwnershipTypes.nsproj", error);
        tests.expect(!ok, item.expectation);
        bool foundDiagnostic = false;
        for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        {
            if (diagnostic.message().find("raw union") != std::string::npos)
            {
                foundDiagnostic = true;
                break;
            }
        }
        tests.expect(foundDiagnostic,
            std::string(item.name) + " produces the raw-union ownership diagnostic");
    }
}

void testExplicitLayoutRejectsOverlappingOwnedStorage(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "ExplicitOverlapOwnedField";
    writeProject(directory);
    writeFile(
        directory / "Ownership.ns",
        "namespace Stage50;\n"
        "public class Payload { public int Value; }\n"
        "public struct Overlap {\n"
        "  public at(0) owned Payload Object;\n"
        "  public at(0) long Bits;\n"
        "}\n");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectObjectLayouts(
        directory / "OwnershipTypes.nsproj", error);
    tests.expect(!ok,
        "explicit layout rejects overlap between owned storage and another field");

    bool foundDiagnostic = false;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find("explicit layout cannot overlap owning storage") !=
            std::string::npos)
        {
            foundDiagnostic = true;
            break;
        }
    }
    tests.expect(foundDiagnostic,
        "explicit-overlap diagnostic identifies missing active-member tracking");
}

void testExplicitLayoutUsesExactOwnedRanges(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const std::filesystem::path nested = root / "ExplicitNestedOwnedOverlap";
    writeProject(nested);
    writeFile(
        nested / "Ownership.ns",
        "namespace Stage50;\n"
        "public class Payload { public int Value; }\n"
        "public struct Wrapper { public owned Payload Object; }\n"
        "public struct Overlap {\n"
        "  public at(0) Wrapper Wrapped;\n"
        "  public at(0) long Bits;\n"
        "}\n");

    ProjectCompilation nestedCompilation;
    std::string error;
    const bool nestedOk = nestedCompilation.compileProjectObjectLayouts(
        nested / "OwnershipTypes.nsproj", error);
    tests.expect(!nestedOk,
        "explicit layout rejects overlap with transitively owned inline storage");

    const std::filesystem::path disjoint = root / "ExplicitDisjointOwnedFields";
    writeProject(disjoint);
    writeFile(
        disjoint / "Ownership.ns",
        "namespace Stage50;\n"
        "public class Payload { public int Value; }\n"
        "public struct Disjoint {\n"
        "  public at(0) owned Payload Object;\n"
        "  public at(8) long Bits;\n"
        "}\n");

    ProjectCompilation disjointCompilation;
    error.clear();
    const bool disjointOk = disjointCompilation.compileProjectObjectLayouts(
        disjoint / "OwnershipTypes.nsproj", error);
    tests.expect(disjointOk,
        "explicit non-overlapping ABI layout may contain owned storage");

    const std::filesystem::path scalar = root / "ExplicitScalarOverlap";
    writeProject(scalar);
    writeFile(
        scalar / "Ownership.ns",
        "namespace Stage50;\n"
        "public struct ScalarOverlay {\n"
        "  public at(0) int Signed;\n"
        "  public at(0) uint Unsigned;\n"
        "}\n");

    ProjectCompilation scalarCompilation;
    error.clear();
    const bool scalarOk = scalarCompilation.compileProjectObjectLayouts(
        scalar / "OwnershipTypes.nsproj", error);
    tests.expect(scalarOk,
        "explicit overlap remains legal when every overlapping field has trivial storage");
}
}

int main()
{
    TestContext tests;
    testCanonicalQualifiedTypes(tests);
    testQualifiedFunctionSignatures(tests);
    testSourceToSemaOwnershipTypes(tests);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_2_ownership_types";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    testProjectMemberOwnershipContracts(tests, root);
    testRawUnionRejectsOwnedField(tests, root);
    testRawUnionRejectsTransitivelyOwnedField(tests, root);
    testRawUnionRejectsTransitiveOwnershipShapes(tests, root);
    testExplicitLayoutRejectsOverlappingOwnedStorage(tests, root);
    testExplicitLayoutUsesExactOwnedRanges(tests, root);
    std::filesystem::remove_all(root, error);

    return tests.result();
}
