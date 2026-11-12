#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"
#include "../../src/Frontend/Generics/StaticFieldTemplateRewriter.h"
#include "../../src/Frontend/Generics/StaticFieldControlFlow.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using nativesharp::ast::AstArena;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

AstNode* descriptorMember(AstArena& arena, std::string_view member)
{
    AstNode* name = arena.make<AstNode>(AstNodeKind::IdentifierExpression, nativesharp::SourceRange{});
    name->setText("field");
    AstNode* node = arena.make<AstNode>(AstNodeKind::MemberExpression, nativesharp::SourceRange{});
    node->setFirst(name);
    node->setText(std::string(member));
    return node;
}

void verifyStaticFieldRewriter(nativesharp::tests::TestContext& tests)
{
    using nativesharp::frontend::internal::rewriteStaticFieldTemplate;
    AstArena arena;
    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* statement = arena.make<AstNode>(AstNodeKind::ExpressionStatement, nativesharp::SourceRange{});
    AstNode* call = arena.make<AstNode>(AstNodeKind::CallExpression, nativesharp::SourceRange{});
    AstNode* receiver = arena.make<AstNode>(AstNodeKind::CallExpression, nativesharp::SourceRange{});
    call->setFirst(descriptorMember(arena, "Get"));
    call->addChild(receiver);
    call->setArgumentName("selected");
    statement->setFirst(call);
    statement->addChild(call);
    body->addChild(statement);
    AstNode* nameUse = descriptorMember(arena, "Name");
    nameUse->setArgumentName("label");
    body->addChild(nameUse);
    body->addChild(nameUse);
    std::vector<const AstNode*> accesses;
    std::string error;
    const bool ok = rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error);
    tests.expect(ok && error.empty(), "static field rewrite accepts a valid cloned body");
    AstNode* rewritten = statement->first();
    tests.expect(ok && rewritten != nullptr && rewritten == statement->children()[0] && accesses.size() == 1 &&
            accesses[0] == rewritten && rewritten->kind() == AstNodeKind::MemberExpression,
        "shared syntax edges use the same recorded field access");
    tests.expect(ok && rewritten != nullptr && rewritten->first() == receiver && rewritten->text() == "Count" &&
            rewritten->argumentName() == "selected",
        "field Get preserves one receiver and its named-argument label");
    tests.expect(ok && body->children()[1]->kind() == AstNodeKind::StringLiteralExpression &&
            body->children()[1] == body->children()[2] && body->children()[1]->text() == "Count" &&
            body->children()[1]->argumentName() == "label",
        "shared field Name uses produce one chosen constant and preserve the argument label");

    AstNode* rejected = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* unchangedName = descriptorMember(arena, "Name");
    AstNode* escaped = arena.make<AstNode>(AstNodeKind::IdentifierExpression, nativesharp::SourceRange{});
    escaped->setText("field");
    rejected->addChild(unchangedName);
    rejected->addChild(escaped);
    const std::size_t before = accesses.size();
    const bool accepted = rewriteStaticFieldTemplate(arena, rejected, "field", "Count", accesses, error);
    tests.expect(!accepted && rejected->children()[0] == unchangedName && before != 0 && accesses.size() == before &&
            accesses.front() == rewritten,
        "escaping descriptor rejects the entire rewrite without mutation or partial access publication");

    AstNode* cyclic = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    cyclic->addChild(cyclic);
    tests.expect(!rewriteStaticFieldTemplate(arena, cyclic, "field", "Count", accesses, error) &&
            cyclic->children()[0] == cyclic && accesses.size() == before,
        "cyclic syntax is rejected without mutation");
}

void verifyStaticFieldNameOfRejection(nativesharp::tests::TestContext& tests)
{
    using nativesharp::frontend::internal::rewriteStaticFieldTemplate;
    AstArena arena;
    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* nameOf = arena.make<AstNode>(AstNodeKind::NameOfExpression, nativesharp::SourceRange{});
    AstNode* nameUse = descriptorMember(arena, "Name");
    nameOf->setFirst(nameUse);
    nameOf->setText("Name");
    body->addChild(nameOf);
    std::vector<const AstNode*> accesses;
    std::string error;
    const bool ok = rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error);
    tests.expect(ok && body->children()[0]->kind() == AstNodeKind::StringLiteralExpression &&
            body->children()[0]->text() == "Name" && accesses.empty(),
        "nameof descriptor member preserves its lexical name without evaluating it");
}

