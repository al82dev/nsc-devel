#include "nativesharp/Frontend/ArrayReferenceLatePathProof.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>

namespace nativesharp::frontend
{
namespace
{
bool emptySourceBody(const ClassMemberSymbol& member)
{
    return !member.isPrecompiled && !member.isUnsafe && !member.isVirtual && !member.isOverride &&
        !member.isAbstract && member.declaredErrorTypeKey.empty() &&
        !member.effects.contains(sema::Effect::Async) && !member.effects.contains(sema::Effect::Generator) &&
        member.syntax != nullptr && member.syntax->first() != nullptr &&
        member.syntax->first()->kind() == ast::AstNodeKind::BlockStatement &&
        member.syntax->first()->children().empty();
}

bool trivialStaticInitialization(const ProjectMemberModel& members, const ProjectNameLookup& names,
                                 std::size_t type)
{
    if (type >= names.namedTypes().size() || names.namedTypes()[type].isPrecompiled) return false;
    for (const auto& part : names.namedTypes()[type].parts) if (part.isPrecompiled) return false;
    for (const std::size_t ordinal : members.membersOfType(type))
    {
        if (ordinal >= members.members().size()) return false;
        const ClassMemberSymbol& member = members.members()[ordinal];
        if (member.declaringTypeOrdinal != type || !member.isStatic) continue;
        if (member.kind == ClassMemberKind::Constructor) return false;
        if (member.kind == ClassMemberKind::Field && !member.isConst &&
            (member.isPrecompiled || member.syntax == nullptr || member.syntax->first() != nullptr)) return false;
    }
    return true;
}

const ast::AstNode* callArgument(const MethodCallBinding& call, std::size_t parameter)
{
    const ast::AstNode* result = nullptr;
    for (std::size_t i = 0; i < call.selectedArgumentParameterOrdinals.size() &&
         i < call.selectedArgumentSyntaxes.size(); ++i)
        if (call.selectedArgumentParameterOrdinals[i] == parameter)
        {
            if (result != nullptr) return nullptr;
            result = call.selectedArgumentSyntaxes[i];
        }
    return result;
}
}

ArrayReferenceLatePathResult evaluateArrayReferenceLatePath(
    const ProjectCompilation& project, const ArrayReferenceReplayObservation& observation)
{
    constexpr auto unknown = ArrayReferenceLatePathResult::Unknown;
    const auto* model = project.methodBodyModel(); const auto* members = project.memberModel();
    const auto* names = project.nameLookup(); const auto* inheritance = project.inheritanceModel();
    const auto* dispatch = project.virtualDispatchModel(); const auto* construction = project.constructionModel();
    const auto* destruction = project.destructionModel();
    const ArrayReferenceRead candidateState = observation.potentialIndex && observation.potentialCandidateState
        ? *observation.potentialCandidateState : observation.state;
    if (model == nullptr || members == nullptr || names == nullptr || inheritance == nullptr ||
        dispatch == nullptr || construction == nullptr || destruction == nullptr ||
        !model->isBuilt() || !construction->isBuilt() || !destruction->isBuilt() ||
        !((candidateState == ArrayReferenceRead::MaybeDead && observation.deadPathWitness) ||
          (candidateState == ArrayReferenceRead::Dead && observation.requiresLateValidation)) ||
        !observation.instanceMethodReceiver || !observation.deadPathEvidence.has_value() ||
        observation.syntax == nullptr || observation.syntax->kind() != ast::AstNodeKind::IndexExpression ||
        !model->arrayReferenceLinearPublicationCoverage(observation.callableOrdinal)) return unknown;
    const auto* caller = model->callableSymbol(observation.callableOrdinal);
    if (caller == nullptr || caller->isUnsafe || !caller->declaredErrorTypeKey.empty() ||
        caller->effects.contains(sema::Effect::Async) || caller->effects.contains(sema::Effect::Generator) ||
        !trivialStaticInitialization(*members, *names, caller->declaringTypeOrdinal))
        return unknown;
    const MethodCallBinding* receiverCall = nullptr;
    for (const auto& call : model->calls())
        if (call.callerMethodMemberOrdinal == observation.callableOrdinal &&
            call.receiverSyntax == observation.syntax && !call.staticCall &&
            call.resolution == MethodCallResolutionKind::Resolved)
        { if (receiverCall != nullptr) return unknown; receiverCall = &call; }
    if (receiverCall == nullptr || receiverCall->interfaceCall || !receiverCall->selectedMemberOrdinal ||
        *receiverCall->selectedMemberOrdinal >= members->members().size() ||
        !receiverCall->selectedArgumentSyntaxes.empty() || !receiverCall->selectedDefaultArguments.empty() ||
        receiverCall->selectedParameterPackOrdinal) return unknown;
    const auto& receiverTarget = members->members()[*receiverCall->selectedMemberOrdinal];
    if (receiverTarget.isVirtual || receiverTarget.isOverride || receiverTarget.isAbstract ||
        !receiverTarget.parameters.empty()) return unknown;
    const auto localOf = [&](const ast::AstNode* syntax) -> std::optional<std::size_t>
    {
        if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::IdentifierExpression) return {};
        const MethodNameBinding* binding = model->nameBinding(syntax, observation.callableOrdinal);
        return binding != nullptr && binding->kind == MethodNameBindingKind::Local
            ? binding->localOrdinal : std::nullopt;
    };
    const auto* index = observation.syntax->children().size() == 1 ? observation.syntax->children()[0]
        : observation.syntax->children().empty() ? observation.syntax->second() : nullptr;
    const auto observedArray = localOf(observation.syntax->first());
    if (!observedArray || index == nullptr || index->kind() != ast::AstNodeKind::IntegerLiteralExpression ||
        index->integerValue() > static_cast<std::uint64_t>(INT64_MAX)) return unknown;
    const auto observedIndex = static_cast<std::int64_t>(index->integerValue());

