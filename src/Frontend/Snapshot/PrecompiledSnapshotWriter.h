#pragma once

#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// EN: Writes the entries of one model section. Syntax is referenced by its relative index in the
//     API unit, other symbols by identity (type key, member signature, namespace symbol key,
//     specialization key) and symbols of the same unit by an ordinal relative to the unit; never by
//     pointer or absolute ordinal. A syntax pointer outside the index makes the writer fail, and
//     the producer then publishes no snapshot.
// ES: Escribe las entradas de una sección de modelo. La sintaxis se referencia por su índice
//     relativo en la unidad de API, los demás símbolos por identidad (llave de tipo, firma de
//     miembro, llave de símbolo de namespace, llave de especialización) y los símbolos de la misma
//     unidad por un ordinal relativo a la unidad; nunca por puntero ni por ordinal absoluto. Un
//     puntero de sintaxis fuera del índice hace fallar al escritor, y el productor no publica
//     snapshot.
class PrecompiledSnapshotWriter final
{
public:
    explicit PrecompiledSnapshotWriter(const PrecompiledSyntaxNodeIndex& index) noexcept;

    void byte(std::uint8_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);
    void text(std::string_view value);

    void node(const ast::AstNode* node);
    void type(const ast::TypeSyntax* type);
    void typeKey(std::string_view key);
    void memberSignature(std::string_view declaringTypeKey, std::string_view signatureKey);
    void namespaceSymbol(std::string_view identityKey);
    void specialization(std::string_view specializationKey);
    void unitOrdinal(std::size_t relativeOrdinal);

    bool failed() const noexcept;
    std::string take();

private:
    void kind(std::uint8_t value);

    const PrecompiledSyntaxNodeIndex& index_;
    std::string bytes_;
    bool failed_ = false;
};
}
