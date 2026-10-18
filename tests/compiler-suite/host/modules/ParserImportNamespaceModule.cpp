#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Returns a parsed root while retaining fixture-owned storage for the caller's assertions. Complexity is O(n).
AstNode* parse(ParserFixture& fixture)
{
    return fixture.parse();
}

// Returns the requested top-level child or null when recovery produced a smaller tree. Complexity is O(1).
AstNode* childAt(AstNode* root, std::size_t index)
{
    return root != nullptr && index < root->children().size() ? root->children()[index] : nullptr;
}

// Registers a parser case in the import/namespace slice. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, std::string contract, TestFunction function)
{
    addParserCase(registry, std::move(id), std::move(contract), "component", "PR", std::move(function));
}

// Registers namespace and symbol import projections. Complexity is O(n).
void registerImportKindsCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-110", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import module A.B; import symbol A.Value;");
        AstNode* root = parse(fixture);
        AstNode* module = childAt(root, 0);
        AstNode* symbol = childAt(root, 1);
        context.expect(module != nullptr && module->kind() == AstNodeKind::UsingNamespaceDirective &&
                module->text() == "A.B" && module->integerValue() == 1,
            "TP-PAR-110 module import must preserve namespace target and kind");
        context.expect(symbol != nullptr && symbol->kind() == AstNodeKind::UsingAliasDirective &&
                symbol->text() == "Value" && symbol->auxiliaryText() == "A.Value" && symbol->integerValue() == 2,
            "TP-PAR-110 symbol import must preserve local name, target, and kind");
        context.expect(fixture.diagnostics().diagnostics().empty(), "TP-PAR-110 must parse without diagnostics");
    });
}

// Registers explicit import alias and target identity. Complexity is O(n).
void registerImportAliasCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-111", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import symbol A.Value as Local;");
        AstNode* node = childAt(parse(fixture), 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::UsingAliasDirective,
            "TP-PAR-111 aliased import must use alias node");
        context.expect(node != nullptr && node->text() == "Local" && node->auxiliaryText() == "A.Value",
            "TP-PAR-111 alias and target must be exact");
    });
}

// Registers trait and extension import metadata kinds. Complexity is O(n).
void registerSpecializedImportsCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-112", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import trait A.Trait; import extensions A.Extensions;");
        AstNode* root = parse(fixture);
        AstNode* trait = childAt(root, 0);
        AstNode* extensions = childAt(root, 1);
        context.expect(trait != nullptr && trait->integerValue() == 3 && trait->text() == "Trait",
            "TP-PAR-112 trait import kind and derived local name must be exact");
        context.expect(extensions != nullptr && extensions->integerValue() == 4 &&
                extensions->text() == "A.Extensions",
            "TP-PAR-112 extension import kind and target must be exact");
    });
}

// Registers the current restricted-glob representation. Complexity is O(n).
void registerRestrictedGlobCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-113", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import A.B.*;");
        AstNode* node = childAt(parse(fixture), 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::UsingNamespaceDirective,
            "TP-PAR-113 restricted glob must use namespace node");
        context.expect(node != nullptr && node->text() == "A.B" && node->integerValue() == 5,
            "TP-PAR-113 restricted glob target and kind must be preserved");
    });
}

// Registers reexport flags independently from import target and kind. Complexity is O(n).
void registerReexportCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-114", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("export import module A.B; export import symbol A.C;");
        AstNode* root = parse(fixture);
        AstNode* module = childAt(root, 0);
        AstNode* symbol = childAt(root, 1);
        context.expect(module != nullptr && module->flag() && module->text() == "A.B" &&
                module->integerValue() == 1,
            "TP-PAR-114 module reexport must retain target and kind");
        context.expect(symbol != nullptr && symbol->flag() && symbol->auxiliaryText() == "A.C" &&
                symbol->integerValue() == 2,
            "TP-PAR-114 symbol reexport must retain target and kind");
    });
}

// Registers a missing import target as an expected-token failure. Complexity is O(n).
void registerMissingImportTargetCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-115", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import module ;");
        AstNode* root = parse(fixture);
        context.expect(root != nullptr && root->children().empty(), "TP-PAR-115 invalid import must add no child");
        context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
            "TP-PAR-115 missing target must diagnose ExpectedToken");
    });
}

