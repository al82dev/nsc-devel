#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "../Support/OwnershipArgumentClassification.h"
#include "../Support/OperatorClassification.h"
#include "../Support/IntegralConstantArithmetic.h"
#include "../Support/OwnershipFlowSnapshotQueries.h"


#include "../MethodBodyInternals.h"
#include "../../Internal/Stage37ConstantValue.h"

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
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

bool ProjectMethodBodyModel::acceptMovedMethodLocal(
    ExpressionResult& movedMethod,
    const ast::AstNode* expression,
    const ScopeName& source,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (source.semanticOrdinal >= locals_.size()) return false;
    const MethodLocalSymbol& local = locals_[source.semanticOrdinal];
    if (!local.isRaii || !isReferenceTypeKey(local.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(receiver.Method) requires a RAII class receiver or an owned class parameter");
        return false;
    }
    if (local.isPinned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move pinned receiver '" + local.name + "' into a method value");
        return false;
    }
    if (movedRaiiLocals_.contains(source.semanticOrdinal) ||
        maybeMovedRaiiLocals_.contains(source.semanticOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot bind method value from moved or possibly moved receiver '" + local.name + "'");
        return false;
    }
    if (hasActiveBorrowOfOwnerLocal(source.semanticOrdinal, scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move receiver '" + local.name +
                "' into a method value while a borrowed local is alive");
        return false;
    }
    const std::string place = "L" + std::to_string(source.semanticOrdinal);
    if (placeHasMovedDescendant(place) || placeHasMaybeMovedDescendant(place))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move receiver '" + local.name +
                "' into a method value after one of its fields was moved");
        return false;
    }
    movedMethod.ownershipSourceLocalOrdinal = source.semanticOrdinal;
    movedRaiiLocals_.insert(source.semanticOrdinal);
    maybeMovedRaiiLocals_.erase(source.semanticOrdinal);
    invalidateHistoricalBorrowsOfOwnerLocal(source.semanticOrdinal);
    return true;
}

bool ProjectMethodBodyModel::acceptMovedMethodParameter(
    ExpressionResult& movedMethod,
    const ast::AstNode* expression,
    const ScopeName& source,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
    if (owner == nullptr || source.semanticOrdinal >= owner->parameters.size()) return false;
    const MethodParameterSymbol& parameter = owner->parameters[source.semanticOrdinal];
    if (parameter.ownership != sema::OwnershipKind::Owned ||
        !isReferenceTypeKey(parameter.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(receiver.Method) requires a RAII class receiver or an owned class parameter");
        return false;
    }
    if (movedOwnedParameters_.contains(source.semanticOrdinal) ||
        maybeMovedOwnedParameters_.contains(source.semanticOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot bind method value from moved or possibly moved parameter '" +
                parameter.name + "'");
        return false;
    }
    if (hasActiveBorrowOfOwnedParameter(source.semanticOrdinal, scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move receiver parameter '" + parameter.name +
                "' into a method value while a borrowed local is alive");
        return false;
    }
    movedMethod.ownershipSourceParameterOrdinal = source.semanticOrdinal;
    movedOwnedParameters_.insert(source.semanticOrdinal);
    maybeMovedOwnedParameters_.erase(source.semanticOrdinal);
    invalidateHistoricalBorrowsOfOwnedParameter(source.semanticOrdinal);
    return true;
}

bool ProjectMethodBodyModel::analyzeMovedMethodValue(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool moveAllowed)
{
    // T08-F05. `move(receiver.Method)` is the explicit owning method-value form.  It is
    // recognized before ordinary field/element moves because the MemberExpression is a
    // method group, not storage.  The delegate cell will receive the receiver's dynamic
    // destruction entry; a named source is consumed with the same path-sensitive rules as
    // move(receiver), while a fresh receiver is adopted directly.
    if (!allowMethodGroup || !moveAllowed || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::MemberExpression) return false;
    ExpressionResult movedMethod = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics, true);
    if (!movedMethod.methodGroup || !movedMethod.valid || movedMethod.receiverSyntax == nullptr)
        return false;
    bool accepted = movedMethod.ownershipFreshOwner;
    if (!accepted &&
        movedMethod.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> source = lookupScopeName(
            scopeOrdinal, movedMethod.receiverSyntax->text());
        if (source.has_value() && source->kind == ScopeNameKind::Local)
            accepted = acceptMovedMethodLocal(
                movedMethod, expression, *source, scopeOrdinal, diagnostics);
        else if (source.has_value() && source->kind == ScopeNameKind::Parameter)
            accepted = acceptMovedMethodParameter(
                movedMethod, expression, *source,
                methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    if (!accepted && !diagnostics.hasErrors())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(receiver.Method) requires a fresh receiver, a direct RAII class local, or a "
            "direct owned class parameter");
    }
    if (accepted)
    {
        movedMethod.methodGroupMovesReceiver = true;
        movedMethod.ownership = sema::OwnershipKind::Owned;
        movedMethod.ownershipTransfer = true;
        result = std::move(movedMethod);
    }
    return true;
}

