#include "OwnershipArgumentClassification.h"
#include "OperatorClassification.h"
#include "IntegralConstantArithmetic.h"
#include "OwnershipFlowSnapshotQueries.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "Bodies/MethodBodyInternals.h"
#include "Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
language::PointerContract pointerContractFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    language::PointerContract contract;
    if (syntax == nullptr) return contract;
    contract.mutablePointee = !syntax->isPointeeConst();
    for (auto modifier = syntax->modifiers().rbegin(); modifier != syntax->modifiers().rend(); ++modifier)
    {
        if (modifier->kind != ast::TypeModifierSyntaxKind::Pointer) continue;
        contract.nullable = modifier->pointerNullability !=
            ast::PointerNullabilitySyntaxKind::NonNull;
        contract.hasProvenance = modifier->pointerHasProvenance;
        contract.knownAlignment = modifier->pointerKnownAlignment;
        contract.addressSpace = modifier->pointerAddressSpace;
        break;
    }
    return contract;
}

bool isGenericValueTypeKey(std::string_view typeKey) noexcept
{
    // An unconstrained generic value may specialize to either Copy or move-only storage. An
    // explicit move is valid for both: it is copy-shaped for a Copy specialization and the
    // required transfer for a move-only specialization. Keep this rule local to explicit-move
    // admission; classifying every generic as globally move-only would break `where T : copy`.
    return typeKey.starts_with("generic:");
}

}