void verifyNestedFieldAccessRewrite(nativesharp::tests::TestContext& tests)
{
    using nativesharp::frontend::internal::rewriteStaticFieldTemplate;
    AstArena arena;
    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* receiver = arena.make<AstNode>(AstNodeKind::IdentifierExpression, nativesharp::SourceRange{});
    receiver->setText("receiver");
    AstNode* inner = arena.make<AstNode>(AstNodeKind::CallExpression, nativesharp::SourceRange{});
    inner->setFirst(descriptorMember(arena, "Get"));
    inner->addChild(receiver);
    AstNode* outer = arena.make<AstNode>(AstNodeKind::CallExpression, nativesharp::SourceRange{});
    outer->setFirst(descriptorMember(arena, "Get"));
    outer->addChild(inner);
    body->addChild(outer);
    std::vector<const AstNode*> accesses;
    std::string error;
    const bool ok = rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error);
    AstNode* result = body->children()[0];
    tests.expect(ok && accesses.size() == 2 && result != nullptr && result->first() != nullptr &&
            result->first()->kind() == AstNodeKind::MemberExpression && result->first()->first() == receiver,
        "nested Get rewrites retain the rewritten receiver chain exactly once");
}

void verifyInvalidFieldAccessRejected(nativesharp::tests::TestContext& tests)
{
    using nativesharp::frontend::internal::rewriteStaticFieldTemplate;
    AstArena arena;
    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* call = arena.make<AstNode>(AstNodeKind::CallExpression, nativesharp::SourceRange{});
    call->setFirst(descriptorMember(arena, "Get"));
    body->addChild(call);
    std::vector<const AstNode*> accesses;
    std::string error;
    tests.expect(!rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error) &&
            body->children()[0] == call && accesses.empty(),
        "Get without a receiver is rejected without publishing a partial rewrite");

    AstNode* shadowBody = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* shadow = arena.make<AstNode>(AstNodeKind::VariableDeclarationStatement, nativesharp::SourceRange{});
    shadow->setText("field");
    shadowBody->addChild(shadow);
    tests.expect(!rewriteStaticFieldTemplate(arena, shadowBody, "field", "Count", accesses, error) &&
            shadowBody->children()[0] == shadow && accesses.empty(),
        "a declaration shadowing the descriptor is rejected without mutation");
}