std::optional<ProjectMethodBodyModel::ScopeName> ProjectMethodBodyModel::partialMoveRoot(
    const ast::AstNode* storage,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal) const
{
    // A field/element move mutates one slot without consuming its containing owner. Find
    // the named owner at the root of that storage expression so an outstanding borrowed
    // view cannot observe the mutation behind the borrow checker's back.
    const ast::AstNode* root = storage;
    while (root != nullptr &&
           (root->kind() == ast::AstNodeKind::MemberExpression ||
            root->kind() == ast::AstNodeKind::IndexExpression))
    {
        root = root->first();
    }
    if (root == nullptr || root->kind() != ast::AstNodeKind::IdentifierExpression)
        return std::nullopt;
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
    if (!name.has_value()) return std::nullopt;
    if (name->kind == ScopeNameKind::Local)
    {
        return name->semanticOrdinal < locals_.size() &&
                (locals_[name->semanticOrdinal].isRaii || locals_[name->semanticOrdinal].isMoveOnly)
            ? name : std::nullopt;
    }
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    return method != nullptr && name->semanticOrdinal < method->parameters.size() &&
            (method->parameters[name->semanticOrdinal].ownership == sema::OwnershipKind::Owned ||
             (method->parameters[name->semanticOrdinal].passKind == ParameterPassKind::Value &&
              memberModel_ != nullptr && memberModel_->typeIsMoveOnly(
                  method->parameters[name->semanticOrdinal].typeKey)))
        ? name : std::nullopt;
}

bool ProjectMethodBodyModel::rejectInvalidPartialMove(
    const std::optional<ScopeName>& root,
    const ast::AstNode* placeExpression,
    const ast::AstNode* moveExpression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!root.has_value()) return false;
    const std::optional<std::string> operationPlace =
        storagePlaceKey(placeExpression, scopeOrdinal);
    const std::optional<std::string_view> operationPlaceView = operationPlace.has_value()
        ? std::optional<std::string_view>(*operationPlace) : std::nullopt;
    if (root->kind == ScopeNameKind::Local)
    {
        if (locals_[root->semanticOrdinal].isPinned)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                moveExpression->range(),
                "cannot partially move from pinned owner '" +
                    locals_[root->semanticOrdinal].name +
                    "'; pinned projections are not movable");
            return true;
        }
        if (!hasActiveBorrowOfOwnerLocal(
                root->semanticOrdinal, scopeOrdinal, operationPlaceView)) return false;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            moveExpression->range(),
            "cannot partially move from owner '" + locals_[root->semanticOrdinal].name +
                "' while a borrowed local is alive");
        return true;
    }
    if (!hasActiveBorrowOfOwnedParameter(
            root->semanticOrdinal, scopeOrdinal, operationPlaceView)) return false;
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        moveExpression->range(),
        "cannot partially move from owned parameter '" +
            method->parameters[root->semanticOrdinal].name +
            "' while a borrowed local is alive");
    return true;
}

