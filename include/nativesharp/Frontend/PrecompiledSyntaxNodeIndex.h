#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
class TypeSyntax;
}

namespace nativesharp::frontend
{
// EN: Index of the syntax tree of one public API unit, built once per unit. Nodes and type
//     syntaxes are numbered in a preorder walk that visits the slots in the order the syntax
//     product serializer writes them. The parser shares some subtrees (one type for several
//     declarators) and the serializer writes a shared subtree once per occurrence, so every
//     occurrence gets its own index and the restored tree indexes alike; indexOf() returns the
//     first occurrence. Snapshot entries refer to syntax by these relative indexes, never by
//     pointer or absolute ordinal.
// ES: Índice del árbol sintáctico de una unidad de API pública, construido una vez por unidad. Los
//     nodos y los type syntax se numeran en un recorrido preorden que visita las ranuras en el
//     orden en que las escribe el serializador del producto sintáctico. El parser comparte algunos
//     subárboles (un tipo para varios declaradores) y el serializador escribe un subárbol
//     compartido una vez por aparición, así que cada aparición recibe su propio índice y el árbol
//     restaurado se indexa igual; indexOf() regresa la primera aparición. Las entradas del snapshot
//     se refieren a la sintaxis por estos índices relativos, nunca por puntero ni por ordinal
//     absoluto.
class PrecompiledSyntaxNodeIndex final
{
public:
    void build(const ast::AstNode* root);

    std::span<const ast::AstNode* const> nodes() const noexcept;
    std::span<const ast::TypeSyntax* const> types() const noexcept;
    std::optional<std::size_t> indexOf(const ast::AstNode* node) const noexcept;
    std::optional<std::size_t> indexOf(const ast::TypeSyntax* type) const noexcept;

    // EN: SHA-256 (hex) of the walk: a tag (node or type) and the node kind or the base token kind
    //     of every entry, in index order. Two trees with the same digest and counts have the same
    //     shape for the snapshot.
    // ES: SHA-256 (hex) del recorrido: una marca (nodo o tipo) y la clase de nodo o el token base de
    //     cada entrada, en orden de índice. Dos árboles con el mismo digest y cantidades tienen la
    //     misma forma para el snapshot.
    const std::string& kindDigest() const noexcept;

private:
    void visitNode(const ast::AstNode* node, std::vector<const void*>& work, std::vector<bool>& isType);
    void visitType(const ast::TypeSyntax* type, std::vector<const void*>& work, std::vector<bool>& isType);

    std::vector<const ast::AstNode*> nodes_;
    std::vector<const ast::TypeSyntax*> types_;
    std::unordered_map<const ast::AstNode*, std::size_t> nodeIndexes_;
    std::unordered_map<const ast::TypeSyntax*, std::size_t> typeIndexes_;
    std::string kindStream_;
    std::string kindDigest_;
};
}
