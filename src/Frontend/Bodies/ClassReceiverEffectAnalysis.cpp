#include "nativesharp/Frontend/ClassReceiverEffectGraph.h"
#include "nativesharp/Frontend/ClassLocalScalarField.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"

#include <unordered_set>

namespace nativesharp::frontend
{
// Bounded to the audited statement and scalar-effect subset below; this is not
// a general proof of exception handling or of arbitrary finally bodies.
static constexpr bool kExperimentalClassLocalFinally = true;
// Narrow caught-throw proof: the matching source exception must have a resolved
// implicit root constructor and no fields, destructor or static initialization.
static constexpr bool kExperimentalClassLocalCaughtThrow = true;

// A static access may run the declaring type's lazy initializer. Until its
// effects are summarized, only a source type with no initializer is proven.
static bool classLocalStaticInitializationIsTrivial(
    const ProjectMemberModel& model, std::size_t typeOrdinal, bool requireStaticField = true)
{
    bool foundStaticField = false;
    for (const std::size_t ordinal : model.membersOfType(typeOrdinal))
    {
        if (ordinal >= model.members().size()) return false;
        const ClassMemberSymbol& member = model.members()[ordinal];
        if (member.declaringTypeOrdinal != typeOrdinal || !member.isStatic) continue;
        if (member.kind == ClassMemberKind::Constructor) return false;
        if (member.kind == ClassMemberKind::Field) foundStaticField = true;
        if (member.kind == ClassMemberKind::Field && !member.isConst &&
            (member.isPrecompiled || member.syntax == nullptr || member.syntax->first() != nullptr))
            return false;
    }
    return foundStaticField || !requireStaticField;
}
std::span<const ClassReceiverEffects> ProjectMethodBodyModel::classReceiverEffectSummaries() const noexcept
{
    return classReceiverEffectSummaries_;
}

ClassReceiverEffects ProjectMethodBodyModel::classReceiverEffectSummary(
    std::size_t callableOrdinal) const noexcept
{
    return callableOrdinal < classReceiverEffectSummaries_.size()
        ? classReceiverEffectSummaries_[callableOrdinal] : ClassReceiverEffects{};
}

std::vector<bool> ProjectMethodBodyModel::classLocalUsesAreProvenSafe(
    std::span<const std::size_t> localOrdinals,
    const ProjectVirtualDispatchModel* virtualDispatch, bool deferVirtual,
    const ProjectConstructionModel* construction) const
{
    std::vector<bool> safe(localOrdinals.size(), false);
    if (localOrdinals.empty() || localOrdinals[0] >= locals_.size()) return safe;
    const std::size_t callable = locals_[localOrdinals[0]].methodMemberOrdinal;
    const ClassMemberSymbol* caller = methodSymbol(callable);
    if (caller == nullptr || caller->syntax == nullptr || caller->syntax->first() == nullptr ||
        caller->isUnsafe || caller->effects.contains(sema::Effect::Async) ||
        caller->effects.contains(sema::Effect::Generator)) return safe;
    const bool exceptionalCallable = !caller->declaredErrorTypeKey.empty();
    bool sawExplicitThrow = false;
    std::unordered_map<std::string, std::vector<std::size_t>> byName;
    std::unordered_map<std::size_t, std::size_t> byOrdinal;
    for (std::size_t i = 0; i < localOrdinals.size(); ++i)
    {
        if (localOrdinals[i] >= locals_.size() || locals_[localOrdinals[i]].methodMemberOrdinal != callable)
            continue;
        byName[locals_[localOrdinals[i]].name].push_back(i);
        if (!byOrdinal.emplace(localOrdinals[i], i).second) continue;
        safe[i] = true;
    }
    // A direct borrowed alias in the owner's scope or a descendant is another
    // name for the candidate and cannot outlive that owner.
    // Reassignment, capture, return and passing the alias are still checked by
    // the same per-use gate as the original object.
    std::unordered_map<const ast::AstNode*, std::size_t> provenBorrowInitializers;
    for (std::size_t ordinal = 0; ordinal < locals_.size(); ++ordinal)
    {
        const MethodLocalSymbol& alias = locals_[ordinal];
        if (alias.methodMemberOrdinal != callable || alias.ownership != sema::OwnershipKind::Borrowed ||
            alias.syntax == nullptr || alias.syntax->first() == nullptr ||
            alias.syntax->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
            !alias.localBorrowBindingOrdinal.has_value() ||
            *alias.localBorrowBindingOrdinal >= localBorrowBindings_.size()) continue;
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[*alias.localBorrowBindingOrdinal];
        if (borrow.sourceKind != MethodLocalBorrowSourceKind::OwnerLocal ||
            !borrow.sourceOwnerLocalOrdinal.has_value() ||
            borrow.sourceLocalOrdinal != borrow.sourceOwnerLocalOrdinal) continue;
        const auto root = byOrdinal.find(*borrow.sourceOwnerLocalOrdinal);
        if (root == byOrdinal.end() || *borrow.sourceOwnerLocalOrdinal >= locals_.size()) continue;
        const MethodLocalSymbol& owner = locals_[*borrow.sourceOwnerLocalOrdinal];
        if (!owner.isScope || owner.isRaii || owner.syntax == nullptr || owner.syntax->first() != nullptr ||
            !scopeContains(owner.scopeOrdinal, alias.scopeOrdinal) ||
            borrow.maximumValidScopeOrdinal != owner.scopeOrdinal ||
            alias.typeKey != owner.typeKey) continue;
        byName[alias.name].push_back(root->second);
        if (!byOrdinal.emplace(ordinal, root->second).second) continue;
        provenBorrowInitializers.emplace(alias.syntax, root->second);
    }
    const auto safeCall = [&](const ast::AstNode* callSyntax, const ast::AstNode* receiver)
    {
        const MethodCallBinding* call = callBinding(callSyntax, callable);
        if (call == nullptr || call->receiverSyntax != receiver) return false;
        const ClassMemberSymbol* target = call->selectedMemberOrdinal.has_value()
            ? methodSymbol(*call->selectedMemberOrdinal) : nullptr;
        std::optional<std::size_t> effectiveOrdinal = call->selectedMemberOrdinal;
        bool deferred = false;
        if (target != nullptr && (target->isVirtual || target->isOverride))
        {
            const MethodNameBinding* receiverName = nameBinding(receiver, callable);
            if (receiverName == nullptr || !receiverName->localOrdinal.has_value() ||
                *receiverName->localOrdinal >= locals_.size()) return false;
            const MethodLocalSymbol& receiverLocal = locals_[*receiverName->localOrdinal];
            const auto exactType = namedTypeOrdinalForTypeKey(receiverLocal.typeKey);
            if (!receiverLocal.isScope || receiverLocal.syntax == nullptr ||
                receiverLocal.syntax->first() != nullptr || !exactType.has_value() ||
                exactType != call->receiverTypeOrdinal) return false;
            if (virtualDispatch == nullptr)
            {
                if (!deferVirtual) return false;
                deferred = true;
            }
            else
            {
                if (!call->receiverTypeOrdinal.has_value()) return false;
                const auto slot = virtualDispatch->slotForMember(*call->selectedMemberOrdinal);
                const ClassVirtualTable* table = virtualDispatch->classTable(*call->receiverTypeOrdinal);
                if (!slot.has_value() || table == nullptr || *slot >= table->slotMemberOrdinals.size() ||
                    *slot >= table->slotAbstract.size() || table->slotAbstract[*slot]) return false;
                effectiveOrdinal = table->slotMemberOrdinals[*slot];
                target = methodSymbol(*effectiveOrdinal);
            }
        }
        bool allowed = call->resolution == MethodCallResolutionKind::Resolved && !call->staticCall &&
            !call->interfaceCall && !call->baseReceiver && call->selectedDefaultArguments.empty() &&
            !call->selectedParameterPackOrdinal.has_value() && target != nullptr;
        if (allowed)
        {
            const ClassReceiverEffects effect = classReceiverEffectSummary(*effectiveOrdinal);
            allowed = !target->isPrecompiled &&
                !target->isAbstract && !classLocalCallableMaySuspend(*target) &&
                (target->declaredTypeKey == "builtin:void" ||
                    isClassLocalScalarType(target->declaredTypeKey)) &&
                target->declaredErrorTypeKey.empty() &&
                (deferred || (!effect.unknown && !effect.published && !effect.returned)) &&
                call->selectedArgumentSyntaxes.size() == target->parameters.size();
        }
        for (std::size_t i = 0; i < call->selectedArgumentSyntaxes.size(); ++i)
        {
            const ast::AstNode* argument = call->selectedArgumentSyntaxes[i];
            if (argument != nullptr && (argument->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                argument->kind() == ast::AstNodeKind::BooleanLiteralExpression)) continue;
            if (argument != nullptr && argument->kind() == ast::AstNodeKind::FloatingLiteralExpression &&
                target != nullptr && i < call->selectedArgumentParameterOrdinals.size() &&
                i < call->selectedConversions.size())
            {
                const std::size_t parameterOrdinal = call->selectedArgumentParameterOrdinals[i];
                const MethodExpressionTypeBinding* value = expressionTypeBinding(argument, callable);
                if (parameterOrdinal < target->parameters.size() && value != nullptr &&
                    (value->typeKey == "builtin:float" || value->typeKey == "builtin:double") &&
                    target->parameters[parameterOrdinal].typeKey == value->typeKey &&
                    target->parameters[parameterOrdinal].passKind == ParameterPassKind::Value &&
                    target->parameters[parameterOrdinal].ownership == sema::OwnershipKind::None &&
                    call->selectedConversions[i].kind == ProjectConversionKind::Identity) continue;
                allowed = false;
                continue;
            }
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression ||
                target == nullptr || i >= call->selectedArgumentParameterOrdinals.size()) { allowed = false; continue; }
            const std::size_t parameterOrdinal = call->selectedArgumentParameterOrdinals[i];
            if (parameterOrdinal >= target->parameters.size()) { allowed = false; continue; }
            const MethodParameterSymbol& parameter = target->parameters[parameterOrdinal];
            const MethodNameBinding* name = nameBinding(argument, callable);
            const MethodExpressionTypeBinding* value = expressionTypeBinding(argument, callable);
            if (name == nullptr || value == nullptr || !isClassLocalScalarType(value->typeKey) ||
                parameter.typeKey != value->typeKey || parameter.passKind != ParameterPassKind::Value ||
                parameter.ownership != sema::OwnershipKind::None) { allowed = false; continue; }
            if (name->kind == MethodNameBindingKind::Local && name->localOrdinal.has_value() &&
                *name->localOrdinal < locals_.size() && !locals_[*name->localOrdinal].isReference &&
                locals_[*name->localOrdinal].typeKey == value->typeKey) continue;
            if (name->kind == MethodNameBindingKind::Parameter && name->parameterOrdinal.has_value() &&
                *name->parameterOrdinal < caller->parameters.size() &&
                caller->parameters[*name->parameterOrdinal].passKind == ParameterPassKind::Value &&
                caller->parameters[*name->parameterOrdinal].typeKey == value->typeKey) continue;
            allowed = false;
        }
        return allowed;
    };
    const auto safeFieldUse = [&](const ast::AstNode* fieldSyntax, const ast::AstNode* context,
                                  const ast::AstNode* receiver)
    {
        if (fieldSyntax == nullptr || context == nullptr || memberModel_ == nullptr) return false;
        const MethodNameBinding* binding = nameBinding(fieldSyntax, callable);
        if (binding == nullptr || binding->kind != MethodNameBindingKind::ExplicitObjectField ||
            binding->receiverSyntax != receiver || !binding->receiverTypeOrdinal.has_value() ||
            binding->candidateMemberOrdinals.size() != 1 ||
            binding->candidateMemberOrdinals[0] >= memberModel_->members().size()) return false;
        const ClassMemberSymbol& field = memberModel_->members()[binding->candidateMemberOrdinals[0]];
        if (field.declaringTypeOrdinal != *binding->receiverTypeOrdinal ||
            !isClassLocalScalarFieldCandidate(field)) return false;
        if (context->kind() == ast::AstNodeKind::ReturnStatement && context->first() == fieldSyntax &&
            !context->isReference() && !caller->isReferenceReturn && caller->declaredTypeKey == field.declaredTypeKey)
            return true;
        if (context->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            context->first() == fieldSyntax && context->typeSyntax() != nullptr && nameLookup_ != nullptr &&
            context->auxiliaryText().empty() && !context->isReference() &&
            context->typeSyntax()->referenceQualifier() == ast::ReferenceQualifierSyntaxKind::None &&
            memberModel_->canonicalTypeKey(context->typeSyntax(), *nameLookup_) == field.declaredTypeKey)
            return true;
        if (context->kind() == ast::AstNodeKind::AssignmentExpression && context->first() == fieldSyntax &&
            context->tokenKind() == lexer::TokenKind::Equal && !context->isReference() &&
            isClassLocalScalarLiteral(context->second(), field.declaredTypeKey))
        {
            const MethodExpressionTypeBinding* value = expressionTypeBinding(context->second(), callable);
            return value != nullptr && isClassLocalScalarType(value->typeKey) &&
                ((field.declaredTypeKey != "builtin:float" && field.declaredTypeKey != "builtin:double") ||
                 value->typeKey == field.declaredTypeKey);
        }
        return false;
    };
    struct Visit { const ast::AstNode* node; const ast::AstNode* parent; const ast::AstNode* grandparent; };
    const auto safeFinallyCallable = [&]()
    {
        if (exceptionalCallable || memberModel_ == nullptr || nameLookup_ == nullptr) return false;
        std::unordered_set<const ast::AstNode*> candidateSyntax;
        for (const std::size_t ordinal : localOrdinals)
            if (ordinal < locals_.size() && locals_[ordinal].syntax != nullptr)
                candidateSyntax.insert(locals_[ordinal].syntax);
        const auto staticScalar = [&](const ast::AstNode* syntax) -> const ClassMemberSymbol*
        {
            if (syntax == nullptr) return nullptr;
            const MethodNameBinding* binding = nameBinding(syntax, callable);
            if (binding == nullptr || binding->kind != MethodNameBindingKind::StaticField ||
                binding->candidateMemberOrdinals.size() != 1 ||
                binding->candidateMemberOrdinals[0] >= memberModel_->members().size()) return nullptr;
            if (syntax->kind() == ast::AstNodeKind::MemberExpression)
            {
                const MethodNameBinding* receiver = nameBinding(syntax->first(), callable);
                if (receiver == nullptr || receiver->kind != MethodNameBindingKind::TypeReference)
                    return nullptr;
            }
            else if (syntax->kind() != ast::AstNodeKind::IdentifierExpression) return nullptr;
            const ClassMemberSymbol& field = memberModel_->members()[binding->candidateMemberOrdinals[0]];
            return field.kind == ClassMemberKind::Field && field.isStatic && !field.isPrecompiled &&
                !field.isVolatile && !field.isBitField && field.syntax != nullptr &&
                !field.syntax->isReference() && isClassLocalScalarType(field.declaredTypeKey) &&
                classLocalStaticInitializationIsTrivial(*memberModel_, field.declaringTypeOrdinal)
                ? &field : nullptr;
        };
        const auto scalarValue = [&](const ast::AstNode* syntax, const std::string& type)
        {
            if (syntax == nullptr || !isClassLocalScalarType(type)) return false;
            const MethodExpressionTypeBinding* binding = expressionTypeBinding(syntax, callable);
            if (binding == nullptr || binding->typeKey != type) return false;
            if (isClassLocalScalarLiteral(syntax, type)) return true;
            const ClassMemberSymbol* field = staticScalar(syntax);
            return field != nullptr && field->declaredTypeKey == type;
        };
        // 0 outside try, 1 protected body, 2 catch, 3 finally.
        struct Statement { const ast::AstNode* syntax; unsigned region; };
        std::vector<Statement> statements{{caller->syntax->first(), 0}};
        std::size_t tryCount = 0;
        const MethodExceptionBinding* caught = nullptr;
        const ast::AstNode* caughtThrow = nullptr;
        while (!statements.empty())
        {
            const Statement current = statements.back();
            statements.pop_back();
            const ast::AstNode* syntax = current.syntax;
            if (syntax == nullptr) return false;
            switch (syntax->kind())
            {
                case ast::AstNodeKind::BlockStatement:
                    if (!syntax->auxiliaryText().empty()) return false;
                    for (const ast::AstNode* child : syntax->children())
                        statements.push_back({child, current.region});
                    break;
                case ast::AstNodeKind::TryStatement:
                    if (++tryCount != 1 || current.region != 0 || !syntax->auxiliaryText().empty() ||
                        syntax->first() == nullptr || syntax->second() == nullptr ||
                        syntax->children().size() > 1) return false;
                    if (!syntax->children().empty())
                    {
                        const ast::AstNode* clause = syntax->children().front();
                        if (clause == nullptr || clause->kind() != ast::AstNodeKind::CatchClause ||
                            clause->first() == nullptr || clause->second() != nullptr) return false;
                        const MethodExceptionBinding* resolved = nullptr;
                        for (const MethodExceptionBinding& binding : exceptionBindings_)
                            if (binding.methodMemberOrdinal == callable && binding.syntax == clause &&
                                binding.kind == MethodExceptionOperationKind::Catch)
                            { if (resolved != nullptr) return false; resolved = &binding; }
                        if (resolved == nullptr || resolved->catchAll || resolved->hasFilter ||
                            !resolved->exceptionTypeOrdinal.has_value()) return false;
                        caught = resolved;
                        statements.push_back({clause->first(), 2});
                    }
                    statements.push_back({syntax->first(), 1});
                    statements.push_back({syntax->second(), 3});
                    break;
                case ast::AstNodeKind::VariableDeclarationStatement:
                {
                    if (candidateSyntax.contains(syntax))
                    {
                        if (current.region == 3 || syntax->first() != nullptr) return false;
                        break;
                    }
                    if (syntax->typeSyntax() == nullptr || syntax->isReference() ||
                        !syntax->auxiliaryText().empty() || syntax->first() == nullptr) return false;
                    const std::string type = memberModel_->canonicalTypeKey(syntax->typeSyntax(), *nameLookup_);
                    if (!scalarValue(syntax->first(), type)) return false;
                    break;
                }
                case ast::AstNodeKind::ReturnStatement:
                    if (current.region == 3 || syntax->isReference() || caller->isReferenceReturn ||
                        (syntax->first() == nullptr ? caller->declaredTypeKey != "builtin:void" :
                            !scalarValue(syntax->first(), caller->declaredTypeKey))) return false;
                    break;
                case ast::AstNodeKind::ExpressionStatement:
                {
                    const ast::AstNode* assignment = syntax->first();
                    if (assignment == nullptr || assignment->kind() != ast::AstNodeKind::AssignmentExpression ||
                        assignment->tokenKind() != lexer::TokenKind::Equal || assignment->isReference()) return false;
                    const ClassMemberSymbol* field = staticScalar(assignment->first());
                    if (field == nullptr || !scalarValue(assignment->second(), field->declaredTypeKey)) return false;
                    break;
                }
                case ast::AstNodeKind::ThrowStatement:
                    if (!kExperimentalClassLocalCaughtThrow || current.region != 1 || caught == nullptr ||
                        caughtThrow != nullptr || syntax->first() == nullptr ||
                        syntax->first()->kind() != ast::AstNodeKind::ObjectCreationExpression ||
                        !syntax->first()->children().empty() || syntax->first()->first() != nullptr ||
                        syntax->first()->second() != nullptr || syntax->first()->third() != nullptr) return false;
                    caughtThrow = syntax;
                    break;
                default: return false;
            }
        }
        if (caughtThrow != nullptr)
        {
            const MethodExceptionBinding* thrown = nullptr;
            for (const MethodExceptionBinding& binding : exceptionBindings_)
                if (binding.methodMemberOrdinal == callable && binding.syntax == caughtThrow &&
                    binding.kind == MethodExceptionOperationKind::Throw)
                { if (thrown != nullptr) return false; thrown = &binding; }
            if (thrown == nullptr || !thrown->exceptionTypeOrdinal.has_value() ||
                thrown->exceptionTypeOrdinal != caught->exceptionTypeOrdinal) return false;
            // Method-body analysis runs before constructor binding. Its deferred
            // result is never sufficient for HIR: construction validates again.
            if (construction == nullptr) return deferVirtual;
            const ObjectConstructionBinding* binding = construction->uniqueClassLocalConstruction(
                caughtThrow->first(), callable);
            const std::size_t type = *thrown->exceptionTypeOrdinal;
            if (binding == nullptr || binding->targetTypeOrdinal != type || binding->targetIsStruct ||
                binding->resolution != ConstructionResolutionKind::ResolvedImplicitDefault ||
                !binding->selectedImplicitDefault || binding->selectedConstructorMemberOrdinal.has_value() ||
                binding->allocatorSelection || !binding->selectedDefaultArguments.empty() ||
                !binding->selectedArgumentParameterOrdinals.empty() || !binding->selectedConversions.empty() ||
                binding->selectedParameterPackOrdinal.has_value() || binding->selectedUsesExpandedParameterPack ||
                binding->selectedSynthesizesEmptyParameterPack || !binding->selectedOwnershipTransfers.empty() ||
                !binding->selectedFullExpressionBorrowArgumentOrdinals.empty() ||
                type >= nameLookup_->namedTypes().size() || nameLookup_->namedTypes()[type].isPrecompiled ||
                nameLookup_->namedTypes()[type].kind != NamedTypeDeclarationKind::Class ||
                nameLookup_->namedTypes()[type].parts.empty() ||
                !classLocalStaticInitializationIsTrivial(*memberModel_, type, false)) return false;
            for (const auto& part : nameLookup_->namedTypes()[type].parts)
                if (part.isPrecompiled || part.syntax == nullptr) return false;
            for (const std::size_t ordinal : memberModel_->membersOfType(type))
            {
                if (ordinal >= memberModel_->members().size()) return false;
                const ClassMemberSymbol& member = memberModel_->members()[ordinal];
                if (member.declaringTypeOrdinal == type && !member.isStatic &&
                    (member.kind == ClassMemberKind::Field || member.kind == ClassMemberKind::Event ||
                     member.kind == ClassMemberKind::Destructor)) return false;
            }
            const auto* init = construction->implicitDefaultConstructorBindingForType(type);
            if (init == nullptr || init->resolution != ImplicitDefaultConstructorResolutionKind::NoBaseRequired ||
                init->baseTypeOrdinal.has_value() || init->directBaseConstructorMemberOrdinal.has_value() ||
                init->entryConstructorMemberOrdinal.has_value()) return false;
        }
        return tryCount == 1;
    };
    std::vector<Visit> pending{{caller->syntax->first(), nullptr, nullptr}};
    while (!pending.empty())
    {
        const Visit visit = pending.back();
        pending.pop_back();
        const ast::AstNode* node = visit.node;
        if (node == nullptr) continue;
        // Capture lists are encoded as text, not IdentifierExpression nodes; do
        // not mistake an unvisited capture for absence of a use. Raw assembly,
        // unsafe blocks and exception paths are outside this initial proof.
        if (node->kind() == ast::AstNodeKind::LambdaExpression ||
            node->kind() == ast::AstNodeKind::InlineAssemblyStatement ||
            (node->kind() == ast::AstNodeKind::TryStatement &&
                (!kExperimentalClassLocalFinally || !safeFinallyCallable())) ||
            node->auxiliaryText() == "unsafe-block")
            return std::vector<bool>(localOrdinals.size(), false);
        if (node->kind() == ast::AstNodeKind::ThrowStatement)
        {
            if (kExperimentalClassLocalCaughtThrow && !exceptionalCallable && safeFinallyCallable())
                continue;
            const ast::AstNode* value = node->first();
            if (!exceptionalCallable || value == nullptr ||
                value->kind() != ast::AstNodeKind::ObjectCreationExpression ||
                !value->children().empty() || value->first() != nullptr ||
                value->second() != nullptr || value->third() != nullptr)
                return std::vector<bool>(localOrdinals.size(), false);
            sawExplicitThrow = true;
        }
        if (exceptionalCallable && (node->kind() == ast::AstNodeKind::CallExpression ||
            (node->kind() == ast::AstNodeKind::ObjectCreationExpression &&
                (visit.parent == nullptr || visit.parent->kind() != ast::AstNodeKind::ThrowStatement))))
            return std::vector<bool>(localOrdinals.size(), false);
        if (node->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto found = byName.find(node->text());
            if (found != byName.end())
            {
                const MethodNameBinding* name = nameBinding(node, callable);
                if (name == nullptr ||
                    (name->kind == MethodNameBindingKind::Local && !name->localOrdinal.has_value()))
                {
                    for (const std::size_t i : found->second) safe[i] = false;
                }
                else if (name->localOrdinal.has_value())
                {
                    const auto candidate = byOrdinal.find(*name->localOrdinal);
                    if (candidate != byOrdinal.end())
                    {
                        const std::size_t i = candidate->second;
                        const auto borrowInitializer = provenBorrowInitializers.find(visit.parent);
                        bool allowed = borrowInitializer != provenBorrowInitializers.end() &&
                            borrowInitializer->second == i && visit.parent->first() == node;
                        if (!allowed) allowed = visit.parent != nullptr &&
                            visit.parent->kind() == ast::AstNodeKind::MemberExpression &&
                            visit.parent->first() == node && visit.grandparent != nullptr;
                        if (allowed && borrowInitializer == provenBorrowInitializers.end())
                        {
                            const bool call = visit.grandparent->kind() == ast::AstNodeKind::CallExpression &&
                                visit.grandparent->first() == visit.parent && safeCall(visit.grandparent, node);
                            allowed = call || safeFieldUse(visit.parent, visit.grandparent, node);
                        }
                        if (!allowed) safe[i] = false;
                    }
                }
            }
        }
        pending.push_back({node->first(), node, visit.parent});
        pending.push_back({node->second(), node, visit.parent});
        pending.push_back({node->third(), node, visit.parent});
        for (const ast::AstNode* child : node->children()) pending.push_back({child, node, visit.parent});
        std::vector<const ast::TypeSyntax*> types;
        if (node->typeSyntax() != nullptr) types.push_back(node->typeSyntax());
        if (node->throwsTypeSyntax() != nullptr) types.push_back(node->throwsTypeSyntax());
        for (const ast::TypeSyntax* type : node->typeSyntaxes()) types.push_back(type);
        while (!types.empty())
        {
            const ast::TypeSyntax* type = types.back();
            types.pop_back();
            if (type == nullptr) continue;
            pending.push_back({type->constExpression(), nullptr, nullptr});
            for (const auto& modifier : type->modifiers())
                pending.push_back({modifier.arrayCount, nullptr, nullptr});
            for (const ast::TypeSyntax* argument : type->typeArguments()) types.push_back(argument);
            types.push_back(type->functionReturnType());
            types.push_back(type->functionErrorType());
        }
    }
    return exceptionalCallable && !sawExplicitThrow ? std::vector<bool>(localOrdinals.size(), false) : safe;
}