// NSC-LIMIT-010. LA REGLA DE COMPATIBILIDAD DE PROPIEDAD, ESCRITA UNA SOLA VEZ.
//
// Estaba solo dentro de `analyzeCall`, y `ProjectConstructionModel::bindConstruction` elegia
// constructor mirando nada mas tipos y conversiones fisicas. Por eso `new Caja(move(duenio))` no
// encontraba el constructor `owned`: fisicamente todos los candidatos valian, y no habia con que
// desempatarlos. Sacada aqui, los dos resolutores contestan lo mismo por construccion.
//
// El orden de las comprobaciones NO es libre. `parameterOwnable` va PRIMERO: un `owned T`
// monomorfizado sobre un tipo valor lleva el calificador escrito y no tiene nada que poseer, asi
// que cualquier forma de argumento vale. Comprobar el calificador antes que la ownabilidad
// rechazaba `LinkedList<int>.AddFirst`, que pasa un `move` de un `int` a un parametro `owned T` --
// medido en Stage 52 y arreglado entonces; aqui se conserva ese orden intacto.
OwnershipArgumentFit classifyOwnershipArgument(
    bool parameterOwnable,
    sema::OwnershipKind parameterOwnership,
    sema::OwnershipKind argumentOwnership,
    bool argumentTransfer,
    bool argumentIsFreshOwner,
    bool argumentIsNull) noexcept
{
    // LA HISTORIA COMPLETA DE ESE ORDEN, medida en Stage 52 y conservada aqui porque no esta
    // escrita en ningun otro sitio:
    //
    // Stage 52 fix: the three branches below must be mutually exclusive on
    // parameterOwnable itself, not just on parameterOwnership -- the ORIGINAL
    // shape of this chain gated the Owned and Borrowed cases on
    // "parameterOwnership == X && parameterOwnable", but then fell through to a
    // plain "else if" for everything else, which included the (Owned,
    // !parameterOwnable) and (Borrowed, !parameterOwnable) combinations too --
    // i.e. a value-typed "owned T"/"borrowed T" parameter (T monomorphized to
    // int, bool, a struct, ...) whose caller passed an EXPLICIT move(...)
    // argument (ownershipTransfer=true) landed in the catch-all
    // "unqualified parameter forbids ownership-transfer argument" branch and was
    // wrongly rejected as non-viable -- discovered via LinkedList<int>.AddFirst's
    // "created[0].SetValue(move(value))" (a plain value(int)-returning move on an
    // owned int parameter, calling another owned-int method) reporting "no viable
    // overload of 'SetValue'". Gating on !parameterOwnable FIRST, uniformly
    // bypassing every ownership check, fixes this for Owned AND Borrowed alike.
    if (!parameterOwnable)
    {
        // Estructuralmente inerte: no hay propiedad que transferir ni que prestar.
        return OwnershipArgumentFit::Ok;
    }
    if (parameterOwnership == sema::OwnershipKind::Owned)
    {
        // T41: null names no owner and therefore has nothing to copy or transfer. Treating it as
        // a fresh owner would make overload resolution work, but would also manufacture a false
        // ownership-transfer binding downstream. Keep absence explicit instead.
        if (argumentIsNull) return OwnershipArgumentFit::Ok;
        // DOS FORMAS DE SATISFACER UN `owned`, y la segunda faltaba.
        //
        //   1. una TRANSFERENCIA de un owner con nombre: `move(local)`, `move(p.campo)`;
        //   2. un owner RECIEN CREADO que todavia no es de nadie: `new T(...)`.
        //
        // Exigir solo la primera hacia imposible `new Caja(new Carga(7))`, y no por una regla del
        // lenguaje sino por una que no contemplaba el caso: no hay de donde mover, asi que `move`
        // ni siquiera se puede escribir ahi. El parametro `owned` se queda con el valor en las
        // dos formas, que es lo unico que la regla tiene que garantizar.
        const bool transfers =
            argumentOwnership == sema::OwnershipKind::Owned && argumentTransfer;
        if (transfers || argumentIsFreshOwner) return OwnershipArgumentFit::Ok;
        return OwnershipArgumentFit::OwnedRequiresTransfer;
    }
    if (parameterOwnership == sema::OwnershipKind::Borrowed)
    {
        const bool checkedBorrowSource =
            (argumentOwnership == sema::OwnershipKind::Owned ||
             argumentOwnership == sema::OwnershipKind::Borrowed) &&
            !argumentTransfer;
        return checkedBorrowSource
            ? OwnershipArgumentFit::Ok
            : OwnershipArgumentFit::BorrowedRequiresCheckedSource;
    }
    // Parametro sin calificador. Lo que no puede colarse dentro es un CONTRATO: una transferencia
    // o un prestamo se perderian en silencio, que es justo lo que Stage 50 existe para impedir.
    //
    // UN OWNER RECIEN CREADO NO ES UN CONTRATO, y por eso NO se rechaza aqui. Se midio: anadir
    // `argumentIsFreshOwner` a esta condicion --que es lo que el comentario de
    // ExpressionResult::ownershipFreshOwner llego a afirmar que pasaba-- rompe cinco de los
    // ejemplos del libro con `no viable overload of 'Insert'`, sobre la forma mas ordinaria que
    // hay en este lenguaje:
    //
    //     public void Insert(Student value)   // parametro sin calificador
    //     tree.Insert(new Student(50, "Ana", 90));
    //
    // Y rechazarlo seria ademas arbitrario: `Student s = new Student(...); tree.Insert(s);` hace
    // exactamente lo mismo con el mismo objeto y siempre fue legal. Lo que un parametro sin
    // calificador no hace es RASTREAR la vida del valor; eso vale igual para un objeto recien
    // creado que para uno que ya existia, y no es algo que esta funcion pueda arreglar.
    //
    // `ownershipFreshOwner` existe para RELAJAR el requisito de un parametro `owned` --que no
    // podia recibir un `new` porque no hay de donde mover-- y no para endurecer este.
    if (argumentTransfer || argumentOwnership == sema::OwnershipKind::Borrowed)
    {
        return OwnershipArgumentFit::UnqualifiedRejectsOwnership;
    }
    return OwnershipArgumentFit::Ok;
}

std::string ownershipArgumentFitMessage(
    OwnershipArgumentFit fit,
    std::string_view parameterName,
    std::string_view parameterTypeKey)
{
    const std::string parameter =
        "parameter '" + std::string(parameterName) + "' of type '" + std::string(parameterTypeKey) + "'";
    switch (fit)
    {
        case OwnershipArgumentFit::OwnedRequiresTransfer:
            return "owned " + parameter +
                " requires an ownership transfer: pass move(...) or a value that already owns "
                "its storage";
        case OwnershipArgumentFit::BorrowedRequiresCheckedSource:
            return "borrowed " + parameter +
                " requires an owned or borrowed source; a moved value cannot be borrowed";
        case OwnershipArgumentFit::UnqualifiedRejectsOwnership:
            return "unqualified " + parameter +
                " cannot receive an ownership transfer or a borrowed value";
        case OwnershipArgumentFit::Ok:
            break;
    }
    return {};
}

namespace
{
using lexer::TokenKind;

std::string typeCandidateList(
    std::span<const std::size_t> candidates,
    const ProjectNameLookup& lookup)
{
    std::string result;
    for (const std::size_t ordinal : candidates)
    {
        if (ordinal >= lookup.namedTypes().size()) continue;
        if (!result.empty()) result += ", ";
        result += lookup.namedTypes()[ordinal].identity.key();
    }
    return result;
}
}
