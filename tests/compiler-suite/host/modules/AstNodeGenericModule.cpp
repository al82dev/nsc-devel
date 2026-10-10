#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::GenericParameterSyntax;
using nativesharp::ast::GenericParameterSyntaxKind;
using nativesharp::ast::TypeSyntax;
using nativesharp::lexer::TokenKind;

// Builds an explicit range for generic-record preservation checks. Complexity is O(1).
constexpr SourceRange makeRange(std::uint32_t begin, std::uint32_t end)
{
    return SourceRange(SourceLocation(FileId{66}, begin), SourceLocation(FileId{66}, end));
}

// Compares all public coordinates of two ranges. Complexity is O(1).
constexpr bool sameRange(SourceRange left, SourceRange right)
{
    return left.begin() == right.begin() && left.end() == right.end();
}

// Registers one AstNode generic-parameter case. Complexity is O(1).
void addGenericCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-007", "Host.AST", "unit", std::move(profile), {"ast", "node", "generic"},
        std::move(function),
    });
}

// Registers empty legacy and structured generic collections. Complexity is O(1).
void registerGenericDefaultsCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-061", "PR", [](TestContext& context) {
        const AstNode node(AstNodeKind::CompilationUnit, {});
        context.expect(node.genericParameters().empty(), "TA-AST-061 legacy generic names must default empty");
        context.expect(node.genericParameterSyntaxes().empty(),
            "TA-AST-061 structured generic records must default empty");
    });
}

// Registers alignment produced by the legacy string overload. Complexity is O(1).
void registerStringOverloadCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-062", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addGenericParameter("T");
        context.expect(node.genericParameters().size() == 1 && node.genericParameters()[0] == "T",
            "TA-AST-062 legacy name must be appended");
        context.expect(node.genericParameterSyntaxes().size() == 1,
            "TA-AST-062 structured record must be appended");
        context.expect(node.genericParameterSyntaxes()[0].name == "T" &&
                node.genericParameterSyntaxes()[0].kind == GenericParameterSyntaxKind::Type,
            "TA-AST-062 generated record must be a matching Type parameter");
    });
}

// Registers alignment and pointer preservation for a constant generic record. Complexity is O(1).
void registerRecordOverloadCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-063", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax constantType({}, TokenKind::KwInt, "int");
        node.addGenericParameter({"N", GenericParameterSyntaxKind::Constant, &constantType, makeRange(1, 2)});
        context.expect(node.genericParameters().size() == 1 && node.genericParameters()[0] == "N",
            "TA-AST-063 legacy name must align with record");
        const auto& record = node.genericParameterSyntaxes()[0];
        context.expect(record.kind == GenericParameterSyntaxKind::Constant && record.constantType == &constantType,
            "TA-AST-063 constant kind and type identity must be exact");
    });
}

// Registers preservation of both pack kinds. Complexity is O(1).
void registerPackKindsCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-064", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addGenericParameter({"Types", GenericParameterSyntaxKind::TypePack, nullptr, {}});
        node.addGenericParameter({"Values", GenericParameterSyntaxKind::ConstantPack, nullptr, {}});
        const auto& records = node.genericParameterSyntaxes();
        context.expect(records.size() == 2, "TA-AST-064 both pack records must be present");
        context.expect(records[0].kind == GenericParameterSyntaxKind::TypePack,
            "TA-AST-064 TypePack must be preserved");
        context.expect(records[1].kind == GenericParameterSyntaxKind::ConstantPack,
            "TA-AST-064 ConstantPack must be preserved");
    });
}

// Registers that empty and duplicate names are retained without syntax validation. Complexity is O(1).
void registerUnvalidatedNamesCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-065", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addGenericParameter("");
        node.addGenericParameter("T");
        node.addGenericParameter("T");
        const auto& names = node.genericParameters();
        context.expect(names.size() == 3, "TA-AST-065 empty and duplicate names must be retained");
        context.expect(names[0].empty() && names[1] == "T" && names[2] == "T",
            "TA-AST-065 name order must be exact");
    });
}