void verifyStaticFieldControlFlow(nativesharp::tests::TestContext& tests)
{
    using nativesharp::frontend::internal::rewriteStaticFieldTemplate;
    using nativesharp::frontend::internal::collectStaticFieldJumps;
    AstArena arena;
    std::vector<const AstNode*> accesses;
    std::string error;
    for (const AstNodeKind kind : {AstNodeKind::BreakStatement, AstNodeKind::ContinueStatement})
    {
        AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
        AstNode* exit = arena.make<AstNode>(kind, nativesharp::SourceRange{});
        body->addChild(exit);
        std::vector<AstNode*> targets;
        tests.expect(collectStaticFieldJumps(body, targets, error) && targets.size() == 1 && targets[0] == exit,
            "exit targeting the static iteration is selected for lowering");
        AstNode* innerBody = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
        AstNode* innerLoop = arena.make<AstNode>(AstNodeKind::WhileStatement, nativesharp::SourceRange{});
        innerLoop->setSecond(exit);
        innerBody->addChild(innerLoop);
        tests.expect(rewriteStaticFieldTemplate(arena, innerBody, "field", "Count", accesses, error),
            "exit targeting a loop entirely inside the static body is retained");
        innerBody->addChild(exit);
        targets.clear();
        tests.expect(!collectStaticFieldJumps(innerBody, targets, error),
            "a shared exit with conflicting destinations is rejected");
    }
    AstNode* body = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* loop = arena.make<AstNode>(AstNodeKind::LoopExpression, nativesharp::SourceRange{});
    AstNode* named = arena.make<AstNode>(AstNodeKind::ContinueStatement, nativesharp::SourceRange{});
    loop->setText("inner");
    named->setText("inner");
    loop->setFirst(named);
    body->addChild(loop);
    tests.expect(rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error),
        "named continue to an inner labeled loop is retained");
    named->setText("outer");
    tests.expect(!rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error),
        "named continue to an outer loop is rejected");
    AstNode* outer = arena.make<AstNode>(AstNodeKind::LoopExpression, nativesharp::SourceRange{});
    outer->setText("outer");
    const std::vector<const AstNode*> outerLabels{outer};
    tests.expect(rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error, outerLabels),
        "a uniquely verified lexical outer loop label remains available");
    AstNode* valueBody = arena.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    AstNode* valueBreak = arena.make<AstNode>(AstNodeKind::BreakStatement, nativesharp::SourceRange{});
    valueBreak->setText("outer");
    valueBreak->setFirst(arena.make<AstNode>(AstNodeKind::IntegerLiteralExpression, nativesharp::SourceRange{}));
    valueBody->addChild(valueBreak);
    tests.expect(!nativesharp::frontend::internal::validateStaticFieldControlFlow(valueBody, error, outerLabels, false),
        "an outer break value is rejected when no field instance can validate it");
    const std::vector<const AstNode*> ambiguousLabels{outer, outer};
    tests.expect(!rewriteStaticFieldTemplate(arena, body, "field", "Count", accesses, error, ambiguousLabels),
        "ambiguous lexical outer labels fail closed");
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void verifyBinaryOrigins(nativesharp::tests::TestContext& tests)
{
    using namespace nativesharp::frontend;
    std::string project;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NSC_TEST_FIELD_BINARY_PROJECT") == 0 && value != nullptr)
        project.assign(value);
    std::free(value);
#else
    if (const char* value = std::getenv("NSC_TEST_FIELD_BINARY_PROJECT"); value != nullptr)
        project.assign(value);
#endif
    if (project.empty())
    {
        std::cout << "SKIP binary syntax origin: NSC_TEST_FIELD_BINARY_PROJECT is unset\n";
        return;
    }
    ProjectCompilation binaryCompilation;
    std::string error;
    const nativesharp::compilation::ProjectLoadOptions options;
    nativesharp::compilation::ProjectGraphLoader loader(1, {}, options);
    auto graph = loader.load(project, error);
    const bool prepared = graph.has_value() && nativesharp::compilation::preparePrecompiledProject(
        std::move(*graph), options, binaryCompilation, error);
    tests.expect(prepared, "binary origin fixture prepares real precompiled modules");
    if (!prepared)
    {
        std::cerr << "binary origin preparation failed: " << error << '\n';
        return;
    }
    const bool ok = binaryCompilation.compileProjectGenerics(project, error);
    tests.expect(ok, "binary origin fixture reaches generic specialization");
    if (!ok)
    {
        std::cerr << "binary origin generic compilation failed: " << error << '\n';
        for (const auto& diagnostic : binaryCompilation.diagnostics().diagnostics())
            std::cerr << diagnostic.message() << '\n';
        return;
    }
    bool source = false;
    bool metadataApi = false;
    bool metadataTemplate = false;
    for (const auto& unit : binaryCompilation.units())
    {
        if (unit->origin() == SyntaxUnitOrigin::Source) source = true;
        if (unit->origin() != SyntaxUnitOrigin::PrecompiledMetadata || unit->moduleName() != "NativeSharpCore")
            continue;
        if (unit->isPrecompiled()) metadataApi = true;
        else metadataTemplate = true;
    }
    tests.expect(source, "consumer source unit retains source origin");
    tests.expect(metadataApi, "precompiled API unit retains metadata origin");
    tests.expect(metadataTemplate,
        "materialized template retains metadata origin even when isPrecompiled is false");
}