bool ProjectMethodBodyModel::analyzeMovedArrayElement(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool moveAllowed)
{
    // Stage 52: move(...) also accepts a direct native-array element access, e.g.
    // "move(items[i])". This is what lets a generic collection (Queue<T>.Dequeue(),
    // Stack<T>.Pop(), ...) extract an owned element out of its backing T[] storage and
    // return it -- previously the only supported move sources were a named RAII local
    // or an owned parameter, neither of which a computed array slot is. Static
    // moved-from tracking (movedRaiiLocals_/maybeMovedRaiiLocals_ and friends) is
    // deliberately NOT extended here: a runtime array index cannot be reasoned about at
    // compile time the way a named local/parameter can, so this operation stays
    // available only through the explicit, auditable move(...) syntax rather than
    // silently applying to ordinary array reads -- and HIR/LLVM lowering pairs every
    // extraction with a null-store into the vacated slot to prevent a later `delete`
    // from double-freeing it. It is the caller's responsibility not to also read the
    // same slot again as if it still held a live value.
    //
    // SHR-005 (residuo de "owned destination"). Aqui tambien entra el ARREGLO FIJO
    // (`T[n]` en un campo). Era el unico almacenamiento con elementos que no participaba
    // del chequeo de ownership: `isNativeArray` compara el sufijo `"[]"` y un arreglo fijo
    // termina en `"n]"`, asi que `move(buf[i])` se rechazaba con este mismo mensaje y
    // `buf[i] = alias` se aceptaba SIN comprobar nada. Los dos lados del mismo hueco.
    //
    // Por que es el MISMO caso y no uno parecido: lo unico que la bajada necesita de este
    // ligado es una DIRECCION de ranura, y el HIR ya sabe dar la de un elemento de arreglo
    // fijo (caso RawElement, "C2.1 fixed arrays"); `MoveArrayElement` recibe esa direccion
    // y hace el load-and-empty de siempre, gobernado por `clearSourceStorage`, que sale
    // del tipo del ELEMENTO. Un arreglo fijo de un tipo por valor sigue siendo el no-op
    // que ya era.
    if (!moveAllowed || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IndexExpression) return false;
    const std::optional<ScopeName> partialRoot = partialMoveRoot(
        expression->first(), methodMemberOrdinal, scopeOrdinal);
    if (rejectInvalidPartialMove(
            partialRoot, expression->first(), expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    const ExpressionResult element = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    // El analisis de la base se mantiene DENTRO del cortocircuito, igual que antes: si
    // el elemento ya no es valido, volver a analizar la base solo duplicaria sus
    // diagnosticos.
    if (!validateMovedArrayElementSource(
            element, expression, methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    recordMovedArrayElement(
        result, element, expression, partialRoot, methodMemberOrdinal, scopeOrdinal);
    return true;
}

bool ProjectMethodBodyModel::validateMovedArrayElementSource(
    const ExpressionResult& element,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool acceptableElementSource = element.valid && element.assignable;
    if (acceptableElementSource)
    {
        const std::string elementBaseTypeKey = expression->first()->first() != nullptr
            ? analyzeExpression(
                  expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics)
                  .typeKey
            : "<error>";
        acceptableElementSource = ProjectTypeConversion::isNativeArray(elementBaseTypeKey) ||
            ProjectMemberModel::fixedArrayElementTypeKey(elementBaseTypeKey).has_value() ||
            ProjectTypeConversion::isPointer(elementBaseTypeKey);
    }
    if (acceptableElementSource) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "move(...) array-element source must be a direct native array, fixed array, or raw "
        "pointer element access");
    return false;
}

void ProjectMethodBodyModel::recordMovedArrayElement(
    ExpressionResult& result,
    const ExpressionResult& element,
    const ast::AstNode* expression,
    const std::optional<ScopeName>& partialRoot,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    MethodMoveBinding move;
    move.syntax = expression;
    move.methodMemberOrdinal = methodMemberOrdinal;
    move.scopeOrdinal = scopeOrdinal;
    move.typeKey = element.typeKey;
    move.sourceIsArrayElement = true;
    if (partialRoot.has_value() && partialRoot->kind == ScopeNameKind::Local)
        move.partialSourceOwnerLocalOrdinal = partialRoot->semanticOrdinal;
    else if (partialRoot.has_value())
        move.partialSourceOwnerParameterOrdinal = partialRoot->semanticOrdinal;
    move.clearSourceStorage = isReferenceTypeKey(element.typeKey) ||
        element.typeKey == "builtin:string" ||
        ProjectTypeConversion::isNativeArray(element.typeKey) ||
        (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(element.typeKey));
    moveBindings_.push_back(std::move(move));
    result.typeKey = element.typeKey;
    result.valid = true;
    result.assignable = false;
    result.ownership = sema::OwnershipKind::Owned;
    result.ownershipTransfer = true;
}

bool ProjectMethodBodyModel::validateOwnedFieldMovePlace(
    std::vector<std::string>& fieldPath,
    const ast::AstNode* expression,
    const std::optional<ScopeName>& partialRoot,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::string> place = partialMovePlaceKey(
        expression->first(), scopeOrdinal, &fieldPath);
    if (!place.has_value()) return true;
    std::string rootTypeKey;
    if (partialRoot.has_value() && partialRoot->kind == ScopeNameKind::Local &&
        partialRoot->semanticOrdinal < locals_.size())
        rootTypeKey = locals_[partialRoot->semanticOrdinal].typeKey;
    else if (partialRoot.has_value())
    {
        const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
        if (method != nullptr && partialRoot->semanticOrdinal < method->parameters.size())
            rootTypeKey = method->parameters[partialRoot->semanticOrdinal].typeKey;
    }
    if (memberModel_->typeTransitivelyHasCustomDestructor(rootTypeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot partially move from value of type '" + rootTypeKey +
                "' because its custom destructor requires the complete value");
        return false;
    }
    if (placeOrAncestorIsPartiallyMoved(*place) || placeHasMovedDescendant(*place))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from already moved field path '" + *place + "'");
        return false;
    }
    if (placeOrAncestorIsMaybePartiallyMoved(*place) || placeHasMaybeMovedDescendant(*place))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from possibly moved field path '" + *place +
                "' after a control-flow join");
        return false;
    }
    partiallyMovedPlaces_.insert(*place);
    maybePartiallyMovedPlaces_.erase(*place);
    return true;
}

