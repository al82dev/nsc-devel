#pragma once

#include "nativesharp/Frontend/ClassValueOrigins.h"
#include "nativesharp/Frontend/ObjectConstructionBinding.h"
#include "nativesharp/Language/EscapeClass.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
struct ClassReceiverEffectEdge final
{
    std::size_t caller = 0;
    std::optional<std::size_t> callee;
    bool receiverIsCallerReceiver = false;
    bool forwardsResult = false;
    bool dispatchClosed = false;
    // Contracts of arguments known to carry the caller's receiver. Missing
    // metadata must be represented as Unknown, not as an empty contract.
    std::vector<language::EscapeClass> receiverArgumentContracts;
};

// A least fixed point over already-validated local summaries. A caller can be
// considered known only if its entire body has separately been covered; this
// routine never upgrades Unknown to a proof of safety.
[[nodiscard]] inline std::vector<ClassReceiverEffects> propagateClassReceiverEffects(
    std::vector<ClassReceiverEffects> effects,
    std::span<const ClassReceiverEffectEdge> edges)
{
    std::vector<std::vector<std::size_t>> dependents(effects.size());
    for (std::size_t i = 0; i < edges.size(); ++i)
        if (edges[i].callee.has_value() && *edges[i].callee < effects.size())
            dependents[*edges[i].callee].push_back(i);
    std::vector<std::size_t> work;
    work.reserve(edges.size());
    std::vector<bool> queued(edges.size(), true);
    for (std::size_t i = 0; i < edges.size(); ++i) work.push_back(i);
    for (std::size_t cursor = 0; cursor < work.size(); ++cursor)
    {
        const std::size_t edgeOrdinal = work[cursor];
        queued[edgeOrdinal] = false;
        const ClassReceiverEffectEdge& edge = edges[edgeOrdinal];
        if (edge.caller >= effects.size()) continue;
        ClassReceiverEffects& caller = effects[edge.caller];
        const ClassReceiverEffects before = caller;
        if (edge.receiverIsCallerReceiver)
        {
            if (!edge.dispatchClosed || !edge.callee.has_value() || *edge.callee >= effects.size())
                caller.unknown = true;
            else
            {
                const ClassReceiverEffects& target = effects[*edge.callee];
                caller.unknown = caller.unknown || target.unknown;
                caller.published = caller.published || target.published;
                caller.returned = caller.returned || (edge.forwardsResult && target.returned);
            }
        }
        for (const language::EscapeClass contract : edge.receiverArgumentContracts)
        {
            switch (contract)
            {
                case language::EscapeClass::NoEscape: break;
                case language::EscapeClass::Return:
                    caller.returned = caller.returned || edge.forwardsResult;
                    break;
                case language::EscapeClass::CallerOwned:
                case language::EscapeClass::Global: caller.published = true; break;
                case language::EscapeClass::Unknown:
                case language::EscapeClass::RefSlotNoEscape:
                case language::EscapeClass::ObjectPointerGlobal: caller.unknown = true; break;
            }
        }
        if (before.returned == caller.returned && before.published == caller.published &&
            before.unknown == caller.unknown) continue;
        for (const std::size_t dependent : dependents[edge.caller])
            if (!queued[dependent])
            {
                queued[dependent] = true;
                work.push_back(dependent);
            }
    }
    return effects;
}

enum class ClassConstructorReceiverAssessment { NoEscape, MayEscape, Unknown };

[[nodiscard]] inline ClassConstructorReceiverAssessment assessClassConstructorReceiver(
    const ObjectConstructionBinding& binding,
    std::span<const ClassReceiverEffects> effects,
    bool initializationProvenSafe)
{
    if (binding.targetIsStruct ||
        (binding.resolution != ConstructionResolutionKind::ResolvedExplicit &&
         binding.resolution != ConstructionResolutionKind::ResolvedImplicitDefault))
        return ClassConstructorReceiverAssessment::Unknown;
    // EN: An implicit default construction whose whole chain (ending at a guarded precompiled root) was
    //     proven by the construction model (doc 17).
    // ES: Una construccion implicita por omision cuya cadena completa (hasta una raiz precompilada protegida)
    //     demostro el modelo de construcciones (doc 17).
    if (binding.selectedImplicitDefault && !binding.selectedConstructorMemberOrdinal.has_value() &&
        initializationProvenSafe)
        return ClassConstructorReceiverAssessment::NoEscape;
    const std::optional<std::size_t> target = binding.selectedConstructorMemberOrdinal.has_value()
        ? binding.selectedConstructorMemberOrdinal : binding.implicitDefaultBaseConstructorMemberOrdinal;
    if (!target.has_value())
    {
        if (!initializationProvenSafe) return ClassConstructorReceiverAssessment::Unknown;
        return binding.selectedImplicitDefault ? ClassConstructorReceiverAssessment::NoEscape
                                               : ClassConstructorReceiverAssessment::Unknown;
    }
    if (*target >= effects.size()) return ClassConstructorReceiverAssessment::Unknown;
    const ClassReceiverEffects& effect = effects[*target];
    if (effect.published || effect.returned) return ClassConstructorReceiverAssessment::MayEscape;
    if (!initializationProvenSafe) return ClassConstructorReceiverAssessment::Unknown;
    return effect.unknown ? ClassConstructorReceiverAssessment::Unknown
                          : ClassConstructorReceiverAssessment::NoEscape;
}
}
