#pragma once

#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"

#include "../Cache/CachePayloadReader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// EN: Reads what PrecompiledSnapshotWriter wrote, against the index of the restored API unit. Any
//     truncation, unexpected reference kind or syntax index outside the unit sets failed(); the
//     reader then returns empty values and the caller discards the snapshot (fallback rule 0.4).
// ES: Lee lo que escribió PrecompiledSnapshotWriter, contra el índice de la unidad de API
//     restaurada. Cualquier truncamiento, clase de referencia inesperada o índice de sintaxis fuera
//     de la unidad activa failed(); entonces el lector regresa valores vacíos y el llamador descarta
//     el snapshot (regla de respaldo 0.4).
class PrecompiledSnapshotReader final
{
public:
    PrecompiledSnapshotReader(std::string_view bytes, const PrecompiledSyntaxNodeIndex& index) noexcept;

    std::uint8_t byte();
    std::uint32_t u32();
    std::uint64_t u64();
    std::string text();

    const ast::AstNode* node();
    const ast::TypeSyntax* type();
    std::string typeKey();
    void memberSignature(std::string& declaringTypeKey, std::string& signatureKey);
    std::string namespaceSymbol();
    std::string specialization();
    std::size_t unitOrdinal();

    bool failed() const noexcept;
    bool atEnd() const noexcept;

private:
    // EN: Reads the reference tag; true when it is `expected`, false (without failing) for Null
    //     when `allowNull`, and failure for anything else.
    // ES: Lee la marca de referencia; true si es `expected`, false (sin fallar) para Null cuando
    //     `allowNull`, y falla con cualquier otra.
    bool expectKind(std::uint8_t expected, bool allowNull);

    internal::CachePayloadReader reader_;
    const PrecompiledSyntaxNodeIndex& index_;
    bool failed_ = false;
};
}
