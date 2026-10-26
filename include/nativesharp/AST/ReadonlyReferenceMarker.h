#pragma once

#include "nativesharp/AST/AstNode.h"

#include <string>
#include <string_view>
#include <utility>

namespace nativesharp::ast
{
// EN: Freeze 3.0 P3. `ref readonly T` on a method, property or indexer return: the declaration
//     (and its getter accessor) carries this marker in its auxiliary text next to the existing
//     markers (`partial-method`, `indexer:get:Item`). isReference() still says that the callable
//     returns an address; the marker only says that the address may not be written. Auxiliary
//     text already survives generic cloning, the C1 template cache and the precompiled API unit.
// ES: Congelamiento 3.0 P3. `ref readonly T` en el retorno de un metodo, propiedad o indexador: la
//     declaracion (y su accesor get) lleva este marcador en su texto auxiliar junto a los
//     marcadores existentes (`partial-method`, `indexer:get:Item`). isReference() sigue diciendo
//     que el callable devuelve una direccion; el marcador solo dice que esa direccion no se puede
//     escribir. El texto auxiliar ya sobrevive al clonado generico, a la cache C1 de plantillas y
//     a la unidad de API precompilada.
inline constexpr std::string_view kReadonlyReferenceMarker = "readonly-reference";

inline bool hasReadonlyReferenceMarker(const AstNode& node)
{
    return node.auxiliaryText().find(kReadonlyReferenceMarker) != std::string::npos;
}

inline void addReadonlyReferenceMarker(AstNode& node)
{
    if (hasReadonlyReferenceMarker(node)) return;
    std::string marker = node.auxiliaryText();
    if (!marker.empty()) marker += ';';
    marker += kReadonlyReferenceMarker;
    node.setAuxiliaryText(std::move(marker));
}
}
