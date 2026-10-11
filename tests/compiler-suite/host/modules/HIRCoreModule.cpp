#include "HIRContractSupport.h"

#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace nsc::suite
{
namespace
{
using hir_contract_support::addMatrix;
using nativesharp::SourceRange;
using nativesharp::hir::HirArena;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using nativesharp::hir::hirNodeKindName;
using nativesharp::lexer::TokenKind;
using nativesharp::sema::ConstantValue;
using nativesharp::sema::ConversionKind;
using nativesharp::sema::Symbol;
using nativesharp::sema::SymbolKind;
using nativesharp::sema::TypeContext;
using nativesharp::sema::TypeKind;

void arenaOracle(TestContext& context, std::size_t variant)
{
    HirArena arena;
    const std::size_t count = variant == 7 ? 4096 : variant + 1;
    std::vector<HirNode*> nodes;
    nodes.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        nodes.push_back(arena.make(
            index % 2 == 0 ? HirNodeKind::Constant : HirNodeKind::Block, SourceRange()));
    }
    context.expect(arena.size() == count, "CF-HIR-001 arena size must equal successful allocations");
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        context.expect(nodes[index] != nullptr, "CF-HIR-001 allocated pointer must be non-null");
        context.expect(nodes[index]->kind() ==
                (index % 2 == 0 ? HirNodeKind::Constant : HirNodeKind::Block),
            "CF-HIR-001 allocation order must preserve node identity");
    }
    context.expect(!std::is_copy_constructible_v<HirArena>, "CF-HIR-001 arena must remain non-copyable");
}

void identityOracle(TestContext& context, std::size_t variant)
{
    TypeContext types;
    Symbol symbol(SymbolKind::Variable, "value", types.builtin(TypeKind::Int), nullptr);
    HirNode node(static_cast<HirNodeKind>(variant % 34), SourceRange());
    node.setType(types.builtin(TypeKind::Int));
    node.setOperationType(types.builtin(TypeKind::Long));
    node.setSymbol(&symbol);
    node.setOperation(TokenKind::Plus);
    context.expect(node.kind() == static_cast<HirNodeKind>(variant % 34),
        "CF-HIR-002 kind identity must be exact");
    context.expect(node.type() == types.builtin(TypeKind::Int), "CF-HIR-002 result type identity must be exact");
    context.expect(node.operationType() == types.builtin(TypeKind::Long),
        "CF-HIR-002 operation type identity must be exact");
    context.expect(node.symbol() == &symbol && node.operation() == TokenKind::Plus,
        "CF-HIR-002 symbol and token identity must be exact");
}

void constantOracle(TestContext& context, std::size_t variant)
{
    HirNode node(HirNodeKind::Constant, SourceRange());
    const std::array values{ConstantValue::signedInteger(-7), ConstantValue::unsignedInteger(9),
        ConstantValue::floating(1.25), ConstantValue::boolean(true), ConstantValue::nullValue(),
        ConstantValue::stringValue("a\0b"), ConstantValue::signedInteger(0),
        ConstantValue::unsignedInteger(std::numeric_limits<std::uint64_t>::max())};
    context.expect(!node.hasConstant() && !node.hasConversion() && !node.hasReadConversion() &&
            !node.hasResultConversion(),
        "CF-HIR-003 unset payloads must remain distinguishable");
    node.setConstant(values[variant]);
    node.setConversionKind(ConversionKind::Identity);
    node.setReadConversionKind(ConversionKind::IntegralPromotion);
    node.setResultConversionKind(ConversionKind::ExplicitNumeric);
    context.expect(node.hasConstant(), "CF-HIR-003 selected constant payload must be present");
    context.expect(node.hasConversion() && node.conversionKind() == ConversionKind::Identity,
        "CF-HIR-003 operand conversion channel must be independent");
    context.expect(node.hasReadConversion() && node.readConversionKind() == ConversionKind::IntegralPromotion,
        "CF-HIR-003 read conversion channel must be independent");
    context.expect(node.hasResultConversion() && node.resultConversionKind() == ConversionKind::ExplicitNumeric,
        "CF-HIR-003 result conversion channel must be independent");
}