    struct Object { std::size_t type; bool dead = false; };
    std::map<std::size_t, Object> objects;
    std::map<std::size_t, std::uint64_t> arrays;
    std::map<std::pair<std::size_t, std::int64_t>, std::size_t> slots;
    std::map<std::size_t, bool> branchValues;
    std::map<const ast::AstNode*, std::optional<std::size_t>> initializedLocals;
    const auto locals = model->locals();
    const ProjectConversionClassifier conversions(*inheritance, project.interfaceModel(), members);
    const auto singleIdentifierUse = [&](const ast::AstNode* syntax, MethodNameBindingKind kind,
                                         std::size_t ordinal)
    {
        if (caller->syntax == nullptr || caller->syntax->first() == nullptr) return false;
        std::size_t matchingUses = 0;
        std::vector<const ast::AstNode*> pending{caller->syntax->first()};
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (node == nullptr) continue;
            if (node->kind() == ast::AstNodeKind::LambdaExpression ||
                node->kind() == ast::AstNodeKind::InlineAssemblyStatement || node->auxiliaryText() == "unsafe-block")
                return false;
            if (node->kind() == ast::AstNodeKind::IdentifierExpression && node->text() == syntax->text())
            {
                const auto* binding = model->nameBinding(node, observation.callableOrdinal);
                if (binding == nullptr) return false;
                const auto bound = kind == MethodNameBindingKind::Local ? binding->localOrdinal : binding->parameterOrdinal;
                if (binding->kind == kind && bound && *bound == ordinal)
                {
                    if (node != syntax) return false;
                    ++matchingUses;
                }
            }
            pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
            for (const auto* child : node->children()) pending.push_back(child);
        }
        return matchingUses == 1;
    };
    const auto provenIndexLiteral = [&](const ast::AstNode* syntax, std::int64_t expected,
                                        std::string_view parameterType)
    {
        if (syntax == nullptr) return false;
        const ast::AstNode* literal = syntax;
        if (syntax->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto* source = model->nameBinding(syntax, observation.callableOrdinal);
            if (source != nullptr && source->kind == MethodNameBindingKind::Parameter && source->parameterOrdinal)
            {
                const auto ordinal = *source->parameterOrdinal;
                return observation.potentialIndex && expected == 0 && ordinal < caller->parameters.size() &&
                    caller->parameters[ordinal].passKind == ParameterPassKind::Value &&
                    caller->parameters[ordinal].ownership == sema::OwnershipKind::None &&
                    caller->parameters[ordinal].typeKey == parameterType &&
                    ProjectTypeConversion::isIntegral(parameterType) &&
                    singleIdentifierUse(syntax, MethodNameBindingKind::Parameter, ordinal);
            }
            const auto local = localOf(syntax);
            if (!local || *local >= locals.size() || locals[*local].isStatic || locals[*local].isReference ||
                locals[*local].typeKey != parameterType || locals[*local].syntax == nullptr ||
                caller->syntax == nullptr || caller->syntax->first() == nullptr) return false;
            literal = locals[*local].syntax->first();
            if (literal == nullptr) return false;
            const auto* initialType = model->expressionTypeBinding(literal, observation.callableOrdinal);
            if (initialType == nullptr || conversions.classifyImplicit(initialType->typeKey, locals[*local].typeKey,
                    initialType->integerConstant).kind != ProjectConversionKind::Identity) return false;
            if (!singleIdentifierUse(syntax, MethodNameBindingKind::Local, *local)) return false;
        }
        return literal->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
            literal->integerValue() <= 0x7fffffffffffffffULL &&
            static_cast<std::int64_t>(literal->integerValue()) == expected;
    };
    for (std::size_t i = 0; i < locals.size(); ++i)
    {
        if (locals[i].methodMemberOrdinal != observation.callableOrdinal || locals[i].syntax == nullptr ||
            locals[i].syntax->first() == nullptr) continue;
        if (ProjectTypeConversion::isIntegral(locals[i].typeKey) || locals[i].typeKey == "builtin:bool")
        {
            const auto* expression = model->expressionTypeBinding(locals[i].syntax->first(), observation.callableOrdinal);
            if (expression == nullptr || conversions.classifyImplicit(expression->typeKey, locals[i].typeKey,
                    expression->integerConstant).kind != ProjectConversionKind::Identity) return unknown;
        }
        const auto [it, inserted] = initializedLocals.emplace(locals[i].syntax->first(), i);
        if (!inserted) it->second.reset();
    }
    std::map<const ast::AstNode*, const ObjectConstructionBinding*> constructions;
    for (const auto& binding : construction->constructions())
        if (binding.callerMemberOrdinal == observation.callableOrdinal && binding.syntax != nullptr)
        {
            const auto [it, inserted] = constructions.emplace(binding.syntax, &binding);
            if (!inserted) it->second = nullptr;
        }
    const auto sourceTypeSafe = [&](std::size_t type)
    {
        if (type >= names->namedTypes().size() || names->namedTypes()[type].isPrecompiled ||
            names->namedTypes()[type].isAbstract || inheritance->baseTypeOrdinal(type).has_value() ||
            !trivialStaticInitialization(*members, *names, type)) return false;
        for (const auto& part : names->namedTypes()[type].parts) if (part.isPrecompiled) return false;
        for (const std::size_t ordinal : members->membersOfType(type))
        {
            if (ordinal >= members->members().size()) return false;
            const ClassMemberSymbol& member = members->members()[ordinal];
            if (member.declaringTypeOrdinal != type || member.isStatic) continue;
            if (member.kind == ClassMemberKind::Event) return false;
            if (member.kind == ClassMemberKind::Field &&
                ((!ProjectTypeConversion::isIntegral(member.declaredTypeKey) &&
                  member.declaredTypeKey != "builtin:bool") || member.declaredTypeSyntax == nullptr ||
                 member.declaredTypeSyntax->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None ||
                 member.declaredOwnership != sema::OwnershipKind::None || member.isReferenceReturn || member.syntax == nullptr ||
                 member.syntax->first() != nullptr)) return false;
        }
        return true;
    };
    const auto constructorBodySafe = [&](std::size_t ordinal, std::size_t type)
    {
        if (ordinal >= members->members().size()) return false;
        const auto& ctor = members->members()[ordinal];
        if (ctor.isPrecompiled || ctor.isUnsafe || ctor.isStatic || !ctor.declaredErrorTypeKey.empty() ||
            ctor.effects.contains(sema::Effect::Async) || ctor.effects.contains(sema::Effect::Generator) ||
            ctor.syntax == nullptr || ctor.syntax->first() == nullptr ||
            ctor.syntax->first()->kind() != ast::AstNodeKind::BlockStatement) return false;
        for (const auto& parameter : ctor.parameters)
            if (parameter.passKind != ParameterPassKind::Value || parameter.ownership != sema::OwnershipKind::None ||
                (!ProjectTypeConversion::isIntegral(parameter.typeKey) && parameter.typeKey != "builtin:bool"))
                return false;
        for (const auto* statement : ctor.syntax->first()->children())
        {
            if (statement == nullptr || statement->kind() != ast::AstNodeKind::ExpressionStatement ||
                statement->first() == nullptr || statement->first()->kind() != ast::AstNodeKind::AssignmentExpression)
                return false;
            const auto* assignment = statement->first();
            if (assignment->tokenKind() != lexer::TokenKind::Equal || assignment->first() == nullptr ||
                assignment->second() == nullptr || assignment->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
                assignment->second()->kind() != ast::AstNodeKind::IdentifierExpression) return false;
            const auto* left = model->nameBinding(assignment->first(), ordinal);
            const auto* right = model->nameBinding(assignment->second(), ordinal);
            if (left == nullptr || left->kind != MethodNameBindingKind::ImplicitField ||
                left->candidateMemberOrdinals.size() != 1 || right == nullptr ||
                right->kind != MethodNameBindingKind::Parameter || !right->parameterOrdinal ||
                *right->parameterOrdinal >= ctor.parameters.size()) return false;
            const std::size_t fieldOrdinal = left->candidateMemberOrdinals.front();
            if (fieldOrdinal >= members->members().size()) return false;
            const auto& field = members->members()[fieldOrdinal];
            if (field.kind != ClassMemberKind::Field || field.isStatic || field.declaringTypeOrdinal != type ||
                field.declaredTypeKey != ctor.parameters[*right->parameterOrdinal].typeKey ||
                right->typeKey != field.declaredTypeKey) return false;
        }
        return true;
    };
    const auto destructorReturns = [&](std::size_t type)
    {
        if (!sourceTypeSafe(type)) return false;
        const ClassDestructionPlan* plan = destruction->planForType(type);
        if (plan == nullptr || (plan->explicitDestructorMemberOrdinal.has_value() &&
            plan->destructorMemberOrdinals.empty())) return false;
        for (const std::size_t ordinal : plan->destructorMemberOrdinals)
        {
            if (ordinal >= members->members().size()) return false;
            const auto& destructor = members->members()[ordinal];
            if (emptySourceBody(destructor)) continue;
            if (destructor.isPrecompiled || destructor.isUnsafe || destructor.isStatic || destructor.isVirtual ||
                destructor.isOverride || destructor.isAbstract || !destructor.declaredErrorTypeKey.empty() ||
                destructor.effects.contains(sema::Effect::Async) || destructor.effects.contains(sema::Effect::Generator) ||
                !destructor.parameters.empty() || destructor.syntax == nullptr || destructor.syntax->first() == nullptr ||
                destructor.syntax->first()->kind() != ast::AstNodeKind::BlockStatement) return false;
            for (const auto* statement : destructor.syntax->first()->children())
            {
                if (statement == nullptr || statement->kind() != ast::AstNodeKind::ExpressionStatement ||
                    statement->first() == nullptr || statement->first()->kind() != ast::AstNodeKind::AssignmentExpression)
                    return false;
                const auto* assignment = statement->first();
                if (assignment->tokenKind() != lexer::TokenKind::Equal || assignment->first() == nullptr ||
                    assignment->second() == nullptr || assignment->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
                    (assignment->second()->kind() != ast::AstNodeKind::IntegerLiteralExpression &&
                     assignment->second()->kind() != ast::AstNodeKind::BooleanLiteralExpression)) return false;
                const auto* left = model->nameBinding(assignment->first(), ordinal);
                const auto* right = model->expressionTypeBinding(assignment->second(), ordinal);
                if (left == nullptr || left->kind != MethodNameBindingKind::ImplicitField ||
                    left->candidateMemberOrdinals.size() != 1 || right == nullptr ||
                    left->candidateMemberOrdinals.front() >= members->members().size()) return false;
                const auto& field = members->members()[left->candidateMemberOrdinals.front()];
                if (field.kind != ClassMemberKind::Field || field.isStatic || field.declaringTypeOrdinal != type ||
                    field.isReferenceReturn || field.isVolatile || field.isBitField || field.isAnonymousMemberPromotion ||
                    field.explicitFieldOffset || conversions.classifyImplicit(right->typeKey, field.declaredTypeKey,
                        right->integerConstant).kind != ProjectConversionKind::Identity) return false;
            }
        }
        return true;
    };
    const auto& events = observation.deadPathEvidence->events;
    if (events.empty() || events.size() > 256) return unknown;
    bool sawBranch = false;
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const ArrayReferenceTraceEvent& event = events[i];
        if (event.syntax == nullptr) return unknown;
        if (event.kind == ArrayReferenceTraceKind::Branch)
        {
            if (!event.branch || event.syntax->kind() != ast::AstNodeKind::IfStatement ||
                event.syntax->first() == nullptr ||
                event.syntax->first()->kind() != ast::AstNodeKind::IdentifierExpression) return unknown;
            const MethodNameBinding* binding = model->nameBinding(event.syntax->first(), observation.callableOrdinal);
            if (binding == nullptr || binding->kind != MethodNameBindingKind::Parameter ||
                !binding->parameterOrdinal || *binding->parameterOrdinal >= caller->parameters.size() ||
                binding->typeKey != "builtin:bool" ||
                caller->parameters[*binding->parameterOrdinal].passKind != ParameterPassKind::Value) return unknown;
            const auto [it, inserted] = branchValues.emplace(*binding->parameterOrdinal, *event.branch);
            if (!inserted && it->second != *event.branch) return unknown;
            sawBranch = true;
            continue;
        }
        if (event.kind == ArrayReferenceTraceKind::Construction)
        {
            const auto localEntry = initializedLocals.find(event.syntax);
            const auto local = localEntry == initializedLocals.end() ? std::optional<std::size_t>{} : localEntry->second;
            if (!local || *local >= model->locals().size()) return unknown;
            const MethodLocalSymbol& symbol = model->locals()[*local];
            if (event.syntax->kind() == ast::AstNodeKind::NewExpression && event.syntax->flag())
            {
                const ast::AstNode* count = event.syntax->first();
                if (!count || count->kind() != ast::AstNodeKind::IntegerLiteralExpression ||
                    !event.syntax->children().empty() || count->integerValue() > 0x7fffffffULL ||
                    arrays.contains(*local)) return unknown;
                arrays.emplace(*local, count->integerValue());
                continue;
            }
            if (event.syntax->kind() != ast::AstNodeKind::ObjectCreationExpression || !symbol.isRaii ||
                objects.contains(*local)) return unknown;
            const auto selectedEntry = constructions.find(event.syntax);
            const ObjectConstructionBinding* selected = selectedEntry == constructions.end() ? nullptr : selectedEntry->second;
            if (selected == nullptr || selected->targetIsStruct || selected->allocatorSelection ||
                selected->targetTypeKey != symbol.typeKey || !sourceTypeSafe(selected->targetTypeOrdinal) ||
                !selected->selectedDefaultArguments.empty() || selected->selectedParameterPackOrdinal ||
                selected->selectedUsesExpandedParameterPack || selected->selectedSynthesizesEmptyParameterPack ||
                !selected->selectedOwnershipTransfers.empty() || !selected->selectedFullExpressionBorrowArgumentOrdinals.empty() ||
                event.syntax->children().size() != selected->selectedArgumentParameterOrdinals.size() ||
                event.syntax->children().size() != selected->selectedConversions.size()) return unknown;
            if (selected->selectedConstructorMemberOrdinal)
            {
                if (selected->resolution != ConstructionResolutionKind::ResolvedExplicit) return unknown;
                const std::size_t ordinal = *selected->selectedConstructorMemberOrdinal;
                if (!constructorBodySafe(ordinal, selected->targetTypeOrdinal)) return unknown;
                const auto& ctor = members->members()[ordinal];
                if (event.syntax->children().size() != ctor.parameters.size()) return unknown;
                std::set<std::size_t> parameters;
                for (std::size_t arg = 0; arg < event.syntax->children().size(); ++arg)
                {
                    const auto* syntax = event.syntax->children()[arg];
                    const std::size_t parameter = selected->selectedArgumentParameterOrdinals[arg];
                    if (syntax == nullptr || parameter >= ctor.parameters.size() || !parameters.insert(parameter).second ||
                        selected->selectedConversions[arg].kind != ProjectConversionKind::Identity) return unknown;
                    if (syntax->kind() != ast::AstNodeKind::IntegerLiteralExpression &&
                        syntax->kind() != ast::AstNodeKind::BooleanLiteralExpression)
                    {
                        const auto* binding = model->nameBinding(syntax, observation.callableOrdinal);
                        if (syntax->kind() != ast::AstNodeKind::IdentifierExpression || binding == nullptr ||
                            (binding->kind != MethodNameBindingKind::Local && binding->kind != MethodNameBindingKind::Parameter) ||
                            binding->typeKey != ctor.parameters[parameter].typeKey) return unknown;
                    }
                }
                const auto* init = construction->constructorInitializerBindingForMember(ordinal);
                if (!init || init->resolution != ConstructorInitializerResolutionKind::NoChainingRequired ||
                    init->explicitInitializer || init->selectedEntryConstructorMemberOrdinal) return unknown;
            }
            else if (selected->selectedImplicitDefault)
            {
                if (selected->resolution != ConstructionResolutionKind::ResolvedImplicitDefault) return unknown;
                if (!event.syntax->children().empty()) return unknown;
                const auto* init = construction->implicitDefaultConstructorBindingForType(selected->targetTypeOrdinal);
                if (!init || init->resolution != ImplicitDefaultConstructorResolutionKind::NoBaseRequired ||
                    init->baseTypeOrdinal || init->directBaseConstructorMemberOrdinal || init->entryConstructorMemberOrdinal)
                    return unknown;
            }
            else return unknown;
            objects.emplace(*local, Object{selected->targetTypeOrdinal, false});
            continue;
        }
        if (event.kind == ArrayReferenceTraceKind::Call)
        {
            const MethodCallBinding* call = model->callBinding(event.syntax, observation.callableOrdinal);
            if (!call || call->resolution != MethodCallResolutionKind::Resolved || !call->selectedMemberOrdinal ||
                *call->selectedMemberOrdinal >= members->members().size() || call->interfaceCall || !call->staticCall)
                return unknown;
            const ClassMemberSymbol& target = members->members()[*call->selectedMemberOrdinal];
            if (target.isVirtual || target.isOverride || target.isAbstract || target.isPrecompiled || target.isUnsafe ||
                !target.declaredErrorTypeKey.empty() || target.effects.contains(sema::Effect::Async) ||
                target.effects.contains(sema::Effect::Generator) || !call->selectedDefaultArguments.empty() ||
                call->selectedParameterPackOrdinal || !trivialStaticInitialization(*members, *names, target.declaringTypeOrdinal))
                return unknown;
            const bool paired = i + 1 < events.size() && events[i + 1].syntax == event.syntax &&
                (events[i + 1].kind == ArrayReferenceTraceKind::Write ||
                 events[i + 1].kind == ArrayReferenceTraceKind::Destruction);
            if (!paired && (!target.parameters.empty() || !call->selectedArgumentSyntaxes.empty() ||
                            !emptySourceBody(target))) return unknown;
            continue;
        }
        if (i == 0 || events[i - 1].kind != ArrayReferenceTraceKind::Call ||
            events[i - 1].syntax != event.syntax) return unknown;
        const MethodCallBinding* call = model->callBinding(event.syntax, observation.callableOrdinal);
        if (!call || !call->selectedMemberOrdinal || *call->selectedMemberOrdinal >= members->members().size())
            return unknown;
        const ClassMemberSymbol& target = members->members()[*call->selectedMemberOrdinal];
        if (event.kind == ArrayReferenceTraceKind::Write)
        {
            const auto owner = localOf(event.source); const auto array = localOf(event.destination);
            const auto effects = model->arrayReferenceWriteSummary(*call->selectedMemberOrdinal);
            const auto direct = model->directArrayReferenceWrites(*call->selectedMemberOrdinal);
            bool supportedIndex = false;
            if (effects.size() == 1 && event.index)
            {
                if (effects[0].index && effects[0].index == event.index && target.parameters.size() == 2)
                    supportedIndex = true;
                else if (!effects[0].index && effects[0].indexParameterOrdinal && target.parameters.size() == 3)
                {
                    const std::size_t parameter = *effects[0].indexParameterOrdinal;
                    if (parameter < target.parameters.size() && parameter != effects[0].sourceParameterOrdinal &&
                        parameter != effects[0].arrayParameterOrdinal &&
                        target.parameters[parameter].passKind == ParameterPassKind::Value &&
                        ProjectTypeConversion::isIntegral(target.parameters[parameter].typeKey))
                        for (std::size_t arg = 0; arg < call->selectedArgumentParameterOrdinals.size() &&
                             arg < call->selectedArgumentSyntaxes.size() && arg < call->selectedConversions.size(); ++arg)
                        {
                            const auto* syntax = call->selectedArgumentSyntaxes[arg];
                            if (call->selectedArgumentParameterOrdinals[arg] == parameter &&
                                provenIndexLiteral(syntax, *event.index, target.parameters[parameter].typeKey) &&
                                call->selectedConversions[arg].kind == ProjectConversionKind::Identity)
                                supportedIndex = true;
                        }
                }
            }
            if (!owner || !array || !event.index || !objects.contains(*owner) || objects[*owner].dead ||
                !arrays.contains(*array) || *event.index < 0 ||
                static_cast<std::uint64_t>(*event.index) >= arrays[*array] || !supportedIndex ||
                call->selectedArgumentSyntaxes.size() != target.parameters.size() || effects.size() != 1 || direct.size() != 1 ||
                !(effects[0] == direct[0]) ||
                callArgument(*call, effects[0].sourceParameterOrdinal) != event.source ||
                callArgument(*call, effects[0].arrayParameterOrdinal) != event.destination ||
                effects[0].sourceParameterOrdinal >= target.parameters.size() ||
                effects[0].arrayParameterOrdinal >= target.parameters.size() ||
                target.parameters[effects[0].sourceParameterOrdinal].typeKey != model->locals()[*owner].typeKey ||
                target.parameters[effects[0].arrayParameterOrdinal].typeKey != model->locals()[*array].typeKey ||
                model->arrayReferenceWritesUnknown(*call->selectedMemberOrdinal) ||
                !model->arrayReferenceHasOnlyModeledPublication(*call->selectedMemberOrdinal)) return unknown;
            slots[{*array, *event.index}] = *owner;
            continue;
        }
        if (event.kind == ArrayReferenceTraceKind::Destruction)
        {
            if ((candidateState == ArrayReferenceRead::MaybeDead && !sawBranch) ||
                event.source == nullptr || event.source->kind() != ast::AstNodeKind::MoveExpression ||
                target.parameters.size() != 1 || callArgument(*call, 0) != event.source ||
                target.parameters[0].ownership != sema::OwnershipKind::Owned ||
                model->arrayReferenceOwnedEffect(*call->selectedMemberOrdinal, 0) != ArrayReferenceOwnedEffect::Destroyed)
                return unknown;
            const auto owner = localOf(event.source->first());
            if (!owner || !objects.contains(*owner) || objects[*owner].dead ||
                *owner >= model->locals().size() || target.parameters[0].typeKey != model->locals()[*owner].typeKey ||
                !destructorReturns(objects[*owner].type)) return unknown;
            objects[*owner].dead = true;
            continue;
        }
        return unknown;
    }
    const auto slot = slots.find({*observedArray, observedIndex});
    if ((candidateState == ArrayReferenceRead::MaybeDead && !sawBranch) ||
        slot == slots.end() || !objects.contains(slot->second) || !objects[slot->second].dead)
        return unknown;
    return ArrayReferenceLatePathResult::ProvenDeadPath;
}