// Registers duplicate aliases as two syntax nodes because collision checking is deferred. Complexity is O(n).
void registerDuplicateAliasSyntaxCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-116", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import symbol A.One as Same; import symbol B.Two as Same;");
        AstNode* root = parse(fixture);
        AstNode* first = childAt(root, 0);
        AstNode* second = childAt(root, 1);
        context.expect(root != nullptr && root->children().size() == 2,
            "TP-PAR-116 both duplicate-alias syntax nodes must be retained");
        context.expect(first != nullptr && second != nullptr &&
                first->text() == "Same" && second->text() == "Same",
            "TP-PAR-116 both local alias spellings must remain exact");
    });
}

// Registers recovery from a missing import terminator to the following declaration. Complexity is O(n).
void registerMissingImportTerminatorCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-117", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import module A.B\nint value;");
        AstNode* root = parse(fixture);
        context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
            "TP-PAR-117 missing semicolon must diagnose ExpectedToken");
        context.expect(root != nullptr && root->children().size() >= 2,
            "TP-PAR-117 recovery must preserve the following declaration");
    });
}

// Registers case-sensitive contextual import recognition. Complexity is O(n).
void registerImportCaseSensitivityCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-118", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("Import module A.B; int value;");
        AstNode* root = parse(fixture);
        for (AstNode* node : root->children())
        {
            context.expect(node->kind() != AstNodeKind::UsingNamespaceDirective &&
                    node->kind() != AstNodeKind::UsingAliasDirective,
                "TP-PAR-118 differently cased spelling must not become a specialized import");
        }
        context.expect(!fixture.diagnostics().diagnostics().empty(),
            "TP-PAR-118 differently cased spelling must follow ordinary syntax diagnostics");
    });
}

// Registers import/declaration dispatch order at compilation-unit level. Complexity is O(n).
void registerImportThenDeclarationCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-119", "CP-PAR-012", [](TestContext& context) {
        ParserFixture fixture("import module A.B; int value;");
        AstNode* root = parse(fixture);
        context.expect(root != nullptr && root->children().size() == 2,
            "TP-PAR-119 import and declaration must both be retained");
        context.expect(childAt(root, 0)->kind() == AstNodeKind::UsingNamespaceDirective &&
                childAt(root, 1)->kind() == AstNodeKind::GlobalVariableDeclaration,
            "TP-PAR-119 child kinds and order must be exact");
    });
}

// Registers a braced namespace with exact name, child, and valid range. Complexity is O(n).
void registerBracedNamespaceCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-120", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("namespace A.B { int value; }");
        AstNode* node = childAt(parse(fixture), 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::NamespaceDeclaration &&
                node->text() == "A.B",
            "TP-PAR-120 braced namespace name and kind must be exact");
        context.expect(node != nullptr && node->children().size() == 1 && node->range().isValid(),
            "TP-PAR-120 braced namespace must retain member and valid range");
    });
}

// Registers file-scoped namespace membership through EOF. Complexity is O(n).
void registerFileScopedNamespaceCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-121", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("namespace A.B; int first; int second;");
        AstNode* node = childAt(parse(fixture), 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::NamespaceDeclaration && node->flag(),
            "TP-PAR-121 file-scoped namespace must carry its form flag");
        context.expect(node != nullptr && node->children().size() == 2,
            "TP-PAR-121 file-scoped namespace must own all following members");
    });
}

// Registers nested and qualified namespace structure. Complexity is O(n).
void registerNestedNamespacesCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-122", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("namespace Outer { namespace Inner.Deep { int value; } }");
        AstNode* outer = childAt(parse(fixture), 0);
        AstNode* inner = outer != nullptr ? childAt(outer, 0) : nullptr;
        context.expect(outer != nullptr && outer->text() == "Outer", "TP-PAR-122 outer name must be exact");
        context.expect(inner != nullptr && inner->kind() == AstNodeKind::NamespaceDeclaration &&
                inner->text() == "Inner.Deep",
            "TP-PAR-122 nested qualified namespace must be exact");
    });
}

// Registers an ordinary using-namespace directive. Complexity is O(n).
void registerUsingNamespaceCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-123", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("using A.B;");
        AstNode* node = childAt(parse(fixture), 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::UsingNamespaceDirective &&
                node->text() == "A.B",
            "TP-PAR-123 using namespace target must be exact");
    });
}