void ProjectMethodBodyModel::finalizeClassReceiverEffects()
{
    std::vector<ClassReceiverEffectEdge> edges;
    for (std::size_t ordinal = 0; ordinal < classReceiverEffectSummaries_.size(); ++ordinal)
    {
        const ClassMemberSymbol* member = methodSymbol(ordinal);
        if (member == nullptr || !callableBodyAnalyzed(ordinal) || member->isPrecompiled ||
            member->isStatic || !member->hasImplicitThis || member->declaringTypeIsStruct ||
            member->isAbstract ||
            classLocalCallableMaySuspend(*member) ||
            (member->kind == ClassMemberKind::Constructor &&
                (member->isUnsafe || !member->declaredErrorTypeKey.empty())) ||
            member->syntax == nullptr) continue;
        const ast::AstNode* body = member->syntax->first();
        if (body == nullptr || body->kind() != ast::AstNodeKind::BlockStatement) continue;
        const auto statements = body->children();
        ClassReceiverEffects& summary = classReceiverEffectSummaries_[ordinal];
        if (statements.empty() && member->parameters.empty()) { summary.unknown = false; continue; }
        const auto safeStaticWrite = [&](const ast::AstNode* statement)
        {
            const ast::AstNode* expression = statement != nullptr ? statement->first() : nullptr;
            if (statement == nullptr || statement->kind() != ast::AstNodeKind::ExpressionStatement ||
                expression == nullptr || expression->kind() != ast::AstNodeKind::AssignmentExpression ||
                expression->tokenKind() != lexer::TokenKind::Equal || memberModel_ == nullptr) return false;
            const auto staticScalar = [&](const ast::AstNode* node) -> const ClassMemberSymbol*
            {
                const MethodNameBinding* binding = node != nullptr ? nameBinding(node, ordinal) : nullptr;
                if (binding == nullptr || binding->kind != MethodNameBindingKind::StaticField ||
                    binding->candidateMemberOrdinals.size() != 1 ||
                    binding->candidateMemberOrdinals[0] >= memberModel_->members().size()) return nullptr;
                if (node->kind() == ast::AstNodeKind::MemberExpression)
                {
                    const MethodNameBinding* receiver = nameBinding(node->first(), ordinal);
                    if (receiver == nullptr || receiver->kind != MethodNameBindingKind::TypeReference)
                        return nullptr;
                }
                else if (node->kind() != ast::AstNodeKind::IdentifierExpression) return nullptr;
                const ClassMemberSymbol& field = memberModel_->members()[binding->candidateMemberOrdinals[0]];
                return field.kind == ClassMemberKind::Field && field.isStatic && !field.isPrecompiled &&
                    !field.isVolatile && !field.isBitField && field.syntax != nullptr &&
                    !field.syntax->isReference() && isClassLocalScalarType(field.declaredTypeKey) &&
                    classLocalStaticInitializationIsTrivial(*memberModel_, field.declaringTypeOrdinal)
                    ? &field : nullptr;
            };
            const ClassMemberSymbol* target = staticScalar(expression->first());
            const auto safeLeaf = [&](const ast::AstNode* node, const std::string& type)
            {
                if (node == nullptr) return false;
                const MethodExpressionTypeBinding* valueType = expressionTypeBinding(node, ordinal);
                if (valueType == nullptr || valueType->typeKey != type) return false;
                if (node->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                    node->kind() == ast::AstNodeKind::BooleanLiteralExpression) return true;
                const ClassMemberSymbol* field = staticScalar(node);
                return field != nullptr && field->declaredTypeKey == type;
            };
            const ast::AstNode* value = expression->second();
            bool safeValue = target != nullptr && safeLeaf(value, target->declaredTypeKey);
            if (target != nullptr && value != nullptr && value->kind() == ast::AstNodeKind::BinaryExpression &&
                (value->tokenKind() == lexer::TokenKind::Plus || value->tokenKind() == lexer::TokenKind::Minus) &&
                builtins::isIntegerTypeKey(target->declaredTypeKey))
            {
                const MethodExpressionTypeBinding* valueType = expressionTypeBinding(value, ordinal);
                safeValue = valueType != nullptr && valueType->typeKey == target->declaredTypeKey &&
                    safeLeaf(value->first(), target->declaredTypeKey) &&
                    safeLeaf(value->second(), target->declaredTypeKey);
            }
            return safeValue;
        };
        if (member->kind == ClassMemberKind::Constructor && memberModel_ != nullptr)
        {
            bool safeBody = true;
            for (const MethodParameterSymbol& parameter : member->parameters)
                if (!isClassLocalScalarType(parameter.typeKey) || parameter.passKind != ParameterPassKind::Value ||
                    parameter.ownership != sema::OwnershipKind::None || parameter.isParameterPack)
                    safeBody = false;
            for (const ast::AstNode* statement : statements)
            {
                if (!safeBody) break;
                if (safeStaticWrite(statement)) continue;
                const ast::AstNode* assignment = statement != nullptr ? statement->first() : nullptr;
                if (statement == nullptr || statement->kind() != ast::AstNodeKind::ExpressionStatement ||
                    assignment == nullptr || assignment->kind() != ast::AstNodeKind::AssignmentExpression ||
                    assignment->tokenKind() != lexer::TokenKind::Equal || assignment->first() == nullptr ||
                    assignment->first()->kind() != ast::AstNodeKind::MemberExpression ||
                    assignment->first()->first() == nullptr ||
                    assignment->first()->first()->kind() != ast::AstNodeKind::ThisExpression)
                { safeBody = false; break; }
                const MethodNameBinding* fieldBinding = nameBinding(assignment->first(), ordinal);
                if (fieldBinding == nullptr || fieldBinding->kind != MethodNameBindingKind::ExplicitThisField ||
                    fieldBinding->candidateMemberOrdinals.size() != 1 ||
                    fieldBinding->candidateMemberOrdinals[0] >= memberModel_->members().size())
                { safeBody = false; break; }
                const ClassMemberSymbol& field = memberModel_->members()[fieldBinding->candidateMemberOrdinals[0]];
                const MethodExpressionTypeBinding* valueType = expressionTypeBinding(assignment->second(), ordinal);
                bool safeValue = isClassLocalScalarLiteral(assignment->second(), field.declaredTypeKey);
                if (!safeValue && assignment->second() != nullptr &&
                    assignment->second()->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const MethodNameBinding* parameter = nameBinding(assignment->second(), ordinal);
                    safeValue = parameter != nullptr && parameter->kind == MethodNameBindingKind::Parameter &&
                        parameter->parameterOrdinal.has_value() &&
                        *parameter->parameterOrdinal < member->parameters.size() &&
                        member->parameters[*parameter->parameterOrdinal].typeKey == field.declaredTypeKey;
                }
                if (field.declaringTypeOrdinal != member->declaringTypeOrdinal ||
                    !isClassLocalScalarFieldCandidate(field) || !safeValue ||
                    valueType == nullptr || !isClassLocalScalarType(valueType->typeKey) ||
                    ((field.declaredTypeKey == "builtin:float" || field.declaredTypeKey == "builtin:double") &&
                        valueType->typeKey != field.declaredTypeKey) ||
                    (assignment->second()->kind() == ast::AstNodeKind::IdentifierExpression &&
                        valueType->typeKey != field.declaredTypeKey))
                { safeBody = false; break; }
            }
            if (safeBody) { summary.unknown = false; continue; }
        }
        if (statements.size() != 1 || statements[0] == nullptr) continue;
        const ast::AstNode* statement = statements[0];
        const ast::AstNode* expression = statement->first();
        // Other destructor bodies, including calls, remain unproven.
        if (member->kind == ClassMemberKind::Destructor && member->parameters.empty() &&
            !member->isUnsafe && member->declaredErrorTypeKey.empty() && safeStaticWrite(statement))
        { summary.unknown = false; continue; }
        if (member->kind == ClassMemberKind::Destructor) continue;
        const auto plainThisField = [&](const ast::AstNode* fieldSyntax) -> const ClassMemberSymbol*
        {
            if (fieldSyntax == nullptr || fieldSyntax->kind() != ast::AstNodeKind::MemberExpression ||
                fieldSyntax->first() == nullptr || fieldSyntax->first()->kind() != ast::AstNodeKind::ThisExpression)
                return nullptr;
            const MethodNameBinding* binding = nameBinding(fieldSyntax, ordinal);
            if (binding == nullptr || binding->kind != MethodNameBindingKind::ExplicitThisField ||
                binding->candidateMemberOrdinals.size() != 1 || memberModel_ == nullptr ||
                binding->candidateMemberOrdinals[0] >= memberModel_->members().size()) return nullptr;
            const ClassMemberSymbol& field = memberModel_->members()[binding->candidateMemberOrdinals[0]];
            return field.declaringTypeOrdinal == member->declaringTypeOrdinal &&
                isClassLocalScalarFieldCandidate(field) ? &field : nullptr;
        };
        if (statement->kind() == ast::AstNodeKind::ReturnStatement && member->parameters.empty())
        {
            const ClassMemberSymbol* field = plainThisField(expression);
            if (field != nullptr && member->declaredTypeKey == field->declaredTypeKey)
            {
                summary.unknown = false;
                continue;
            }
        }
        if (statement->kind() == ast::AstNodeKind::ExpressionStatement && expression != nullptr &&
            expression->kind() == ast::AstNodeKind::AssignmentExpression &&
            expression->tokenKind() == lexer::TokenKind::Equal && member->parameters.size() == 1 &&
            member->declaredTypeKey == "builtin:void" && expression->second() != nullptr &&
            expression->second()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const ClassMemberSymbol* field = plainThisField(expression->first());
            const MethodNameBinding* parameter = nameBinding(expression->second(), ordinal);
            if (field != nullptr && member->parameters[0].typeKey == field->declaredTypeKey &&
                member->parameters[0].passKind == ParameterPassKind::Value &&
                member->parameters[0].ownership == sema::OwnershipKind::None && parameter != nullptr &&
                parameter->kind == MethodNameBindingKind::Parameter && parameter->parameterOrdinal == 0)
            {
                summary.unknown = false;
                continue;
            }
        }
        // This narrow scalar copy cannot inspect or publish the receiver. Do not
        // generalize it to user-defined types, references, conversions or calls.
        if (member->parameters.size() == 1 && member->parameters[0].typeKey == "builtin:int" &&
            member->parameters[0].passKind == ParameterPassKind::Value &&
            member->parameters[0].ownership == sema::OwnershipKind::None &&
            statement->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            statement->typeSyntax() != nullptr &&
            statement->typeSyntax()->baseTokenKind() == lexer::TokenKind::KwInt &&
            statement->typeSyntax()->modifiers().empty() && expression != nullptr &&
            expression->kind() == ast::AstNodeKind::IdentifierExpression &&
            expression->text() == member->parameters[0].name)
        {
            summary.unknown = false;
            continue;
        }
        if (!member->parameters.empty()) continue;
        if (statement->kind() == ast::AstNodeKind::ReturnStatement && expression != nullptr &&
            expression->kind() == ast::AstNodeKind::ThisExpression)
        {
            summary.returned = true;
            summary.unknown = false;
            continue;
        }
        if (statement->kind() != ast::AstNodeKind::ExpressionStatement || expression == nullptr ||
            expression->kind() != ast::AstNodeKind::CallExpression) continue;
        const MethodCallBinding* call = callBinding(expression, ordinal);
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved ||
            call->staticCall || call->interfaceCall || call->baseReceiver ||
            (!call->implicitThis && !call->explicitThis) ||
            !call->selectedMemberOrdinal.has_value() || !call->selectedArgumentSyntaxes.empty() ||
            !call->selectedDefaultArguments.empty() || call->selectedParameterPackOrdinal.has_value())
            continue;
        const ClassMemberSymbol* target = methodSymbol(*call->selectedMemberOrdinal);
        if (target == nullptr || target->declaredTypeKey != "builtin:void" ||
            !target->parameters.empty()) continue;
        const bool closed = !target->isVirtual && !target->isOverride && !target->isAbstract &&
            !target->isPrecompiled;
        summary.unknown = false;
        edges.push_back({ordinal, call->selectedMemberOrdinal, true, false, closed, {}});
    }
    classReceiverEffectSummaries_ = propagateClassReceiverEffects(
        std::move(classReceiverEffectSummaries_), edges);
}
}
