#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"

#include "Cache/CachePayloadWriter.h"

#include <cstdint>

namespace nativesharp::frontend
{
namespace
{
constexpr std::uint8_t kNodeTag = 0U;
constexpr std::uint8_t kTypeTag = 1U;

// EN: Appends the slots of one entry to the work list in reverse, so they pop in slot order.
// ES: Agrega las ranuras de una entrada a la lista de trabajo al revés, para que salgan en orden.
void pushReversed(
    const std::vector<const void*>& slots,
    const std::vector<bool>& slotIsType,
    std::vector<const void*>& work,
    std::vector<bool>& isType)
{
    for (std::size_t index = slots.size(); index > 0; --index)
    {
        if (slots[index - 1] == nullptr) continue;
        work.push_back(slots[index - 1]);
        isType.push_back(slotIsType[index - 1]);
    }
}

// EN: The declaration metadata slots of a node: generic parameter types and constraint types.
// ES: Las ranuras de metadata de declaración de un nodo: tipos de parámetros y de restricciones.
void appendDeclarationSlots(
    const ast::AstNode& node,
    std::vector<const void*>& slots,
    std::vector<bool>& slotIsType)
{
    for (const ast::GenericParameterSyntax& parameter : node.genericParameterSyntaxes())
    {
        slots.push_back(parameter.constantType);
        slotIsType.push_back(true);
    }
    for (const ast::GenericConstraintClause& clause : node.genericConstraints())
    {
        slots.push_back(clause.equalityConstraint);
        slotIsType.push_back(true);
        slots.push_back(clause.associatedProjection);
        slotIsType.push_back(true);
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
        {
            slots.push_back(constraint);
            slotIsType.push_back(true);
        }
        slots.push_back(clause.substitutedArgument);
        slotIsType.push_back(true);
    }
}
}

void PrecompiledSyntaxNodeIndex::build(const ast::AstNode* root)
{
    nodes_.clear();
    types_.clear();
    nodeIndexes_.clear();
    typeIndexes_.clear();
    kindStream_.clear();
    std::vector<const void*> work;
    std::vector<bool> isType;
    if (root != nullptr)
    {
        work.push_back(root);
        isType.push_back(false);
    }
    while (!work.empty())
    {
        const void* const entry = work.back();
        const bool entryIsType = isType.back();
        work.pop_back();
        isType.pop_back();
        if (entryIsType)
            visitType(static_cast<const ast::TypeSyntax*>(entry), work, isType);
        else
            visitNode(static_cast<const ast::AstNode*>(entry), work, isType);
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(kindStream_.data()), kindStream_.size()};
    kindDigest_ = basic::sha256Hex(basic::sha256(bytes));
    kindStream_.clear();
}

void PrecompiledSyntaxNodeIndex::visitNode(
    const ast::AstNode* node,
    std::vector<const void*>& work,
    std::vector<bool>& isType)
{
    nodeIndexes_.try_emplace(node, nodes_.size());
    nodes_.push_back(node);
    internal::CachePayloadWriter::byte(kindStream_, kNodeTag);
    internal::CachePayloadWriter::u32(kindStream_, static_cast<std::uint32_t>(node->kind()));
    // EN: Slot order of the syntax product serializer: types, declaration metadata, the three
    //     fixed operands, then the ordered children.
    // ES: Orden de ranuras del serializador del producto sintáctico: tipos, metadata de
    //     declaración, los tres operandos fijos y luego los hijos en orden.
    std::vector<const void*> slots = {node->typeSyntax(), node->throwsTypeSyntax()};
    std::vector<bool> slotIsType = {true, true};
    for (const ast::TypeSyntax* type : node->typeSyntaxes())
    {
        slots.push_back(type);
        slotIsType.push_back(true);
    }
    appendDeclarationSlots(*node, slots, slotIsType);
    for (const ast::AstNode* operand : {node->first(), node->second(), node->third()})
    {
        slots.push_back(operand);
        slotIsType.push_back(false);
    }
    for (const ast::AstNode* child : node->children())
    {
        slots.push_back(child);
        slotIsType.push_back(false);
    }
    pushReversed(slots, slotIsType, work, isType);
}

void PrecompiledSyntaxNodeIndex::visitType(
    const ast::TypeSyntax* type,
    std::vector<const void*>& work,
    std::vector<bool>& isType)
{
    typeIndexes_.try_emplace(type, types_.size());
    types_.push_back(type);
    internal::CachePayloadWriter::byte(kindStream_, kTypeTag);
    internal::CachePayloadWriter::u32(
        kindStream_, static_cast<std::uint32_t>(type->baseTokenKind()));
    // EN: Slot order of the serializer: arguments, function result and error, array counts of the
    //     modifiers, then the const-generic expression.
    // ES: Orden de ranuras del serializador: argumentos, resultado y error de función, cuentas de
    //     arreglo de los modificadores y luego la expresión de genérico constante.
    std::vector<const void*> slots;
    std::vector<bool> slotIsType;
    for (const ast::TypeSyntax* argument : type->typeArguments())
    {
        slots.push_back(argument);
        slotIsType.push_back(true);
    }
    slots.push_back(type->functionReturnType());
    slotIsType.push_back(true);
    slots.push_back(type->functionErrorType());
    slotIsType.push_back(true);
    for (const ast::TypeModifierSyntax& modifier : type->modifiers())
    {
        slots.push_back(modifier.arrayCount);
        slotIsType.push_back(false);
    }
    slots.push_back(type->constExpression());
    slotIsType.push_back(false);
    pushReversed(slots, slotIsType, work, isType);
}

std::span<const ast::AstNode* const> PrecompiledSyntaxNodeIndex::nodes() const noexcept
{
    return nodes_;
}

std::span<const ast::TypeSyntax* const> PrecompiledSyntaxNodeIndex::types() const noexcept
{
    return types_;
}

std::optional<std::size_t> PrecompiledSyntaxNodeIndex::indexOf(
    const ast::AstNode* node) const noexcept
{
    const auto found = nodeIndexes_.find(node);
    if (found == nodeIndexes_.end()) return std::nullopt;
    return found->second;
}

std::optional<std::size_t> PrecompiledSyntaxNodeIndex::indexOf(
    const ast::TypeSyntax* type) const noexcept
{
    const auto found = typeIndexes_.find(type);
    if (found == typeIndexes_.end()) return std::nullopt;
    return found->second;
}

const std::string& PrecompiledSyntaxNodeIndex::kindDigest() const noexcept
{
    return kindDigest_;
}
}