static bool hasLiteralTrueCallerForWhileParameter(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation)
{
    const auto* model = project.methodBodyModel();
    const auto* members = project.memberModel();
    const auto* names = project.nameLookup();
    if (model == nullptr || members == nullptr || names == nullptr || observation.syntax == nullptr ||
        observation.syntax->first() == nullptr) return false;
    const auto* condition = model->nameBinding(observation.syntax->first(), observation.callableOrdinal);
    const auto* callee = model->callableSymbol(observation.callableOrdinal);
    if (condition == nullptr || condition->kind != MethodNameBindingKind::Parameter ||
        condition->parameterOrdinal != 0 || condition->typeKey != "builtin:bool" || callee == nullptr ||
        !callee->isStatic || callee->isUnsafe || callee->isVirtual || callee->isOverride || callee->isAbstract ||
        callee->parameters.size() != 1 || callee->parameters[0].typeKey != "builtin:bool" ||
        callee->parameters[0].passKind != ParameterPassKind::Value ||
        callee->parameters[0].ownership != sema::OwnershipKind::None || callee->declaredTypeKey != "builtin:int")
        return false;
    for (const auto& call : model->calls())
    {
        if (call.resolution != MethodCallResolutionKind::Resolved || !call.staticCall || call.interfaceCall ||
            call.selectedMemberOrdinal != observation.callableOrdinal || call.syntax == nullptr ||
            call.selectedArgumentSyntaxes.size() != 1 || call.selectedArgumentParameterOrdinals.size() != 1 ||
            call.selectedArgumentParameterOrdinals[0] != 0 || call.selectedConversions.size() != 1 ||
            call.selectedConversions[0].kind != ProjectConversionKind::Identity ||
            !call.selectedDefaultArguments.empty() || call.selectedParameterPackOrdinal ||
            call.selectedUsesExpandedParameterPack) continue;
        const auto* argument = call.selectedArgumentSyntaxes[0];
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::BooleanLiteralExpression ||
            argument->tokenKind() != lexer::TokenKind::KwTrue) continue;
        const auto* argumentType = model->expressionTypeBinding(argument, call.callerMethodMemberOrdinal);
        if (argumentType == nullptr || argumentType->typeKey != "builtin:bool") continue;
        const auto* caller = model->callableSymbol(call.callerMethodMemberOrdinal);
        if (caller == nullptr || !caller->isStatic || caller->isUnsafe || caller->isVirtual || caller->isOverride ||
            caller->isAbstract || caller->declaredTypeKey != "builtin:int" ||
            !caller->declaredErrorTypeKey.empty() || caller->effects.contains(sema::Effect::Async) ||
            caller->effects.contains(sema::Effect::Generator) || caller->syntax == nullptr ||
            caller->syntax->first() == nullptr ||
            caller->syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
            caller->syntax->first()->children().size() != 1) continue;
        const auto* statement = caller->syntax->first()->children()[0];
        if (statement == nullptr || statement->kind() != ast::AstNodeKind::ReturnStatement ||
            statement->first() != call.syntax) continue;
        if (caller->declaringTypeOrdinal < names->namedTypes().size() &&
            !trivialStaticInitialization(*members, *names, caller->declaringTypeOrdinal)) continue;
        return true;
    }
    return false;
}