void verifyBinaryFieldMetadata(nativesharp::tests::TestContext& tests)
{
    using namespace nativesharp;
    std::string project;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NSC_TEST_FIELD_METADATA_PROJECT") == 0 && value != nullptr)
        project.assign(value);
    std::free(value);
#else
    if (const char* value = std::getenv("NSC_TEST_FIELD_METADATA_PROJECT"); value != nullptr)
        project.assign(value);
#endif
    if (project.empty())
    {
        std::cout << "SKIP binary field metadata: NSC_TEST_FIELD_METADATA_PROJECT is unset\n";
        return;
    }
    for (const bool demand : {false, true})
    {
        frontend::ProjectCompilation context;
        const compilation::ProjectLoadOptions options;
        compilation::ProjectGraphLoader loader(1, {}, options);
        std::string error;
        auto graph = loader.load(project, error);
        compilation::PrecompiledSurfaceSourceProvider source = [](const std::filesystem::path&)
            -> std::optional<std::string> { return std::nullopt; };
        const bool prepared = graph.has_value() && compilation::preparePrecompiledProject(
            std::move(*graph), options, context, error, {}, demand ? &source : nullptr);
        tests.expect(prepared, "field metadata fixture prepares its binary module");
        if (!prepared)
        {
            std::cerr << "field metadata preparation failed: " << error << '\n';
            continue;
        }
        const frontend::PrecompiledSyntaxModule* module = nullptr;
        for (const auto& item : context.precompiledModules())
            if (item.moduleName == "MetadataProducer") module = &item;
        tests.expect(module != nullptr, "field metadata fixture locates exact producer module");
        if (module == nullptr) continue;
        tests.expect(module->surfacePruned == demand,
            "the source-demand fixture distinguishes full and pruned surfaces");
        interop::LibraryMetadata metadata;
        interop::LibraryMetadataCodec codec;
        const bool decoded = codec.read(module->metadataPath, metadata, error);
        tests.expect(decoded, "complete producer metadata decodes successfully");
        if (!decoded) continue;
        const interop::LibraryMetadataType* record = nullptr;
        for (const auto& type : metadata.types)
            if (type.fullyQualifiedName == "MetadataFields.Record") record = &type;
        tests.expect(record != nullptr && record->fields.size() == 4,
            "complete metadata preserves all four physical fields");
        if (record == nullptr || record->fields.size() != 4) continue;
        tests.expect(record->fields[0].name == "First" && record->fields[1].name == "Hidden" &&
                record->fields[2].name == "__nsc_backing_Automatic$stage38" &&
                record->fields[3].name == "Last" && (record->fields[2].flags & 1024U) != 0U,
            "metadata retains declaration order and the property backing flag");
        const auto first = module->sourceText.find("public int First;");
        const auto hidden = module->sourceText.find("private int Hidden;");
        const auto backing = module->sourceText.find("private int __nsc_layout_property_2;");
        const auto last = module->sourceText.find("public int Last;");
        tests.expect(first != std::string::npos && first < hidden && hidden < backing && backing < last &&
                last != std::string::npos,
            "materialized API preserves physical order and a private property layout field");
    }
}