// Registers a using alias whose target is a full pointer type. Complexity is O(n).
void registerUsingAliasCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-124", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("using Ptr = int*; using Buffer<T> = T*;");
        AstNode* root = parse(fixture);
        AstNode* node = childAt(root, 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::UsingAliasDirective &&
                node->text() == "Ptr",
            "TP-PAR-124 using alias name and kind must be exact");
        context.expect(node != nullptr && node->typeSyntax() != nullptr &&
                node->typeSyntax()->baseTokenKind() == TokenKind::KwInt &&
                node->typeSyntax()->modifiers().size() == 1,
            "TP-PAR-124 using alias must retain full pointer TypeSyntax");
        AstNode* generic = childAt(root, 1);
        context.expect(generic != nullptr && generic->kind() == AstNodeKind::UsingAliasDirective &&
                generic->text() == "Buffer" && generic->genericParameters().size() == 1 &&
                generic->genericParameters()[0] == "T" && generic->typeSyntax() != nullptr &&
                generic->typeSyntax()->baseName() == "T" &&
                generic->typeSyntax()->modifiers().size() == 1,
            "TP-PAR-124 generic alias must retain parameter scope and pointer target");
    });
}

// Registers extension receiver, bounded body context, and synthesized receiver parameter. Complexity is O(n).
void registerExtensionCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-125", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("extension int { int Identity(int value) { return value; } }");
        AstNode* extension = childAt(parse(fixture), 0);
        AstNode* function = extension != nullptr && !extension->children().empty()
            ? extension->children()[0] : nullptr;
        context.expect(extension != nullptr && extension->kind() == AstNodeKind::ExtensionDeclaration &&
                extension->typeSyntax() != nullptr,
            "TP-PAR-125 extension receiver must be preserved");
        context.expect(extension != nullptr && extension->children().size() == 1 && function != nullptr &&
                function->kind() == AstNodeKind::FunctionDeclaration,
            "TP-PAR-125 extension body must retain its function");
        context.expect(function != nullptr && function->children().size() >= 2,
            "TP-PAR-125 extension function must receive a synthesized receiver before source parameters");
    });
}

// Registers a missing namespace close as a finite expected-token failure. Complexity is O(n).
void registerMissingNamespaceCloseCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-126", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("namespace A { int value;");
        AstNode* node = childAt(parse(fixture), 0);
        context.expect(node != nullptr && node->kind() == AstNodeKind::NamespaceDeclaration,
            "TP-PAR-126 partial namespace node must remain available");
        context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
            "TP-PAR-126 missing close must diagnose ExpectedToken");
    });
}

// Registers global recovery from an unknown token to the next declaration. Complexity is O(n).
void registerUnknownGlobalCase(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-127", "CP-PAR-013", [](TestContext& context) {
        ParserFixture fixture("@ ; int value;");
        AstNode* root = parse(fixture);
        context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
            "TP-PAR-127 unknown global token must diagnose ExpectedDeclaration");
        context.expect(root != nullptr && !root->children().empty() &&
                root->children().back()->kind() == AstNodeKind::GlobalVariableDeclaration,
            "TP-PAR-127 recovery must preserve the following declaration");
    });
}
}

// Registers TP-PAR-110..127 for imports, namespace forms, using directives, and recovery.
void registerParserImportNamespaceModule(TestRegistry& registry)
{
    registerImportKindsCase(registry);
    registerImportAliasCase(registry);
    registerSpecializedImportsCase(registry);
    registerRestrictedGlobCase(registry);
    registerReexportCase(registry);
    registerMissingImportTargetCase(registry);
    registerDuplicateAliasSyntaxCase(registry);
    registerMissingImportTerminatorCase(registry);
    registerImportCaseSensitivityCase(registry);
    registerImportThenDeclarationCase(registry);
    registerBracedNamespaceCase(registry);
    registerFileScopedNamespaceCase(registry);
    registerNestedNamespacesCase(registry);
    registerUsingNamespaceCase(registry);
    registerUsingAliasCase(registry);
    registerExtensionCase(registry);
    registerMissingNamespaceCloseCase(registry);
    registerUnknownGlobalCase(registry);
}
}