ExperimentalForFacts inspectExperimentalForFirstEvaluation(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation)
{
    ExperimentalForFacts result;
    const auto* model = project.methodBodyModel();
    const auto* loop = observation.syntax;
    if (model != nullptr && model->isBuilt() && !observation.truncated && loop != nullptr &&
        loop->kind() == ast::AstNodeKind::WhileStatement && loop->first() != nullptr &&
        (loop->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression ||
         loop->first()->kind() == ast::AstNodeKind::IdentifierExpression) &&
        loop->second() != nullptr && loop->second()->kind() == ast::AstNodeKind::BlockStatement)
    {
        const auto* conditionType = model->expressionTypeBinding(loop->first(), observation.callableOrdinal);
        if (conditionType == nullptr || conditionType->typeKey != "builtin:bool") return result;
        // For a parameter, True describes the witnessed invocation only;
        // callers passing false and calls without a witness remain unknown.
        if (loop->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto* condition = model->nameBinding(loop->first(), observation.callableOrdinal);
            const auto* callable = model->callableSymbol(observation.callableOrdinal);
            if (condition != nullptr && condition->kind == MethodNameBindingKind::Local &&
                condition->localOrdinal && *condition->localOrdinal < model->locals().size() &&
                callable != nullptr && callable->syntax != nullptr && callable->syntax->first() != nullptr &&
                callable->syntax->first()->kind() == ast::AstNodeKind::BlockStatement)
            {
                const auto& local = model->locals()[*condition->localOrdinal];
                const auto* declaration = local.syntax;
                const auto& statements = callable->syntax->first()->children();
                bool immediatelyBefore = false;
                for (std::size_t i = 1; i < statements.size(); ++i)
                    if (statements[i] == loop && statements[i - 1] == declaration) immediatelyBefore = true;
                if (!immediatelyBefore || local.methodMemberOrdinal != observation.callableOrdinal ||
                    local.typeKey != "builtin:bool" || local.isStatic || local.isReference ||
                    declaration == nullptr || declaration->kind() != ast::AstNodeKind::VariableDeclarationStatement ||
                    declaration->first() == nullptr ||
                    declaration->first()->kind() != ast::AstNodeKind::BooleanLiteralExpression) return result;
                const auto* type = model->expressionTypeBinding(declaration->first(), observation.callableOrdinal);
                if (type == nullptr || type->typeKey != "builtin:bool") return result;
                if (declaration->first()->tokenKind() == lexer::TokenKind::KwTrue)
                    result.firstCondition = ExperimentalForFirstCondition::True;
                else if (declaration->first()->tokenKind() == lexer::TokenKind::KwFalse)
                    result.firstCondition = ExperimentalForFirstCondition::False;
                else return result;
            }
            else if (hasLiteralTrueCallerForWhileParameter(project, observation))
                result.firstCondition = ExperimentalForFirstCondition::True;
            else return result;
        }
        else if (loop->first()->tokenKind() == lexer::TokenKind::KwTrue)
            result.firstCondition = ExperimentalForFirstCondition::True;
        else if (loop->first()->tokenKind() == lexer::TokenKind::KwFalse)
            result.firstCondition = ExperimentalForFirstCondition::False;
        else return result;
        for (const auto* statement : loop->second()->children())
            if (statement != nullptr && statement->kind() == ast::AstNodeKind::BreakStatement)
                for (const auto& exit : model->structuredExits())
                    if (exit.methodMemberOrdinal == observation.callableOrdinal && exit.syntax == statement &&
                        exit.kind == MethodStructuredExitKind::Break)
                        ++result.boundBreakCount;
        return result;
    }
    if (model == nullptr || !model->isBuilt() || observation.truncated || loop == nullptr ||
        loop->kind() != ast::AstNodeKind::ForStatement || loop->first() == nullptr ||
        loop->first()->kind() != ast::AstNodeKind::VariableDeclarationStatement ||
        loop->first()->first() == nullptr || loop->second() == nullptr ||
        loop->second()->kind() != ast::AstNodeKind::BinaryExpression) return result;
    const auto* initializer = loop->first();
    const auto* condition = loop->second();
    const MethodLocalSymbol* induction = nullptr;
    std::size_t inductionOrdinal = 0;
    const auto locals = model->locals();
    for (std::size_t i = 0; i < locals.size(); ++i)
        if (locals[i].methodMemberOrdinal == observation.callableOrdinal && locals[i].syntax == initializer)
        { if (induction != nullptr) return result; induction = &locals[i]; inductionOrdinal = i; }
    if (induction == nullptr || induction->typeKey != "builtin:int" || induction->isReference || induction->isStatic ||
        condition->first() == nullptr || condition->second() == nullptr ||
        condition->first()->kind() != ast::AstNodeKind::IdentifierExpression) return result;
    const auto* left = model->nameBinding(condition->first(), observation.callableOrdinal);
    const auto* leftType = model->expressionTypeBinding(condition->first(), observation.callableOrdinal);
    const auto* rightType = model->expressionTypeBinding(condition->second(), observation.callableOrdinal);
    const auto* initialType = model->expressionTypeBinding(initializer->first(), observation.callableOrdinal);
    const auto* conditionType = model->expressionTypeBinding(condition, observation.callableOrdinal);
    if (left == nullptr || left->kind != MethodNameBindingKind::Local || left->localOrdinal != inductionOrdinal ||
        leftType == nullptr || rightType == nullptr || initialType == nullptr || conditionType == nullptr ||
        leftType->typeKey != "builtin:int" || rightType->typeKey != "builtin:int" ||
        initialType->typeKey != "builtin:int" || conditionType->typeKey != "builtin:bool") return result;
    ProjectIntegerEvaluator integers(nullptr);
    const auto initial = integers.evaluate(initializer->first());
    const auto limit = integers.evaluate(condition->second());
    if (!initial || !limit || initial->typeKey != "builtin:int" || limit->typeKey != "builtin:int") return result;
    const auto lhs = initial->asSigned(), rhs = limit->asSigned();
    bool first = false;
    switch (condition->tokenKind())
    {
        case lexer::TokenKind::Less: first = lhs < rhs; break;
        case lexer::TokenKind::LessEqual: first = lhs <= rhs; break;
        case lexer::TokenKind::Greater: first = lhs > rhs; break;
        case lexer::TokenKind::GreaterEqual: first = lhs >= rhs; break;
        case lexer::TokenKind::EqualEqual: first = lhs == rhs; break;
        case lexer::TokenKind::BangEqual: first = lhs != rhs; break;
        default: return result;
    }
    result.firstCondition = first ? ExperimentalForFirstCondition::True : ExperimentalForFirstCondition::False;
    for (const auto& exit : model->structuredExits())
        if (exit.methodMemberOrdinal == observation.callableOrdinal &&
            exit.kind == MethodStructuredExitKind::Break && exit.targetScopeOrdinal == induction->scopeOrdinal)
            ++result.boundBreakCount;
    // A bound break can be unreachable, conditional or followed by cleanup;
    // this observation intentionally never asserts normal termination.
    return result;
}