// Registers preservation of a complete record with a nondefault range. Complexity is O(1).
void registerRecordRangeCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-066", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax type({}, TokenKind::KwUInt, "uint");
        const SourceRange range = makeRange(4, 19);
        node.addGenericParameter({"Size", GenericParameterSyntaxKind::Constant, &type, range});
        const auto& record = node.genericParameterSyntaxes()[0];
        context.expect(record.name == "Size" && record.constantType == &type,
            "TA-AST-066 record name and pointer must be exact");
        context.expect(sameRange(record.range, range), "TA-AST-066 record range must be exact");
    });
}

// Registers synchronized clearing of legacy and structured generic vectors. Complexity is O(n).
void registerClearGenericsCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-067", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addGenericParameter("T");
        node.addGenericParameter("U");
        node.clearGenericParameters();
        context.expect(node.genericParameters().empty(), "TA-AST-067 legacy names must clear");
        context.expect(node.genericParameterSyntaxes().empty(), "TA-AST-067 structured records must clear");
    });
}

// Registers correct realignment when adding a parameter after clear. Complexity is O(n).
void registerAppendAfterClearCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-068", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addGenericParameter("Old");
        node.clearGenericParameters();
        node.addGenericParameter("New");
        context.expect(node.genericParameters().size() == 1 && node.genericParameters()[0] == "New",
            "TA-AST-068 legacy vector must restart with New");
        context.expect(node.genericParameterSyntaxes().size() == 1 &&
                node.genericParameterSyntaxes()[0].name == "New",
            "TA-AST-068 structured vector must restart aligned with New");
    });
}

// Registers owned names after temporary string and record inputs are destroyed. Complexity is O(n).
void registerTemporaryInputsCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-069", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addGenericParameter(std::string("TemporaryString"));
        GenericParameterSyntax record;
        record.name = std::string("TemporaryRecord");
        node.addGenericParameter(std::move(record));
        context.expect(node.genericParameters()[0] == "TemporaryString",
            "TA-AST-069 string-overload name must be owned");
        context.expect(node.genericParameters()[1] == "TemporaryRecord" &&
                node.genericParameterSyntaxes()[1].name == "TemporaryRecord",
            "TA-AST-069 moved record name must be owned in both vectors");
    });
}

// Registers index alignment over a large mixed generic-parameter sequence. Complexity is O(n).
void registerGenericScaleCase(TestRegistry& registry)
{
    addGenericCase(registry, "TA-AST-070", "NIGHTLY", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        constexpr std::size_t count = 4096;
        for (std::size_t index = 0; index < count; ++index)
        {
            GenericParameterSyntax value;
            value.name = "P" + std::to_string(index);
            value.kind = static_cast<GenericParameterSyntaxKind>(index % 4);
            node.addGenericParameter(std::move(value));
        }
        const auto& names = node.genericParameters();
        const auto& records = node.genericParameterSyntaxes();
        context.expect(names.size() == count && records.size() == count,
            "TA-AST-070 both generic vectors must contain every record");
        for (std::size_t index = 0; index < count; ++index)
        {
            context.expect(names[index] == records[index].name,
                "TA-AST-070 every legacy name must align with its structured record");
        }
    });
}
}

// Registers CA-AST-007 synchronized generic-parameter storage contracts.
void registerAstNodeGenericModule(TestRegistry& registry)
{
    registerGenericDefaultsCase(registry);
    registerStringOverloadCase(registry);
    registerRecordOverloadCase(registry);
    registerPackKindsCase(registry);
    registerUnvalidatedNamesCase(registry);
    registerRecordRangeCase(registry);
    registerClearGenericsCase(registry);
    registerAppendAfterClearCase(registry);
    registerTemporaryInputsCase(registry);
    registerGenericScaleCase(registry);
}
}