void flagsOracle(TestContext& context, std::size_t variant)
{
    HirNode node(HirNodeKind::IndexAddress, SourceRange());
    node.setRequiresBoundsCheck((variant & 1U) != 0);
    node.setArrayForm((variant & 2U) != 0);
    node.setPostfix((variant & 4U) != 0);
    context.expect(node.requiresBoundsCheck() == ((variant & 1U) != 0),
        "CF-HIR-004 bounds flag must preserve every combination");
    context.expect(node.arrayForm() == ((variant & 2U) != 0),
        "CF-HIR-004 array flag must preserve every combination");
    context.expect(node.postfix() == ((variant & 4U) != 0),
        "CF-HIR-004 postfix flag must preserve every combination");
    context.expect(!node.hasOffset(), "CF-HIR-004 absent offset must be observable");
    const std::uint64_t offset = variant == 7 ? std::numeric_limits<std::uint64_t>::max() : variant;
    node.setOffset(offset);
    context.expect(node.hasOffset() && node.offset() == offset,
        "CF-HIR-004 zero and boundary offsets must retain presence and value");
}

void edgesOracle(TestContext& context, std::size_t variant)
{
    HirArena arena;
    HirNode* root = arena.make(HirNodeKind::For, SourceRange());
    HirNode* first = arena.make(HirNodeKind::Constant, SourceRange());
    HirNode* second = arena.make(HirNodeKind::Load, SourceRange());
    HirNode* third = arena.make(HirNodeKind::Block, SourceRange());
    root->setFirst(variant == 2 ? nullptr : first);
    root->setSecond(variant == 3 ? nullptr : second);
    root->setThird(variant == 4 ? nullptr : third);
    const std::size_t children = variant == 7 ? 1024 : variant;
    for (std::size_t index = 0; index < children; ++index)
    {
        root->addChild(arena.make(HirNodeKind::ExpressionStatement, SourceRange()));
    }
    context.expect(root->first() == (variant == 2 ? nullptr : first),
        "CF-HIR-005 first edge identity must be exact");
    context.expect(root->second() == (variant == 3 ? nullptr : second),
        "CF-HIR-005 second edge identity must be exact");
    context.expect(root->third() == (variant == 4 ? nullptr : third),
        "CF-HIR-005 third edge identity must be exact");
    context.expect(root->children().size() == children,
        "CF-HIR-005 ordered child count must be exact");
}

void namesOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 35> names{"CompilationUnit", "StructDeclaration",
        "FieldDeclaration", "EnumDeclaration", "EnumMemberDeclaration", "GlobalVariableDeclaration",
        "FunctionDeclaration", "ParameterDeclaration", "Block", "LocalVariableDeclaration", "If", "While",
        "For", "Break", "Continue", "Return", "ExpressionStatement", "Constant", "SymbolAddress", "Load",
        "Store", "Conversion", "Unary", "Binary", "Conditional", "Call", "IndexAddress", "FieldAddress",
        "DereferenceAddress", "ExtractField", "CompoundAssignment", "Increment", "Allocation", "Deallocation",
        "StackAllocation"};
    const std::size_t begin = variant == 7 ? 0 : variant * 4;
    const std::size_t end = variant == 7 ? names.size() : std::min(names.size(), begin + 6);
    for (std::size_t index = begin; index < end; ++index)
    {
        context.expect(hirNodeKindName(static_cast<HirNodeKind>(index)) == names[index],
            "CF-HIR-006 enum-to-name mapping must be exhaustive and exact");
    }
    context.expect(hirNodeKindName(static_cast<HirNodeKind>(999)) == "Unknown",
        "CF-HIR-006 invalid enum values must map to Unknown");
}
}

// Registers CF-HIR-001..006 low-level ownership and node-record matrices.
void registerHIRCoreModule(TestRegistry& registry)
{
    addMatrix(registry, 1, "unit", arenaOracle);
    addMatrix(registry, 2, "unit", identityOracle);
    addMatrix(registry, 3, "unit", constantOracle);
    addMatrix(registry, 4, "unit", flagsOracle);
    addMatrix(registry, 5, "unit", edgesOracle);
    addMatrix(registry, 6, "unit", namesOracle);
}
}