static bool inspectExperimentalForPrefix(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation)
{
    const auto* model = project.methodBodyModel(); const auto* members = project.memberModel();
    const auto* names = project.nameLookup(); const auto* inheritance = project.inheritanceModel();
    const auto* construction = project.constructionModel();
    if (model == nullptr || members == nullptr || names == nullptr || inheritance == nullptr ||
        construction == nullptr || !construction->isBuilt() || observation.truncated ||
        !observation.identityComplete || observation.beforeLoop.unknown || observation.beforeLoop.traceUnknown)
        return false;
    const auto* caller = model->callableSymbol(observation.callableOrdinal);
    if (caller == nullptr || caller->syntax == nullptr || caller->syntax->first() == nullptr || caller->isUnsafe ||
        !caller->declaredErrorTypeKey.empty() || caller->effects.contains(sema::Effect::Async) ||
        caller->effects.contains(sema::Effect::Generator) ||
        caller->syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
        !trivialStaticInitialization(*members, *names, caller->declaringTypeOrdinal)) return false;
    const auto locals = model->locals();
    std::map<std::size_t, std::uint64_t> arrays;
    std::set<std::size_t> owners;
    std::size_t cursor = 0;
    const auto& events = observation.beforeLoop.traceEvents;
    const auto consume = [&](ArrayReferenceTraceKind kind, const ast::AstNode* syntax) -> const ArrayReferenceTraceEvent*
    {
        if (cursor >= events.size() || events[cursor].kind != kind || events[cursor].syntax != syntax) return nullptr;
        return &events[cursor++];
    };
    const auto localFor = [&](const ast::AstNode* syntax) -> std::optional<std::size_t>
    {
        if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::IdentifierExpression) return {};
        const auto* binding = model->nameBinding(syntax, observation.callableOrdinal);
        return binding != nullptr && binding->kind == MethodNameBindingKind::Local ? binding->localOrdinal : std::nullopt;
    };
    bool reachedLoop = false;
    for (const auto* statement : caller->syntax->first()->children())
    {
        if (statement == observation.syntax) { reachedLoop = true; break; }
        if (statement == nullptr) return false;
        if (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        {
            std::optional<std::size_t> local;
            for (std::size_t i = 0; i < locals.size(); ++i)
                if (locals[i].methodMemberOrdinal == observation.callableOrdinal && locals[i].syntax == statement)
                { if (local) return false; local = i; }
            if (!local || locals[*local].isStatic || locals[*local].isReference || statement->first() == nullptr) return false;
            const auto* initializer = statement->first();
            if (initializer->kind() == ast::AstNodeKind::ObjectCreationExpression)
            {
                if (!locals[*local].isRaii || !initializer->children().empty() || initializer->first() != nullptr ||
                    initializer->second() != nullptr || initializer->third() != nullptr ||
                    consume(ArrayReferenceTraceKind::Construction, initializer) == nullptr) return false;
                const ObjectConstructionBinding* selected = nullptr;
                for (const auto& binding : construction->constructions())
                    if (binding.callerMemberOrdinal == observation.callableOrdinal && binding.syntax == initializer)
                    { if (selected != nullptr) return false; selected = &binding; }
                if (selected == nullptr || selected->targetIsStruct || selected->allocatorSelection ||
                    selected->targetTypeKey != locals[*local].typeKey || selected->targetTypeOrdinal >= names->namedTypes().size() ||
                    names->namedTypes()[selected->targetTypeOrdinal].isPrecompiled ||
                    inheritance->baseTypeOrdinal(selected->targetTypeOrdinal) ||
                    !trivialStaticInitialization(*members, *names, selected->targetTypeOrdinal) ||
                    !selected->selectedArgumentParameterOrdinals.empty() || !selected->selectedDefaultArguments.empty() ||
                    selected->selectedParameterPackOrdinal || selected->selectedUsesExpandedParameterPack ||
                    selected->selectedSynthesizesEmptyParameterPack || !selected->selectedConversions.empty() ||
                    !selected->selectedOwnershipTransfers.empty() || !selected->selectedFullExpressionBorrowArgumentOrdinals.empty()) return false;
                for (const auto& part : names->namedTypes()[selected->targetTypeOrdinal].parts)
                    if (part.isPrecompiled) return false;
                for (const auto ordinal : members->membersOfType(selected->targetTypeOrdinal))
                {
                    if (ordinal >= members->members().size()) return false;
                    const auto& member = members->members()[ordinal];
                    if (member.declaringTypeOrdinal == selected->targetTypeOrdinal && !member.isStatic &&
                        (member.kind == ClassMemberKind::Field || member.kind == ClassMemberKind::Event)) return false;
                }
                if (selected->resolution == ConstructionResolutionKind::ResolvedImplicitDefault && selected->selectedImplicitDefault)
                {
                    const auto* init = construction->implicitDefaultConstructorBindingForType(selected->targetTypeOrdinal);
                    if (init == nullptr || init->resolution != ImplicitDefaultConstructorResolutionKind::NoBaseRequired ||
                        init->baseTypeOrdinal || init->directBaseConstructorMemberOrdinal || init->entryConstructorMemberOrdinal) return false;
                }
                else if (selected->resolution == ConstructionResolutionKind::ResolvedExplicit && selected->selectedConstructorMemberOrdinal)
                {
                    const auto ordinal = *selected->selectedConstructorMemberOrdinal;
                    const auto* init = construction->constructorInitializerBindingForMember(ordinal);
                    if (ordinal >= members->members().size() || !members->members()[ordinal].parameters.empty() ||
                        !emptySourceBody(members->members()[ordinal]) || init == nullptr ||
                        init->resolution != ConstructorInitializerResolutionKind::NoChainingRequired || init->explicitInitializer ||
                        init->selectedEntryConstructorMemberOrdinal)
                        return false;
                }
                else return false;
                owners.insert(*local);
                continue;
            }
            if (initializer->kind() == ast::AstNodeKind::NewExpression && initializer->flag())
            {
                if (!ProjectTypeConversion::isNativeArray(locals[*local].typeKey) || initializer->first() == nullptr ||
                    initializer->first()->kind() != ast::AstNodeKind::IntegerLiteralExpression ||
                    initializer->first()->integerValue() > 0x7fffffffULL || !initializer->children().empty() ||
                    initializer->second() != nullptr || initializer->third() != nullptr ||
                    consume(ArrayReferenceTraceKind::Construction, initializer) == nullptr) return false;
                const auto element = ProjectTypeConversion::nativeArrayElementType(locals[*local].typeKey);
                const auto elementType = element ? inheritance->typeOrdinalForTypeKey(*element) : std::nullopt;
                if (!elementType || !inheritance->isClass(*elementType)) return false;
                arrays.emplace(*local, initializer->first()->integerValue());
                continue;
            }
            if (initializer->kind() != ast::AstNodeKind::IntegerLiteralExpression &&
                initializer->kind() != ast::AstNodeKind::BooleanLiteralExpression) return false;
            const auto* expression = model->expressionTypeBinding(initializer, observation.callableOrdinal);
            if (expression == nullptr || expression->typeKey != locals[*local].typeKey) return false;
            continue;
        }
        if (statement->kind() != ast::AstNodeKind::ExpressionStatement || statement->first() == nullptr ||
            statement->first()->kind() != ast::AstNodeKind::CallExpression) return false;
        const auto* syntax = statement->first();
        if (consume(ArrayReferenceTraceKind::Call, syntax) == nullptr) return false;
        const auto* call = model->callBinding(syntax, observation.callableOrdinal);
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved || !call->staticCall ||
            call->interfaceCall || !call->selectedMemberOrdinal || *call->selectedMemberOrdinal >= members->members().size() ||
            !call->selectedDefaultArguments.empty() || call->selectedParameterPackOrdinal || call->selectedUsesExpandedParameterPack)
            return false;
        const auto& target = members->members()[*call->selectedMemberOrdinal];
        if (target.isPrecompiled || target.isUnsafe || target.isVirtual || target.isOverride || target.isAbstract ||
            !target.declaredErrorTypeKey.empty() || target.effects.contains(sema::Effect::Async) ||
            target.effects.contains(sema::Effect::Generator) || target.declaredTypeKey != "builtin:void" ||
            !trivialStaticInitialization(*members, *names, target.declaringTypeOrdinal)) return false;
        if (target.parameters.empty() && call->selectedArgumentSyntaxes.empty() && emptySourceBody(target)) continue;
        const auto effects = model->arrayReferenceWriteSummary(*call->selectedMemberOrdinal);
        const auto direct = model->directArrayReferenceWrites(*call->selectedMemberOrdinal);
        if (target.parameters.size() != 2 || effects.size() != 1 || direct.size() != 1 || !(effects[0] == direct[0]) ||
            !effects[0].index || *effects[0].index < 0 || model->arrayReferenceWritesUnknown(*call->selectedMemberOrdinal) ||
            !model->arrayReferenceHasOnlyModeledPublication(*call->selectedMemberOrdinal) ||
            call->selectedArgumentSyntaxes.size() != 2 || call->selectedConversions.size() != 2 ||
            call->selectedConversions[0].kind != ProjectConversionKind::Identity ||
            call->selectedConversions[1].kind != ProjectConversionKind::Identity) return false;
        for (const auto& parameter : target.parameters)
            if (parameter.passKind != ParameterPassKind::Value || parameter.ownership != sema::OwnershipKind::None)
                return false;
        const auto source = localFor(callArgument(*call, effects[0].sourceParameterOrdinal));
        const auto array = localFor(callArgument(*call, effects[0].arrayParameterOrdinal));
        if (!source || !array || !owners.contains(*source) || !arrays.contains(*array) ||
            static_cast<std::uint64_t>(*effects[0].index) >= arrays[*array] ||
            effects[0].sourceParameterOrdinal >= target.parameters.size() ||
            effects[0].arrayParameterOrdinal >= target.parameters.size() ||
            locals[*source].typeKey != target.parameters[effects[0].sourceParameterOrdinal].typeKey ||
            locals[*array].typeKey != target.parameters[effects[0].arrayParameterOrdinal].typeKey) return false;
        const auto* event = consume(ArrayReferenceTraceKind::Write, syntax);
        if (event == nullptr || event->index != effects[0].index ||
            event->source != callArgument(*call, effects[0].sourceParameterOrdinal) ||
            event->destination != callArgument(*call, effects[0].arrayParameterOrdinal)) return false;
    }
    return reachedLoop && cursor == events.size();
}