// This opt-in fixture mutates only compilation-owned, in-memory certificates.
// It does not exercise modified metadata files or an altered materialized clone.
void verifyInheritedGenericCertificates(nativesharp::tests::TestContext& tests)
{
    using namespace nativesharp;
    std::string project;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NSC_TEST_INHERITED_CERT_PROJECT") == 0 && value != nullptr)
        project.assign(value);
    std::free(value);
#else
    if (const char* value = std::getenv("NSC_TEST_INHERITED_CERT_PROJECT"); value != nullptr)
        project.assign(value);
#endif
    if (project.empty())
    {
        std::cout << "SKIP inherited generic certificates: NSC_TEST_INHERITED_CERT_PROJECT is unset\n";
        return;
    }
    for (const std::string_view mode : {"control", "missing-base", "duplicate-base", "wrong-base",
             "wrong-template", "wrong-template-base"})
    {
        frontend::ProjectCompilation context;
        const compilation::ProjectLoadOptions options;
        compilation::ProjectGraphLoader loader(1, {}, options);
        std::string error;
        auto graph = loader.load(project, error);
        const bool prepared = graph.has_value() && compilation::preparePrecompiledProject(
            std::move(*graph), options, context, error);
        tests.expect(prepared, "inherited certificate fixture prepares its binary module");
        if (!prepared)
        {
            std::cerr << mode << ": preparation failed: " << error << '\n';
            continue;
        }
        std::size_t changed = 0;
        for (const auto& view : context.precompiledModules())
        {
            // The module belongs to this non-const compilation and is isolated per variant.
            auto& module = const_cast<frontend::PrecompiledSyntaxModule&>(view);
            if (mode == "missing-base")
            {
                const auto before = module.fieldCertificates.size();
                std::erase_if(module.fieldCertificates, [](const auto& cert) {
                    return cert.typeIdentity.ends_with("::GenericInheritance.Base");
                });
                changed += before - module.fieldCertificates.size();
            }
            else if (mode == "duplicate-base")
            {
                const auto found = std::ranges::find_if(module.fieldCertificates, [](const auto& cert) {
                    return cert.typeIdentity.ends_with("::GenericInheritance.Base");
                });
                if (found != module.fieldCertificates.end())
                {
                    auto copy = *found;
                    module.fieldCertificates.push_back(std::move(copy));
                    ++changed;
                }
            }
            else if (mode == "wrong-base")
            {
                for (auto& cert : module.fieldCertificates)
                    if (cert.typeIdentity.ends_with("::GenericInheritance.Base"))
                    {
                        cert.typeIdentity += "_Wrong";
                        ++changed;
                    }
            }
            else if (mode == "wrong-template" || mode == "wrong-template-base")
            {
                for (auto& cert : module.templateFieldCertificates)
                {
                    if (cert.templateKey.find("Box") == std::string::npos) continue;
                    const std::string_view original = mode == "wrong-template" ? "Same" : "GenericInheritance.Base";
                    const std::string_view replacement = mode == "wrong-template" ? "Other" : "GenericInheritance.Evil";
                    const auto pos = cert.sourceText.find(original);
                    if (pos != std::string::npos)
                    {
                        cert.sourceText.replace(pos, original.size(), replacement);
                        ++changed;
                    }
                }
            }
        }
        const bool expectedMutation = mode == "control" ? changed == 0 : changed == 1;
        tests.expect(expectedMutation, "inherited certificate fixture alters the intended certificate exactly once");
        if (!expectedMutation) continue;
        const bool ok = context.compileProjectMethodBodies(project, error);
        if (mode == "control")
            tests.expect(ok, "unchanged inherited generic certificate is accepted");
        else
        {
            const std::string_view expected = mode == "wrong-template" || mode == "wrong-template-base"
                ? "static field iteration cannot verify the materialized generic type"
                : "static field iteration cannot verify the generic base identity";
            bool matched = false;
            for (const auto& diagnostic : context.diagnostics().diagnostics())
                matched = matched || diagnostic.message().find(expected) != std::string::npos;
            tests.expect(!ok && matched, "inconsistent inherited certificate is rejected at the expected boundary");
        }
        if ((mode == "control" && !ok) || (mode != "control" && ok))
            std::cerr << mode << ": unexpected result: " << error << '\n';
    }
}