bool ProjectMethodBodyModel::tryOwnedFieldMoveSource(
    ExpressionResult& result,
    const ExpressionResult& fieldSource,
    const ast::AstNode* expression,
    const std::optional<ScopeName>& partialRoot,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Bloque 1 (audit finding "move-out desde field owned"): move(...) also accepts a
    // direct `owned` FIELD access -- `move(holder.slot)` and, inside the declaring class,
    // `move(slot)`. Without this, an owned field was an ownership SINK: values could be
    // transferred in but never taken back out, so a container could not implement the
    // Take()/Dequeue()-shaped operations its array-backed siblings already have via
    // move(items[i]).
    //
    // Semantics, atomically from ownership's point of view: load the slot, null it, hand
    // the loaded value to the new owner. The null-store is what makes the container's own
    // later discharge (its destruction entry, or an overwrite's release) skip the vacated
    // slot -- the same "the slot's null state IS the guard" rule every owner cleanup here
    // uses. Like the array-element case above, static moved-from tracking is deliberately
    // NOT extended to fields: which object a field belongs to is a runtime fact, so the
    // vacated slot simply holds null and a later read sees an empty slot.
    if (!fieldSource.valid || !fieldSource.assignable ||
        !fieldSource.assignmentMemberOrdinal.has_value() || memberModel_ == nullptr ||
        *fieldSource.assignmentMemberOrdinal >= memberModel_->members().size()) return false;
    const ClassMemberSymbol& sourceField =
        memberModel_->members()[*fieldSource.assignmentMemberOrdinal];
    const bool compilerVariantPayloadExtract =
        expression->auxiliaryText() == "variant-payload-extract";
    const bool movableStorageField = sourceField.kind == ClassMemberKind::Field &&
        !sourceField.isStatic &&
        (compilerVariantPayloadExtract ||
         sourceField.declaredOwnership == sema::OwnershipKind::Owned ||
         memberModel_->typeIsMoveOnly(sourceField.declaredTypeKey) ||
         isGenericValueTypeKey(sourceField.declaredTypeKey) ||
         callableBelongsToGenericTypeSpecialization(methodMemberOrdinal));
    if (!movableStorageField) return false;
    std::vector<std::string> fieldPath;
    if (!validateOwnedFieldMovePlace(
            fieldPath, expression, partialRoot,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    MethodMoveBinding move;
    move.syntax = expression;
    move.methodMemberOrdinal = methodMemberOrdinal;
    move.scopeOrdinal = scopeOrdinal;
    move.typeKey = fieldSource.typeKey;
    move.sourceIsOwnedField = true;
    move.partialFieldPath = std::move(fieldPath);
    if (partialRoot.has_value() && partialRoot->kind == ScopeNameKind::Local)
        move.partialSourceOwnerLocalOrdinal = partialRoot->semanticOrdinal;
    else if (partialRoot.has_value())
        move.partialSourceOwnerParameterOrdinal = partialRoot->semanticOrdinal;
    move.clearSourceStorage = isReferenceTypeKey(fieldSource.typeKey) ||
        fieldSource.typeKey == "builtin:string" ||
        ProjectTypeConversion::isNativeArray(fieldSource.typeKey) ||
        memberModel_->typeRequiresValueDrop(fieldSource.typeKey);
    moveBindings_.push_back(std::move(move));
    result.typeKey = fieldSource.typeKey;
    result.valid = true;
    result.assignable = false;
    result.ownership = sema::OwnershipKind::Owned;
    result.ownershipTransfer = true;
    return true;
}

bool ProjectMethodBodyModel::analyzeMovedIteratorCurrent(
    ExpressionResult& result,
    const ExpressionResult& member,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T07-F04: `move(it.Current)` transfers a move-only item out of the
    // iterator's item slot exactly once.
    if (expression->first()->text() != "Current" || !member.valid || memberModel_ == nullptr ||
        !memberModel_->typeIsMoveOnly(member.typeKey)) return false;
    const ast::AstNode* iteratorSyntax = expression->first()->first();
    bool iteratorPrecise = true;
    const std::optional<std::string> iteratorPlace = storagePlaceKey(
        iteratorSyntax, scopeOrdinal, &iteratorPrecise);
    if (iteratorPlace.has_value() && movedIteratorItems_.contains(*iteratorPlace))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from the iterator item twice; the previous move(it.Current) already "
            "transferred it");
        return true;
    }
    if (iteratorPlace.has_value()) movedIteratorItems_.insert(*iteratorPlace);
    MethodMoveBinding move;
    move.syntax = expression;
    move.methodMemberOrdinal = methodMemberOrdinal;
    move.scopeOrdinal = scopeOrdinal;
    move.typeKey = member.typeKey;
    move.sourceIsIteratorCurrent = true;
    move.clearSourceStorage = true;
    moveBindings_.push_back(std::move(move));
    result.typeKey = member.typeKey;
    result.ownership = sema::OwnershipKind::Owned;
    result.ownershipTransfer = true;
    return true;
}