ExperimentalForBreakPath inspectExperimentalForBreakPath(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation)
{
    ExperimentalForBreakPath unknown;
    const auto first = inspectExperimentalForFirstEvaluation(project, observation);
    const auto* model = project.methodBodyModel(); const auto* members = project.memberModel();
    const auto* names = project.nameLookup(); const auto* inheritance = project.inheritanceModel();
    const auto* destruction = project.destructionModel();
    const auto* loop = observation.syntax;
    const bool isWhile = loop != nullptr && loop->kind() == ast::AstNodeKind::WhileStatement;
    const auto* body = isWhile ? loop->second()
        : loop != nullptr && loop->children().size() == 1 ? loop->children()[0] : nullptr;
    if (first.firstCondition != ExperimentalForFirstCondition::True || first.boundBreakCount != 1 ||
        observation.truncated || !observation.identityComplete || observation.breakStates.size() != 1 ||
        observation.beforeLoop.unknown || observation.beforeLoop.traceUnknown || model == nullptr || members == nullptr ||
        names == nullptr || inheritance == nullptr || destruction == nullptr || !destruction->isBuilt() ||
        body == nullptr || body->kind() != ast::AstNodeKind::BlockStatement) return unknown;
    const MethodLocalSymbol* induction = nullptr;
    const auto locals = model->locals();
    if (!isWhile)
    {
        for (const auto& local : locals)
            if (local.methodMemberOrdinal == observation.callableOrdinal && local.syntax == loop->first())
            { if (induction != nullptr) return unknown; induction = &local; }
        if (induction == nullptr) return unknown;
    }
    const auto destructorReturns = [&](std::size_t type)
    {
        if (type >= names->namedTypes().size() || names->namedTypes()[type].isPrecompiled ||
            inheritance->baseTypeOrdinal(type) || !trivialStaticInitialization(*members, *names, type)) return false;
        for (const auto& part : names->namedTypes()[type].parts) if (part.isPrecompiled) return false;
        for (const auto ordinal : members->membersOfType(type))
        {
            if (ordinal >= members->members().size()) return false;
            const auto& member = members->members()[ordinal];
            if (member.declaringTypeOrdinal != type || member.isStatic) continue;
            if (member.kind == ClassMemberKind::Event) return false;
            if (member.kind == ClassMemberKind::Field &&
                ((!ProjectTypeConversion::isIntegral(member.declaredTypeKey) && member.declaredTypeKey != "builtin:bool") ||
                 member.isReferenceReturn || member.isVolatile || member.isBitField || member.syntax == nullptr ||
                 member.syntax->first() != nullptr)) return false;
        }
        const auto* plan = destruction->planForType(type);
        if (plan == nullptr || (plan->explicitDestructorMemberOrdinal && plan->destructorMemberOrdinals.empty())) return false;
        for (const auto ordinal : plan->destructorMemberOrdinals)
            if (ordinal >= members->members().size() || !emptySourceBody(members->members()[ordinal])) return false;
        return true;
    };
    ExperimentalForBreakPath result;
    std::set<std::size_t> destroyedIdentities;
    for (const auto* statement : body->children())
    {
        if (statement == nullptr) return unknown;
        if (statement->kind() == ast::AstNodeKind::BreakStatement)
        {
            const MethodStructuredExitBinding* selected = nullptr;
            for (const auto& exit : model->structuredExits())
                if (exit.methodMemberOrdinal == observation.callableOrdinal && exit.syntax == statement &&
                    exit.kind == MethodStructuredExitKind::Break)
                { if (selected != nullptr) return unknown; selected = &exit; }
            // A direct break in the while body binds to that loop; for a for
            // loop also check its dedicated induction-variable scope.
            if (selected == nullptr || (!isWhile && selected->targetScopeOrdinal != induction->scopeOrdinal) ||
                !selected->cleanupRegionOrdinals.empty()) return unknown;
            result.reachesAfterLoopFromEntry = true;
            result.preLoopReachabilityProven = inspectExperimentalForPrefix(project, observation);
            return result;
        }
        if (statement->kind() != ast::AstNodeKind::ExpressionStatement || statement->first() == nullptr ||
            statement->first()->kind() != ast::AstNodeKind::CallExpression) return unknown;
        const auto* call = model->callBinding(statement->first(), observation.callableOrdinal);
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved || !call->staticCall ||
            call->interfaceCall || !call->selectedMemberOrdinal || *call->selectedMemberOrdinal >= members->members().size() ||
            !call->selectedDefaultArguments.empty() || call->selectedParameterPackOrdinal ||
            call->selectedUsesExpandedParameterPack) return unknown;
        const auto& target = members->members()[*call->selectedMemberOrdinal];
        if (target.isPrecompiled || target.isUnsafe || target.isAbstract || target.isVirtual || target.isOverride ||
            !target.declaredErrorTypeKey.empty() || target.effects.contains(sema::Effect::Async) ||
            target.effects.contains(sema::Effect::Generator) || target.declaredTypeKey != "builtin:void" ||
            !trivialStaticInitialization(*members, *names, target.declaringTypeOrdinal)) return unknown;
        if (target.parameters.empty() && call->selectedArgumentSyntaxes.empty() && emptySourceBody(target)) continue;
        if (target.parameters.size() != 1 || target.parameters[0].ownership != sema::OwnershipKind::Owned ||
            call->selectedArgumentSyntaxes.size() != 1 || callArgument(*call, 0) != call->selectedArgumentSyntaxes[0] ||
            model->arrayReferenceOwnedEffect(*call->selectedMemberOrdinal, 0) != ArrayReferenceOwnedEffect::Destroyed)
            return unknown;
        const auto* move = call->selectedArgumentSyntaxes[0];
        if (move == nullptr || move->kind() != ast::AstNodeKind::MoveExpression || move->first() == nullptr ||
            move->first()->kind() != ast::AstNodeKind::IdentifierExpression) return unknown;
        const auto* bound = model->nameBinding(move->first(), observation.callableOrdinal);
        if (bound == nullptr || bound->kind != MethodNameBindingKind::Local || !bound->localOrdinal ||
            *bound->localOrdinal >= locals.size()) return unknown;
        const auto& owner = locals[*bound->localOrdinal];
        if (owner.syntax == nullptr || owner.typeKey != target.parameters[0].typeKey) return unknown;
        std::optional<std::size_t> shardOwner;
        for (const auto& [ordinal, syntax] : observation.ownerDeclarations)
            if (syntax == owner.syntax) { if (shardOwner) return unknown; shardOwner = ordinal; }
        if (!shardOwner || observation.beforeLoop.maybeAbsentOwners.contains(*shardOwner)) return unknown;
        const auto found = observation.beforeLoop.owners.find(*shardOwner);
        if (found == observation.beforeLoop.owners.end() || found->second.size() != 1) return unknown;
        const auto identity = *found->second.begin();
        const auto life = observation.beforeLoop.objects.find(identity);
        if (life == observation.beforeLoop.objects.end() || life->second != ArrayReferenceLife::Alive ||
            observation.beforeLoop.unknownObjects.contains(identity) || !destroyedIdentities.insert(identity).second)
            return unknown;
        const auto type = inheritance->typeOrdinalForTypeKey(owner.typeKey);
        if (!type || !destructorReturns(*type)) return unknown;
        result.destroyedOwners.push_back(owner.syntax);
    }
    return unknown;
}