// These mutations apply to prepared certificates in memory, never to metadata on disk.
void verifyGenericBaseTemplateCertificates(nativesharp::tests::TestContext& tests)
{
    using namespace nativesharp;
    std::string project;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NSC_TEST_GENERIC_BASE_CERT_PROJECT") == 0 && value != nullptr)
        project.assign(value);
    std::free(value);
#else
    if (const char* value = std::getenv("NSC_TEST_GENERIC_BASE_CERT_PROJECT"); value != nullptr)
        project.assign(value);
#endif
    if (project.empty())
    {
        std::cout << "SKIP generic base template certificates: NSC_TEST_GENERIC_BASE_CERT_PROJECT is unset\n";
        return;
    }
    for (const std::string_view mode : {"control", "missing-base", "duplicate-base", "wrong-identity",
             "wrong-module", "wrong-box-base"})
    {
        frontend::ProjectCompilation context;
        const compilation::ProjectLoadOptions options;
        compilation::ProjectGraphLoader loader(1, {}, options);
        std::string error;
        auto graph = loader.load(project, error);
        const bool prepared = graph.has_value() && compilation::preparePrecompiledProject(
            std::move(*graph), options, context, error);
        tests.expect(prepared, "generic base certificate fixture prepares binary modules");
        if (!prepared)
        {
            std::cerr << mode << ": preparation failed: " << error << '\n';
            continue;
        }
        std::size_t changed = 0;
        for (const auto& view : context.precompiledModules())
        {
            auto& certs = const_cast<frontend::PrecompiledSyntaxModule&>(view).templateFieldCertificates;
            const auto isBase = [](const auto& cert) {
                return cert.templateKey.ends_with("::GenericInheritance.Base`1");
            };
            if (mode == "missing-base")
            {
                const auto before = certs.size();
                std::erase_if(certs, isBase);
                changed += before - certs.size();
            }
            else if (mode == "duplicate-base")
            {
                const auto found = std::ranges::find_if(certs, isBase);
                if (found != certs.end()) { auto copy = *found; certs.push_back(std::move(copy)); ++changed; }
            }
            else if (mode == "wrong-identity" || mode == "wrong-module")
            {
                for (auto& cert : certs) if (isBase(cert))
                {
                    if (mode == "wrong-identity") cert.templateKey += "_Wrong";
                    else cert.templateKey.replace(0, cert.templateKey.find("::"), "DifferentProducer");
                    ++changed;
                }
            }
            else if (mode == "wrong-box-base")
            {
                for (auto& cert : certs) if (cert.templateKey.ends_with("::GenericInheritance.Box`1"))
                {
                    const auto at = cert.sourceText.find("Base<T>");
                    if (at != std::string::npos) { cert.sourceText.replace(at, 7, "Evil<T>"); ++changed; }
                }
            }
        }
        const bool expectedMutation = mode == "control" ? changed == 0 : changed == 1;
        tests.expect(expectedMutation, "generic base fixture mutates exactly the intended certificate");
        if (!expectedMutation) continue;
        const bool ok = context.compileProjectMethodBodies(project, error);
        if (mode == "control") tests.expect(ok, "unchanged generic base certificate is accepted");
        else
        {
            bool matched = false;
            for (const auto& diagnostic : context.diagnostics().diagnostics())
                matched = matched || diagnostic.message().find(
                    "static field iteration cannot verify the materialized generic type") != std::string::npos;
            tests.expect(!ok && matched, "inconsistent generic base certificate is rejected at the expected boundary");
        }
        if ((mode == "control" && !ok) || (mode != "control" && ok))
            std::cerr << mode << ": unexpected result: " << error << '\n';
    }
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    verifyStaticFieldRewriter(tests);
    verifyStaticFieldNameOfRejection(tests);
    verifyNestedFieldAccessRewrite(tests);
    verifyInvalidFieldAccessRejected(tests);
    verifyStaticFieldControlFlow(tests);
    using namespace nativesharp::frontend;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f01";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Types.ns", R"(
namespace Meta;
public struct Pair
{
    private int Hidden;
    public int X;
    public long Y;
    public int Sum() { return X + (int)Y; }
}
internal struct Secret { public int Value; }
)");

    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectObjectLayouts(root / "Demo.nsproj", error),
        "F01 project reaches the real names, members, and layout pipeline");
    ProjectSyntaxUnit unknown(0, 0, "Unknown", nativesharp::FileId{}, true);
    tests.expect(unknown.isPrecompiled() && unknown.origin() == SyntaxUnitOrigin::Unknown,
        "precompiled marker alone does not prove metadata origin");
    bool sourceOrigin = false;
    for (const auto& unit : compilation.units())
        sourceOrigin = sourceOrigin || unit->origin() == SyntaxUnitOrigin::Source;
    tests.expect(sourceOrigin, "ordinary source unit has source origin");
    verifyBinaryOrigins(tests);
    verifyBinaryFieldMetadata(tests);
    verifyInheritedGenericCertificates(tests);
    verifyGenericBaseTemplateCertificates(tests);
    const ProjectMetaprogrammingModel* model = compilation.metaprogrammingModel();
    tests.expect(model != nullptr && model->isBuilt(),
        "typed introspection is attached to ProjectCompilation");
    if (model != nullptr)
    {
        const StaticTypeInfo* pair = model->findType("Demo::Meta.Pair", 99);
        tests.expect(pair != nullptr && pair->size == 16 && pair->alignment == 8,
            "authorized compile-time layout is exposed as typed scalar data");
        if (pair != nullptr)
        {
            const auto external = model->visibleMembers(*pair, 99);
            tests.expect(external.size() == 3 && external[0].name == "X" &&
                    external[1].name == "Y" && external[2].name == "Sum",
                "external introspection is source-stable and filters private members");
            const auto internal = model->visibleMembers(*pair, pair->ownerProjectOrdinal);
            tests.expect(internal.size() == 4 && internal[0].name == "Hidden",
                "same-module introspection can inspect private members");
        }
        tests.expect(model->findType("Demo::Meta.Secret", 99) == nullptr,
            "private module types remain invisible to external introspection");
        tests.expect(!model->emitsRuntimeMetadata(),
            "typed introspection does not request a runtime reflection registry");
    }
    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