bool ProjectMethodBodyModel::analyzeMovedMember(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool moveAllowed)
{
    if (!moveAllowed || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::MemberExpression) return false;
    const std::optional<ScopeName> partialRoot = partialMoveRoot(
        expression->first(), methodMemberOrdinal, scopeOrdinal);
    if (rejectInvalidPartialMove(
            partialRoot, expression->first(), expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    ++partialMoveSourceDepth_;
    const ExpressionResult member = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    --partialMoveSourceDepth_;
    if (tryOwnedFieldMoveSource(
            result, member, expression, partialRoot,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    if (analyzeMovedIteratorCurrent(
            result, member, expression,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "move(...) member source must be an assignable owned field or an assignable "
        "move-only field");
    return true;
}

bool ProjectMethodBodyModel::validateLocalMoveState(
    const MethodLocalSymbol& local,
    const ast::AstNode* expression,
    std::size_t sourceOrdinal,
    std::size_t scopeOrdinal,
    bool sourceRequiresMove,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!sourceRequiresMove) return true;
    if (local.isPinned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move pinned owner '" + local.name +
                "'; it must remain at a stable address until lexical destruction");
        return false;
    }
    if (movedRaiiLocals_.contains(sourceOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from already moved-from " +
                std::string(local.isRaii ? "RAII local '" : "move-only local '") +
                local.name + "'");
        return false;
    }
    if (maybeMovedRaiiLocals_.contains(sourceOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from possibly moved " +
                std::string(local.isRaii ? "RAII local '" : "move-only local '") +
                local.name + "' after a control-flow join");
        return false;
    }
    if (!hasActiveBorrowOfOwnerLocal(sourceOrdinal, scopeOrdinal)) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "cannot move owner '" + local.name + "' while a borrowed local is alive");
    return false;
}

bool ProjectMethodBodyModel::completeLocalMove(
    ExpressionResult& result,
    const MethodLocalSymbol& local,
    const ast::AstNode* expression,
    std::size_t sourceOrdinal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool sourceRequiresMove,
    diagnostics::DiagnosticEngine& diagnostics)
{
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::Local, expression->first(), methodMemberOrdinal, scopeOrdinal,
        local.name, local.typeKey, sourceOrdinal, std::nullopt, {}, nullptr, "",
        std::nullopt, false});
    MethodMoveBinding move;
    move.syntax = expression;
    move.methodMemberOrdinal = methodMemberOrdinal;
    move.scopeOrdinal = scopeOrdinal;
    move.sourceLocalOrdinal = sourceOrdinal;
    move.typeKey = local.typeKey;
    const std::string place = "L" + std::to_string(sourceOrdinal);
    if (sourceRequiresMove &&
        (placeHasMovedDescendant(place) || placeHasMaybeMovedDescendant(place)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move complete value '" + local.name +
                "' after one of its fields was moved; reinitialize the field first");
        return false;
    }
    move.clearSourceStorage = sourceRequiresMove &&
        (isReferenceTypeKey(local.typeKey) || local.typeKey == "builtin:string" ||
         ProjectTypeConversion::isNativeArray(local.typeKey) ||
         memberModel_->typeRequiresValueDrop(local.typeKey));
    moveBindings_.push_back(std::move(move));
    if (sourceRequiresMove)
    {
        maybeMovedRaiiLocals_.erase(sourceOrdinal);
        movedRaiiLocals_.insert(sourceOrdinal);
        invalidateHistoricalBorrowsOfOwnerLocal(sourceOrdinal);
    }
    result.typeKey = local.typeKey;
    result.callableTrait = local.callableTrait;
    result.ownershipSourceLocalOrdinal = sourceOrdinal;
    result.objectLifetimeOwnership = local.objectLifetimeOwnership;
    result.storageOwnership = local.storageOwnership;
    return true;
}