ExperimentalForReadPath inspectExperimentalForReadPath(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation)
{
    ExperimentalForReadPath unknown;
    const auto path = inspectExperimentalForBreakPath(project, observation);
    const auto* model = project.methodBodyModel(); const auto* members = project.memberModel();
    const auto* names = project.nameLookup(); const auto* inheritance = project.inheritanceModel();
    if (!path.reachesAfterLoopFromEntry || !path.preLoopReachabilityProven || path.destroyedOwners.empty() ||
        model == nullptr || members == nullptr || names == nullptr || inheritance == nullptr ||
        !observation.identityComplete || observation.beforeLoop.unknown) return unknown;
    const auto* caller = model->callableSymbol(observation.callableOrdinal);
    if (caller == nullptr || caller->syntax == nullptr || caller->syntax->first() == nullptr ||
        caller->syntax->first()->kind() != ast::AstNodeKind::BlockStatement) return unknown;
    const auto locals = model->locals();
    std::set<std::size_t> destroyed;
    for (const auto* declaration : path.destroyedOwners)
    {
        std::optional<std::size_t> shardOwner;
        for (const auto& [ordinal, syntax] : observation.ownerDeclarations)
            if (syntax == declaration) { if (shardOwner) return unknown; shardOwner = ordinal; }
        if (!shardOwner) return unknown;
        const auto owner = observation.beforeLoop.owners.find(*shardOwner);
        if (owner == observation.beforeLoop.owners.end() || owner->second.size() != 1) return unknown;
        destroyed.insert(*owner->second.begin());
    }
    bool afterLoop = false;
    for (const auto* statement : caller->syntax->first()->children())
    {
        if (!afterLoop) { if (statement == observation.syntax) afterLoop = true; continue; }
        if (statement == nullptr) return unknown;
        const ast::AstNode* expression = nullptr;
        if (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        {
            if (statement->isReference() || statement->first() == nullptr) return unknown;
            expression = statement->first();
            if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                expression->kind() == ast::AstNodeKind::BooleanLiteralExpression)
            {
                const MethodLocalSymbol* local = nullptr;
                for (const auto& candidate : locals)
                    if (candidate.methodMemberOrdinal == observation.callableOrdinal && candidate.syntax == statement)
                    { if (local != nullptr) return unknown; local = &candidate; }
                const auto* type = model->expressionTypeBinding(expression, observation.callableOrdinal);
                if (local == nullptr || local->isStatic || local->isReference || type == nullptr ||
                    local->typeKey != type->typeKey) return unknown;
                continue;
            }
        }
        else if (statement->kind() == ast::AstNodeKind::ExpressionStatement) expression = statement->first();
        if (expression == nullptr || expression->kind() != ast::AstNodeKind::CallExpression) return unknown;
        const auto* call = model->callBinding(expression, observation.callableOrdinal);
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved || !call->selectedMemberOrdinal ||
            *call->selectedMemberOrdinal >= members->members().size() || call->interfaceCall ||
            !call->selectedArgumentSyntaxes.empty() || !call->selectedDefaultArguments.empty() ||
            call->selectedParameterPackOrdinal || call->selectedUsesExpandedParameterPack) return unknown;
        const auto& target = members->members()[*call->selectedMemberOrdinal];
        if (target.isPrecompiled || target.isUnsafe || target.isVirtual || target.isOverride || target.isAbstract ||
            !target.parameters.empty() || !target.declaredErrorTypeKey.empty() ||
            target.effects.contains(sema::Effect::Async) || target.effects.contains(sema::Effect::Generator) ||
            !trivialStaticInitialization(*members, *names, target.declaringTypeOrdinal)) return unknown;
        if (call->staticCall)
        {
            if (statement->kind() != ast::AstNodeKind::ExpressionStatement || target.declaredTypeKey != "builtin:void" ||
                !emptySourceBody(target)) return unknown;
            continue;
        }
        const auto* receiver = call->receiverSyntax;
        if (receiver == nullptr || receiver->kind() != ast::AstNodeKind::IndexExpression || receiver->first() == nullptr ||
            receiver->first()->kind() != ast::AstNodeKind::IdentifierExpression || target.isReferenceReturn ||
            target.syntax == nullptr || target.syntax->first() == nullptr ||
            target.syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
            target.syntax->first()->children().size() != 1) return unknown;
        const auto* ret = target.syntax->first()->children()[0];
        if (ret == nullptr || ret->kind() != ast::AstNodeKind::ReturnStatement || ret->first() == nullptr ||
            (ret->first()->kind() != ast::AstNodeKind::IntegerLiteralExpression &&
             ret->first()->kind() != ast::AstNodeKind::BooleanLiteralExpression)) return unknown;
        const auto* retType = model->expressionTypeBinding(ret->first(), *call->selectedMemberOrdinal);
        if (retType == nullptr || retType->typeKey != target.declaredTypeKey) return unknown;
        if (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        {
            const MethodLocalSymbol* result = nullptr;
            for (const auto& local : locals)
                if (local.methodMemberOrdinal == observation.callableOrdinal && local.syntax == statement)
                { if (result != nullptr) return unknown; result = &local; }
            if (result == nullptr || result->isStatic || result->isReference || result->typeKey != target.declaredTypeKey)
                return unknown;
        }
        const auto* arrayBinding = model->nameBinding(receiver->first(), observation.callableOrdinal);
        const auto* index = receiver->children().size() == 1 ? receiver->children()[0]
            : receiver->children().empty() ? receiver->second() : nullptr;
        if (arrayBinding == nullptr || arrayBinding->kind != MethodNameBindingKind::Local || !arrayBinding->localOrdinal ||
            *arrayBinding->localOrdinal >= locals.size() || index == nullptr ||
            index->kind() != ast::AstNodeKind::IntegerLiteralExpression || index->integerValue() > 0x7fffffffffffffffULL)
            return unknown;
        const auto* arrayDeclaration = locals[*arrayBinding->localOrdinal].syntax;
        std::optional<std::size_t> shardArray;
        for (const auto& [ordinal, syntax] : observation.arrayDeclarations)
            if (syntax == arrayDeclaration) { if (shardArray) return unknown; shardArray = ordinal; }
        if (!shardArray || observation.beforeLoop.escapedArrays.contains(*shardArray)) return unknown;
        const ArrayReferenceSlot slot{*shardArray, static_cast<std::int64_t>(index->integerValue())};
        const auto values = observation.beforeLoop.slots.find(slot);
        if (observation.beforeLoop.read(slot) != ArrayReferenceRead::Alive || values == observation.beforeLoop.slots.end() ||
            values->second.size() != 1 || !destroyed.contains(*values->second.begin())) return unknown;
        return {true, receiver};
    }
    return unknown;
}

