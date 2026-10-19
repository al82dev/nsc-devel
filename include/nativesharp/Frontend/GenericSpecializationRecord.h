#pragma once

#include "nativesharp/Frontend/GenericTemplateKind.h"
#include "nativesharp/Frontend/InstantiationContext.h"
#include "nativesharp/Frontend/SpecializationState.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/SourceRange.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct GenericSpecializationRecord final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    const ast::AstNode* templateSyntax = nullptr;
    const ast::AstNode* specializedSyntax = nullptr;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
    std::size_t cacheOrdinal = 0;
    // NSC-LIMIT-007. Lo que la validacion de constraints necesita y no podia deducir: DE QUE
    // PLANTILLA salio esta especializacion, y DONDE se pidio.
    //
    // El sitio importa mas de lo que parece: sin el, un `Caja<int>` invalido se diagnostica sobre
    // la declaracion de `Caja<T>`, que es correcta, y el lector va al sitio equivocado. El
    // diagnostico tiene que apuntar a quien escribio `Caja<int>`.
    std::size_t templateOrdinal = 0;
    const ast::AstNode* instantiationSyntax = nullptr;
    SourceRange instantiationRange;
    // NSC-LIMIT-007, segunda vuelta. CERO si esta especializacion es la unica respuesta posible a
    // su sitio de instanciacion; distinto de cero si salio de un conjunto de SOBRECARGAS que se
    // materializaron todas juntas porque comparten nombre y aridad y elegir entre ellas necesita
    // los tipos de los argumentos --que esta fase no tiene--.
    //
    // Importa para la validacion: en un grupo asi, que UNA candidata incumpla su constraint no es
    // un error del programa, porque la llamada puede estar resolviendo a otra. Solo lo es si
    // fallan TODAS. Sin este campo, `F<Valor>(...)` con dos `F<T>` declaradas se rechazaba por la
    // candidata equivocada.
    std::size_t overloadGroup = 0;
    // NSC305-G09. The demand-owned instantiation context (request site + packs + projections).
    InstantiationContext instantiationContext;
    // NSC305-G07. Explicit lifecycle state (see SpecializationState). A record is pushed as
    // Building and flips to Ready when the committed clone is attached; precompiled seed and lambda
    // instances are born Ready.
    SpecializationState state = SpecializationState::Unseen;
};
}