bool ProjectMethodBodyModel::analyzeNamedLocalMove(
    ExpressionResult& result,
    bool& copyShapedMove,
    const ast::AstNode* expression,
    const ScopeName& source,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool compilerVariantResultTransfer,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (source.semanticOrdinal >= locals_.size()) return false;
    const MethodLocalSymbol& local = locals_[source.semanticOrdinal];
    const bool sourceRequiresMove = local.isRaii || local.isMoveOnly ||
        compilerVariantResultTransfer;
    // `move` may spell an explicit copy for value-shaped inputs, but it must never
    // manufacture ownership for a reference which was not owned by its source.
    if (!sourceRequiresMove && typeKeyIsOwnable(local.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(...) source must be a RAII local or owned parameter");
        return false;
    }
    copyShapedMove = !sourceRequiresMove;
    if (!validateLocalMoveState(
            local, expression, source.semanticOrdinal,
            scopeOrdinal, sourceRequiresMove, diagnostics)) return false;
    return completeLocalMove(
        result, local, expression, source.semanticOrdinal,
        methodMemberOrdinal, scopeOrdinal, sourceRequiresMove, diagnostics);
}

bool ProjectMethodBodyModel::validateParameterMoveState(
    const MethodParameterSymbol& parameter,
    const ast::AstNode* expression,
    std::size_t sourceOrdinal,
    std::size_t scopeOrdinal,
    bool sourceRequiresMove,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!sourceRequiresMove) return true;
    if (movedOwnedParameters_.contains(sourceOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from already moved-from parameter '" + parameter.name + "'");
        return false;
    }
    if (maybeMovedOwnedParameters_.contains(sourceOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move from possibly moved parameter '" + parameter.name +
                "' after a control-flow join");
        return false;
    }
    if (!hasActiveBorrowOfOwnedParameter(sourceOrdinal, scopeOrdinal)) return true;
    const std::string parameterKind = parameter.ownership == sema::OwnershipKind::Owned
        ? "owned parameter '" : "parameter '";
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "cannot move " + parameterKind + parameter.name +
            "' while a borrowed local is alive");
    return false;
}