void reportArrayReferenceLatePaths(const ProjectCompilation& project,
    std::span<const ArrayReferenceReplayObservation> observations,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::set<std::pair<std::size_t, const ast::AstNode*>> reported;
    for (const auto& observation : observations)
    {
        if (observation.syntax == nullptr ||
            evaluateArrayReferenceLatePath(project, observation) != ArrayReferenceLatePathResult::ProvenDeadPath ||
            !reported.emplace(observation.callableOrdinal, observation.syntax).second)
            continue;
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand, diagnostics::DiagnosticSeverity::Error,
            observation.syntax->range(),
            observation.potentialIndex
                ? "reference to a potentially destroyed object cannot be used as an instance method receiver"
                : "reference to a destroyed object cannot be used as an instance method receiver");
    }
    const auto* model = project.methodBodyModel();
    if (model == nullptr) return;
    for (const auto& observation : model->arrayReferenceForObservations())
    {
        const auto proof = inspectExperimentalForReadPath(project, observation);
        if (!proof.provenFromCallableEntry || proof.receiver == nullptr ||
            !reported.emplace(observation.callableOrdinal, proof.receiver).second) continue;
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand, diagnostics::DiagnosticSeverity::Error,
            proof.receiver->range(),
            "reference to a destroyed object cannot be used as an instance method receiver");
    }
}
}