bool ProjectMethodBodyModel::completeParameterMove(
    ExpressionResult& result,
    const MethodParameterSymbol& parameter,
    const ast::AstNode* expression,
    std::size_t sourceOrdinal,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool sourceRequiresMove,
    diagnostics::DiagnosticEngine& diagnostics)
{
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::Parameter, expression->first(), methodMemberOrdinal, scopeOrdinal,
        parameter.name, parameter.typeKey, std::nullopt, sourceOrdinal, {}, nullptr, "",
        std::nullopt, false});
    MethodMoveBinding move;
    move.syntax = expression;
    move.methodMemberOrdinal = methodMemberOrdinal;
    move.scopeOrdinal = scopeOrdinal;
    move.sourceParameterOrdinal = sourceOrdinal;
    move.typeKey = parameter.typeKey;
    const std::string place = "P" + std::to_string(sourceOrdinal);
    if (sourceRequiresMove &&
        (placeHasMovedDescendant(place) || placeHasMaybeMovedDescendant(place)))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot move complete parameter '" + parameter.name +
                "' after one of its fields was moved; reinitialize the field first");
        return false;
    }
    move.clearSourceStorage = sourceRequiresMove &&
        (isReferenceTypeKey(parameter.typeKey) || parameter.typeKey == "builtin:string" ||
         ProjectTypeConversion::isNativeArray(parameter.typeKey) ||
         memberModel_->typeRequiresValueDrop(parameter.typeKey));
    moveBindings_.push_back(std::move(move));
    if (sourceRequiresMove)
    {
        maybeMovedOwnedParameters_.erase(sourceOrdinal);
        movedOwnedParameters_.insert(sourceOrdinal);
        invalidateHistoricalBorrowsOfOwnedParameter(sourceOrdinal);
    }
    result.typeKey = parameter.typeKey;
    result.ownershipSourceParameterOrdinal = sourceOrdinal;
    result.objectLifetimeOwnership = sema::OwnershipKind::Owned;
    result.storageOwnership = sema::OwnershipKind::Owned;
    return true;
}

bool ProjectMethodBodyModel::analyzeNamedParameterMove(
    ExpressionResult& result,
    bool& copyShapedMove,
    const ast::AstNode* expression,
    const ScopeName& source,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool compilerVariantResultTransfer,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || source.semanticOrdinal >= method->parameters.size()) return false;
    const MethodParameterSymbol& parameter = method->parameters[source.semanticOrdinal];
    const bool sourceRequiresMove = parameter.ownership == sema::OwnershipKind::Owned ||
        (parameter.passKind == ParameterPassKind::Value &&
         (memberModel_->typeIsMoveOnly(parameter.typeKey) ||
          isGenericValueTypeKey(parameter.typeKey))) ||
        compilerVariantResultTransfer;
    if (!sourceRequiresMove && typeKeyIsOwnable(parameter.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(...) source must be a RAII local or owned parameter");
        return false;
    }
    const bool genericCopyShapedMove = parameter.passKind == ParameterPassKind::Value &&
        callableBelongsToGenericTypeSpecialization(methodMemberOrdinal) && !sourceRequiresMove;
    copyShapedMove = !sourceRequiresMove || genericCopyShapedMove;
    if (!validateParameterMoveState(
            parameter, expression, source.semanticOrdinal,
            scopeOrdinal, sourceRequiresMove, diagnostics)) return false;
    return completeParameterMove(
        result, parameter, expression, source.semanticOrdinal,
        methodMemberOrdinal, scopeOrdinal, sourceRequiresMove, diagnostics);
}

bool ProjectMethodBodyModel::moveExpressionAllowed(
    const ast::AstNode* expression,
    bool allowMove) const
{
    return allowMove || expression->auxiliaryText() == "variant-payload-transfer" ||
        expression->auxiliaryText() == "variant-payload-extract" ||
        expression->auxiliaryText() == "variant-result-transfer" ||
        expression->auxiliaryText() == "comprehension-result";
}

void ProjectMethodBodyModel::completeMoveResult(
    ExpressionResult& result,
    bool copyShapedMove) const
{
    result.valid = true;
    result.assignable = false;
    result.ownership = copyShapedMove
        ? sema::OwnershipKind::None : sema::OwnershipKind::Owned;
    result.ownershipTransfer = !copyShapedMove;
    if (copyShapedMove)
    {
        result.ownershipSourceLocalOrdinal.reset();
        result.ownershipSourceParameterOrdinal.reset();
        result.objectLifetimeOwnership = sema::OwnershipKind::None;
        result.storageOwnership = sema::OwnershipKind::None;
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindMoveExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    if (expression->kind() != ast::AstNodeKind::MoveExpression) return result;
    const bool compilerVariantResultTransfer =
        expression->auxiliaryText() == "variant-result-transfer";
    const bool moveAllowed = moveExpressionAllowed(expression, allowMove);
    if (analyzeMovedMethodValue(
            result, expression, methodMemberOrdinal, scopeOrdinal,
            diagnostics, allowMethodGroup, moveAllowed)) return result;
    if (analyzeMovedArrayElement(
            result, expression, methodMemberOrdinal,
            scopeOrdinal, diagnostics, moveAllowed)) return result;
    if (analyzeMovedMember(
            result, expression, methodMemberOrdinal,
            scopeOrdinal, diagnostics, moveAllowed)) return result;
    if (!moveAllowed || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(...) is only valid for a direct RAII local or owned parameter, or a "
            "move-only local/parameter, in a consuming context");
        return result;
    }
    const std::optional<ScopeName> source = lookupScopeName(
        scopeOrdinal, expression->first()->text());
    if (!source.has_value())
    {
        // An identifier that is not a local or parameter can still be a FIELD of the
        // current type reached through the implicit `this` -- `move(slot)` inside the
        // declaring class is the container's own Take() idiom.
        ++partialMoveSourceDepth_;
        const ExpressionResult implicitField = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        --partialMoveSourceDepth_;
        if (tryOwnedFieldMoveSource(
                result, implicitField, expression, std::nullopt,
                methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "move(...) source must be a RAII local or owned parameter, or a move-only "
            "local/parameter");
        return result;
    }
    bool copyShapedMove = false;
    const bool accepted = source->kind == ScopeNameKind::Local
        ? analyzeNamedLocalMove(
              result, copyShapedMove, expression, *source,
              methodMemberOrdinal, scopeOrdinal, compilerVariantResultTransfer, diagnostics)
        : analyzeNamedParameterMove(
              result, copyShapedMove, expression, *source,
              methodMemberOrdinal, scopeOrdinal, compilerVariantResultTransfer, diagnostics);
    if (accepted) completeMoveResult(result, copyShapedMove);
    return result;
}
}
