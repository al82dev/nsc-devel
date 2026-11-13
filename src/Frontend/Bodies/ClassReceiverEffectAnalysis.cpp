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

// EN: Returns the null-initialized owned class field written by a constructor statement of the exact
//     form `field = new T();` or `this.field = new T();`, or null when the statement has any other
//     shape. The receiver is used only as the base address of its own owned slot: the creation has
//     no arguments, initializer or user conversion, so the child cannot observe the receiver.
//     The child constructor and destructor are proven separately by the construction model.
// ES: Devuelve el campo owned de clase inicializado a null que escribe una sentencia del constructor
//     con la forma exacta `field = new T();` o `this.field = new T();`, o null si tiene otra forma.
//     El receptor solo se usa como direccion base de su propia ranura owned: la creacion no tiene
//     argumentos, inicializador ni conversion de usuario, por lo que el hijo no puede observar el
//     receptor. El constructor y el destructor del hijo se demuestran aparte en el modelo de
//     construccion.
// Complexity: O(1). Effects: none.
static const ClassMemberSymbol* classLocalOwnedChildAssignmentField(
    const ProjectMethodBodyModel& model, const ProjectMemberModel& members,
    const ClassMemberSymbol& constructor, std::size_t constructorOrdinal, const ast::AstNode* statement)
{
    const ast::AstNode* assignment = statement != nullptr ? statement->first() : nullptr;
    if (statement == nullptr || statement->kind() != ast::AstNodeKind::ExpressionStatement ||
        assignment == nullptr || assignment->kind() != ast::AstNodeKind::AssignmentExpression ||
        assignment->tokenKind() != lexer::TokenKind::Equal || assignment->isReference() ||
        model.callBinding(assignment, constructorOrdinal) != nullptr) return nullptr;
    const ast::AstNode* target = assignment->first();
    const ast::AstNode* value = assignment->second();
    if (target == nullptr || value == nullptr) return nullptr;
    const MethodNameBinding* binding = model.nameBinding(target, constructorOrdinal);
    if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
        binding->candidateMemberOrdinals[0] >= members.members().size()) return nullptr;
    const bool implicitField = target->kind() == ast::AstNodeKind::IdentifierExpression &&
        binding->kind == MethodNameBindingKind::ImplicitField;
    const bool explicitField = target->kind() == ast::AstNodeKind::MemberExpression &&
        target->first() != nullptr && target->first()->kind() == ast::AstNodeKind::ThisExpression &&
        binding->kind == MethodNameBindingKind::ExplicitThisField;
    if (!implicitField && !explicitField) return nullptr;
    const ClassMemberSymbol& field = members.members()[binding->candidateMemberOrdinals[0]];
    if (field.declaringTypeOrdinal != constructor.declaringTypeOrdinal ||
        !isClassLocalNullOwnedClassField(field, true)) return nullptr;
    if (value->kind() != ast::AstNodeKind::ObjectCreationExpression || !value->children().empty() ||
        value->first() != nullptr || value->second() != nullptr || value->third() != nullptr ||
        value->isReference()) return nullptr;
    const MethodExpressionTypeBinding* valueType = model.expressionTypeBinding(value, constructorOrdinal);
    const MethodExpressionTypeBinding* targetType = model.expressionTypeBinding(target, constructorOrdinal);
    if (valueType == nullptr || targetType == nullptr || valueType->typeKey != field.declaredTypeKey ||
        targetType->typeKey != field.declaredTypeKey) return nullptr;
    return &field;
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

// EN: General receiver escape summary (M1-01, docs/3.0/main-fixes/10). True unless the body of
//     `callableOrdinal` provably keeps its receiver, and every object its owned or reference fields hold,
//     inside the call:
//     - `this` appears only as the base of a member access;
//     - a reference, array or struct field of `this` is used only as a member-access base, an assignment
//       target, an operand of `==`/`!=`, or an indexed element used the same way (never stored, returned,
//       passed, converted or bound to a local);
//     - no lambda, method group of `this`, address-of or `ref` of a field, inline assembly or unsafe block;
//     - every call on `this` or `base` is resolved, non-virtual (or a base call) and itself non-escaping;
//     - the method is source, concrete, not async, not a generator, not unsafe and not a `ref` return.
//     Recursion is answered conservatively (escape).
// ES: Resumen general de escape del receptor (M1-01, doc 10). Verdadero salvo que el cuerpo de
//     `callableOrdinal` demuestre que conserva su receptor, y todo objeto que guardan sus campos owned o de
//     referencia, dentro de la llamada: `this` solo como base de acceso a miembro; un campo de referencia,
//     arreglo o struct de `this` solo como base de acceso, destino de asignacion, operando de `==`/`!=` o
//     elemento indexado usado igual (nunca guardado, retornado, pasado, convertido o ligado a un local);
//     sin lambdas, grupos de metodos de `this`, direcciones o `ref` de campos, ensamblador ni bloques
//     unsafe; toda llamada sobre `this` o `base` resuelta, no virtual (o llamada base) y sin escape; el
//     metodo es fuente, concreto, no async, no generador, no unsafe y sin retorno `ref`. La recursion se
//     responde de forma conservadora (escape).
// Complexity: O(body nodes + callees on `this`), memoized. Effects: the cache.
bool ProjectMethodBodyModel::classReceiverMayEscape(std::size_t callableOrdinal) const
{
    if (const auto found = classReceiverEscapeCache_.find(callableOrdinal); found != classReceiverEscapeCache_.end())
        return found->second;
    if (classReceiverEscapeInProgress_.contains(callableOrdinal)) return true;
    const ClassMemberSymbol* symbol = methodSymbol(callableOrdinal);
    if (symbol == nullptr || symbol->isPrecompiled || symbol->isAbstract || symbol->isUnsafe ||
        symbol->isReferenceReturn || symbol->syntax == nullptr || symbol->syntax->first() == nullptr ||
        classLocalCallableMaySuspend(*symbol) || memberModel_ == nullptr)
        return classReceiverEscapeCache_.emplace(callableOrdinal, true).first->second;
    classReceiverEscapeInProgress_.insert(callableOrdinal);
    const auto isComparison = [](const ast::AstNode* node)
    {
        return node != nullptr && node->kind() == ast::AstNodeKind::BinaryExpression &&
            (node->tokenKind() == lexer::TokenKind::EqualEqual || node->tokenKind() == lexer::TokenKind::BangEqual);
    };
    const auto ownFieldOfReferenceType = [&](const ast::AstNode* node)
    {
        if (node->kind() != ast::AstNodeKind::IdentifierExpression &&
            node->kind() != ast::AstNodeKind::MemberExpression) return false;
        const MethodNameBinding* binding = nameBinding(node, callableOrdinal);
        if (binding == nullptr || (binding->kind != MethodNameBindingKind::ImplicitField &&
                                   binding->kind != MethodNameBindingKind::ExplicitThisField &&
                                   binding->kind != MethodNameBindingKind::ExplicitBaseField)) return false;
        return !isClassLocalScalarType(binding->typeKey);
    };
    // EN: A value position that does not let the object leave: member base, assignment target, comparison.
    // ES: Una posicion de valor que no deja salir al objeto: base de miembro, destino de asignacion,
    //     comparacion.
    const auto contained = [&](const ast::AstNode* node, const ast::AstNode* parent)
    {
        if (parent == nullptr) return false;
        if (parent->kind() == ast::AstNodeKind::MemberExpression && parent->first() == node) return true;
        if (parent->kind() == ast::AstNodeKind::AssignmentExpression && parent->first() == node &&
            parent->tokenKind() == lexer::TokenKind::Equal) return true;
        return isComparison(parent);
    };
    struct Visit final
    {
        const ast::AstNode* node;
        const ast::AstNode* parent;
        const ast::AstNode* grandparent;
    };
    bool escapes = false;
    std::vector<Visit> pending{{symbol->syntax->first(), nullptr, nullptr}};
    while (!pending.empty() && !escapes)
    {
        const Visit visit = pending.back();
        pending.pop_back();
        const ast::AstNode* node = visit.node;
        if (node == nullptr) continue;
        switch (node->kind())
        {
            case ast::AstNodeKind::LambdaExpression:
            case ast::AstNodeKind::InlineAssemblyStatement:
                escapes = true;
                continue;
            case ast::AstNodeKind::ThisExpression:
                if (visit.parent == nullptr || visit.parent->kind() != ast::AstNodeKind::MemberExpression ||
                    visit.parent->first() != node) escapes = true;
                continue;
            case ast::AstNodeKind::ReturnStatement:
                if (node->isReference()) escapes = true;
                break;
            case ast::AstNodeKind::UnaryExpression:
                if (node->tokenKind() == lexer::TokenKind::Amp && node->first() != nullptr)
                {
                    const ast::AstNode* operand = node->first();
                    while (operand != nullptr && (operand->kind() == ast::AstNodeKind::IndexExpression ||
                           (operand->kind() == ast::AstNodeKind::MemberExpression && operand->first() != nullptr &&
                            operand->first()->kind() != ast::AstNodeKind::ThisExpression)))
                        operand = operand->first();
                    if (operand != nullptr)
                        if (const MethodNameBinding* binding = nameBinding(operand, callableOrdinal);
                            binding != nullptr && (binding->kind == MethodNameBindingKind::ImplicitField ||
                                                   binding->kind == MethodNameBindingKind::ExplicitThisField ||
                                                   binding->kind == MethodNameBindingKind::ExplicitBaseField))
                            escapes = true;
                }
                break;
            case ast::AstNodeKind::CallExpression:
                if (const MethodCallBinding* call = callBinding(node, callableOrdinal);
                    call != nullptr && !call->staticCall &&
                    (call->receiverSyntax == nullptr || call->baseReceiver ||
                     call->receiverSyntax->kind() == ast::AstNodeKind::ThisExpression))
                {
                    const ClassMemberSymbol* target = call->resolution == MethodCallResolutionKind::Resolved &&
                        call->selectedMemberOrdinal.has_value() ? methodSymbol(*call->selectedMemberOrdinal) : nullptr;
                    if (target == nullptr || call->interfaceCall ||
                        ((target->isVirtual || target->isOverride || target->isAbstract) && !call->baseReceiver) ||
                        classReceiverMayEscape(*call->selectedMemberOrdinal)) escapes = true;
                }
                else if (call == nullptr) escapes = true;
                break;
            default:
                break;
        }
        if (escapes) break;
        if (node->auxiliaryText() == "unsafe-block") { escapes = true; break; }
        if (node->kind() == ast::AstNodeKind::IdentifierExpression ||
            node->kind() == ast::AstNodeKind::MemberExpression)
            if (const MethodNameBinding* binding = nameBinding(node, callableOrdinal); binding != nullptr)
            {
                // EN: A method group of the receiver used as a value builds a delegate over `this`.
                // ES: Un grupo de metodos del receptor usado como valor construye un delegado sobre `this`.
                if ((binding->kind == MethodNameBindingKind::ImplicitMethodGroup ||
                     binding->kind == MethodNameBindingKind::ExplicitThisMethodGroup) &&
                    (visit.parent == nullptr || visit.parent->kind() != ast::AstNodeKind::CallExpression ||
                     visit.parent->first() != node)) { escapes = true; break; }
                if (ownFieldOfReferenceType(node) && !contained(node, visit.parent))
                {
                    const bool indexedContained = visit.parent != nullptr &&
                        visit.parent->kind() == ast::AstNodeKind::IndexExpression && visit.parent->first() == node &&
                        contained(visit.parent, visit.grandparent);
                    if (!indexedContained) { escapes = true; break; }
                }
            }
        pending.push_back({node->first(), node, visit.parent});
        pending.push_back({node->second(), node, visit.parent});
        pending.push_back({node->third(), node, visit.parent});
        for (const ast::AstNode* child : node->children()) pending.push_back({child, node, visit.parent});
    }
    classReceiverEscapeInProgress_.erase(callableOrdinal);
    return classReceiverEscapeCache_.insert_or_assign(callableOrdinal, escapes).first->second;
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
        const auto ownerType = namedTypeOrdinalForTypeKey(owner.typeKey);
        const auto aliasType = namedTypeOrdinalForTypeKey(alias.typeKey);
        const bool safeType = alias.typeKey == owner.typeKey ||
            (inheritanceModel_ != nullptr && ownerType.has_value() && aliasType.has_value() &&
             inheritanceModel_->isDerivedFrom(*ownerType, *aliasType));
        if (!owner.isScope || owner.isRaii || owner.syntax == nullptr || owner.syntax->first() != nullptr ||
            !scopeContains(owner.scopeOrdinal, alias.scopeOrdinal) ||
            borrow.maximumValidScopeOrdinal != owner.scopeOrdinal || !safeType) continue;
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
            const auto receiverOwner = byOrdinal.find(*receiverName->localOrdinal);
            if (receiverOwner == byOrdinal.end() || receiverOwner->second >= localOrdinals.size() ||
                localOrdinals[receiverOwner->second] >= locals_.size()) return false;
            const MethodLocalSymbol& receiverLocal = locals_[localOrdinals[receiverOwner->second]];
            const auto exactType = namedTypeOrdinalForTypeKey(receiverLocal.typeKey);
            if (!receiverLocal.isScope || receiverLocal.syntax == nullptr ||
                receiverLocal.syntax->first() != nullptr || !exactType.has_value() ||
                !call->receiverTypeOrdinal.has_value() || inheritanceModel_ == nullptr ||
                !inheritanceModel_->isDerivedFrom(*exactType, *call->receiverTypeOrdinal, true)) return false;
            if (virtualDispatch == nullptr)
            {
                if (!deferVirtual) return false;
                deferred = true;
            }
            else
            {
                if (!call->receiverTypeOrdinal.has_value()) return false;
                const auto slot = virtualDispatch->slotForMember(*call->selectedMemberOrdinal);
                const ClassVirtualTable* table = virtualDispatch->classTable(*exactType);
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
            if (argument != nullptr && argument->kind() == ast::AstNodeKind::UnaryExpression &&
                (argument->tokenKind() == lexer::TokenKind::Plus || argument->tokenKind() == lexer::TokenKind::Minus) &&
                argument->first() != nullptr &&
                argument->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
                target != nullptr && i < call->selectedArgumentParameterOrdinals.size() &&
                i < call->selectedConversions.size() && callBinding(argument, callable) == nullptr)
            {
                const std::size_t parameterOrdinal = call->selectedArgumentParameterOrdinals[i];
                const MethodExpressionTypeBinding* value = expressionTypeBinding(argument, callable);
                if (parameterOrdinal < target->parameters.size() && value != nullptr &&
                    builtins::isIntegerTypeKey(value->typeKey) &&
                    target->parameters[parameterOrdinal].typeKey == value->typeKey &&
                    target->parameters[parameterOrdinal].passKind == ParameterPassKind::Value &&
                    target->parameters[parameterOrdinal].ownership == sema::OwnershipKind::None &&
                    call->selectedConversions[i].kind == ProjectConversionKind::Identity) continue;
                allowed = false;
                continue;
            }
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression ||
                target == nullptr || i >= call->selectedArgumentParameterOrdinals.size() ||
                i >= call->selectedConversions.size()) { allowed = false; continue; }
            const std::size_t parameterOrdinal = call->selectedArgumentParameterOrdinals[i];
            if (parameterOrdinal >= target->parameters.size()) { allowed = false; continue; }
            const MethodParameterSymbol& parameter = target->parameters[parameterOrdinal];
            const MethodNameBinding* name = nameBinding(argument, callable);
            const MethodExpressionTypeBinding* value = expressionTypeBinding(argument, callable);
            const ProjectConversionKind conversion = call->selectedConversions[i].kind;
            // Built-in integer conversions cannot run user code or carry the receiver.
            const bool integralConversion = value != nullptr && builtins::isIntegerTypeKey(value->typeKey) &&
                builtins::isIntegerTypeKey(parameter.typeKey) &&
                (conversion == ProjectConversionKind::IntegralPromotion || conversion == ProjectConversionKind::Numeric ||
                 conversion == ProjectConversionKind::ConstantInteger);
            if (name == nullptr || value == nullptr || !isClassLocalScalarType(value->typeKey) ||
                !((parameter.typeKey == value->typeKey && conversion == ProjectConversionKind::Identity) ||
                    integralConversion) || parameter.passKind != ParameterPassKind::Value ||
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
        // EN: General path (doc 10): any arguments and result, when the effective target's receiver cannot
        //     escape. An argument that names the local itself is rejected by the visitor below, because its
        //     parent is not a member access.
        // ES: Via general (doc 10): cualquier argumento y resultado, cuando el receptor del destino efectivo
        //     no puede escapar. Un argumento que nombra al propio local lo rechaza el recorrido de abajo,
        //     porque su padre no es un acceso a miembro.
        if (!allowed && call->resolution == MethodCallResolutionKind::Resolved && !call->staticCall &&
            !call->interfaceCall && !call->baseReceiver && !call->selectedParameterPackOrdinal.has_value() &&
            target != nullptr && effectiveOrdinal.has_value() && !target->isPrecompiled && !target->isAbstract &&
            !target->isReferenceReturn && !classLocalCallableMaySuspend(*target) &&
            target->declaredErrorTypeKey.empty() && (deferred || !classReceiverMayEscape(*effectiveOrdinal)))
            allowed = true;
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
        // A scalar method that never refers to its receiver cannot publish it,
        // regardless of the scalar control flow inside the method. Consult
        // bindings as well as syntax so implicit instance accesses are rejected.
        const auto receiverIndependent = [&]()
        {
            if (member->kind != ClassMemberKind::Method || member->isUnsafe || member->isReferenceReturn ||
                member->declaredOwnership != sema::OwnershipKind::None ||
                (member->declaredTypeKey != "builtin:void" && !isClassLocalScalarType(member->declaredTypeKey)))
                return false;
            for (const MethodParameterSymbol& parameter : member->parameters)
                if (!isClassLocalScalarType(parameter.typeKey) || parameter.passKind != ParameterPassKind::Value ||
                    parameter.ownership != sema::OwnershipKind::None || parameter.isParameterPack)
                    return false;
            std::vector<const ast::AstNode*> pending{body};
            std::unordered_set<const ast::AstNode*> visited;
            while (!pending.empty())
            {
                const ast::AstNode* node = pending.back();
                pending.pop_back();
                if (node == nullptr || !visited.insert(node).second) continue;
                if (node->kind() == ast::AstNodeKind::ThisExpression ||
                    node->kind() == ast::AstNodeKind::BaseExpression ||
                    node->kind() == ast::AstNodeKind::LambdaExpression ||
                    node->kind() == ast::AstNodeKind::InlineAssemblyStatement ||
                    node->auxiliaryText() == "unsafe-block") return false;
                if (node->kind() == ast::AstNodeKind::IdentifierExpression ||
                    node->kind() == ast::AstNodeKind::MemberExpression)
                {
                    const MethodNameBinding* binding = nameBinding(node, ordinal);
                    if (binding == nullptr) return false;
                    switch (binding->kind)
                    {
                        case MethodNameBindingKind::ImplicitField:
                        case MethodNameBindingKind::ImplicitMethodGroup:
                        case MethodNameBindingKind::ThisReference:
                        case MethodNameBindingKind::BaseReference:
                        case MethodNameBindingKind::ExplicitThisField:
                        case MethodNameBindingKind::ExplicitThisMethodGroup:
                        case MethodNameBindingKind::ExplicitBaseField:
                        case MethodNameBindingKind::ExplicitBaseMethodGroup:
                        case MethodNameBindingKind::ImplicitEvent:
                        case MethodNameBindingKind::ExplicitThisEvent:
                            return false;
                        default: break;
                    }
                }
                if (node->kind() == ast::AstNodeKind::CallExpression)
                {
                    const MethodCallBinding* call = callBinding(node, ordinal);
                    if (call != nullptr && (call->implicitThis || call->explicitThis || call->baseReceiver ||
                        call->forwardedReceiver)) return false;
                }
                pending.push_back(node->first());
                pending.push_back(node->second());
                pending.push_back(node->third());
                for (const ast::AstNode* child : node->children()) pending.push_back(child);
                std::vector<const ast::TypeSyntax*> types;
                if (node->typeSyntax() != nullptr) types.push_back(node->typeSyntax());
                if (node->throwsTypeSyntax() != nullptr) types.push_back(node->throwsTypeSyntax());
                for (const ast::TypeSyntax* type : node->typeSyntaxes()) types.push_back(type);
                while (!types.empty())
                {
                    const ast::TypeSyntax* type = types.back();
                    types.pop_back();
                    if (type == nullptr) continue;
                    pending.push_back(type->constExpression());
                    for (const auto& modifier : type->modifiers()) pending.push_back(modifier.arrayCount);
                    for (const ast::TypeSyntax* argument : type->typeArguments()) types.push_back(argument);
                    types.push_back(type->functionReturnType());
                    types.push_back(type->functionErrorType());
                }
            }
            return true;
        };
        if (receiverIndependent()) { summary.unknown = false; continue; }
        // Prove scalar field operations and closed calls on the same receiver
        // without permitting its address or a reference to a field to escape.
        std::vector<ClassReceiverEffectEdge> scalarReceiverEdges;
        const auto scalarReceiverOperations = [&]()
        {
            if (member->kind != ClassMemberKind::Method || member->isUnsafe || member->isReferenceReturn ||
                member->declaredOwnership != sema::OwnershipKind::None || memberModel_ == nullptr ||
                nameLookup_ == nullptr ||
                (member->declaredTypeKey != "builtin:void" && !isClassLocalScalarType(member->declaredTypeKey)))
                return false;
            for (const MethodParameterSymbol& parameter : member->parameters)
                if (!isClassLocalScalarType(parameter.typeKey) || parameter.passKind != ParameterPassKind::Value ||
                    parameter.ownership != sema::OwnershipKind::None || parameter.isParameterPack)
                    return false;
            const auto scalar = [&](const ast::AstNode* syntax)
            {
                const MethodExpressionTypeBinding* type = syntax != nullptr ? expressionTypeBinding(syntax, ordinal) : nullptr;
                return type != nullptr && isClassLocalScalarType(type->typeKey);
            };
            const auto field = [&](const MethodNameBinding* binding)
            {
                return binding != nullptr && binding->candidateMemberOrdinals.size() == 1 &&
                    binding->candidateMemberOrdinals[0] < memberModel_->members().size() &&
                    isClassLocalScalarFieldCandidate(memberModel_->members()[binding->candidateMemberOrdinals[0]]) &&
                    !memberModel_->members()[binding->candidateMemberOrdinals[0]].isVolatile;
            };
            std::vector<const ast::AstNode*> pending{body};
            std::unordered_set<const ast::AstNode*> visited;
            std::unordered_set<const ast::AstNode*> permittedThis;
            std::unordered_set<const ast::AstNode*> permittedMethodGroups;
            bool usedReceiverField = false;
            while (!pending.empty())
            {
                const ast::AstNode* node = pending.back();
                pending.pop_back();
                if (node == nullptr || !visited.insert(node).second) continue;
                if (node->isReference() || node->auxiliaryText() == "unsafe-block") return false;
                switch (node->kind())
                {
                    case ast::AstNodeKind::BlockStatement:
                    case ast::AstNodeKind::IfStatement:
                    case ast::AstNodeKind::WhileStatement:
                    case ast::AstNodeKind::DoWhileStatement:
                    case ast::AstNodeKind::ForStatement:
                    case ast::AstNodeKind::SwitchStatement:
                    case ast::AstNodeKind::SwitchSection:
                    case ast::AstNodeKind::SwitchLabel:
                    case ast::AstNodeKind::BreakStatement:
                    case ast::AstNodeKind::ContinueStatement:
                    case ast::AstNodeKind::ExpressionStatement:
                        break;
                    case ast::AstNodeKind::VariableDeclarationStatement:
                        if (node->typeSyntax() == nullptr || node->hasModifier(lexer::TokenKind::KwStatic) ||
                            !isClassLocalScalarType(memberModel_->canonicalTypeKey(node->typeSyntax(), *nameLookup_)) ||
                            (node->first() != nullptr && !scalar(node->first()))) return false;
                        break;
                    case ast::AstNodeKind::ReturnStatement:
                        if (node->first() == nullptr ? member->declaredTypeKey != "builtin:void" :
                            (!scalar(node->first()) || expressionTypeBinding(node->first(), ordinal)->typeKey !=
                                member->declaredTypeKey)) return false;
                        break;
                    case ast::AstNodeKind::IdentifierExpression:
                    {
                        const MethodNameBinding* binding = nameBinding(node, ordinal);
                        if (binding != nullptr && (binding->kind == MethodNameBindingKind::ImplicitMethodGroup ||
                            binding->kind == MethodNameBindingKind::StaticMethodGroup) &&
                            permittedMethodGroups.contains(node)) break;
                        if (binding == nullptr || !scalar(node)) return false;
                        if (binding->kind == MethodNameBindingKind::ImplicitField)
                        {
                            if (!field(binding)) return false;
                            usedReceiverField = true;
                        }
                        else if (binding->kind == MethodNameBindingKind::Local)
                        {
                            if (!binding->localOrdinal.has_value() || *binding->localOrdinal >= locals_.size() ||
                                locals_[*binding->localOrdinal].isReference || locals_[*binding->localOrdinal].isStatic)
                                return false;
                        }
                        else if (binding->kind != MethodNameBindingKind::Parameter) return false;
                        break;
                    }
                    case ast::AstNodeKind::MemberExpression:
                    {
                        const MethodNameBinding* binding = nameBinding(node, ordinal);
                        if (binding != nullptr && binding->kind == MethodNameBindingKind::ExplicitThisMethodGroup &&
                            permittedMethodGroups.contains(node)) break;
                        if (binding == nullptr || binding->kind != MethodNameBindingKind::ExplicitThisField ||
                            node->first() == nullptr || node->first()->kind() != ast::AstNodeKind::ThisExpression ||
                            !field(binding) || !scalar(node)) return false;
                        permittedThis.insert(node->first());
                        usedReceiverField = true;
                        break;
                    }
                    case ast::AstNodeKind::ThisExpression:
                        if (!permittedThis.contains(node)) return false;
                        break;
                    case ast::AstNodeKind::IntegerLiteralExpression:
                    case ast::AstNodeKind::FloatingLiteralExpression:
                    case ast::AstNodeKind::BooleanLiteralExpression:
                    case ast::AstNodeKind::CharacterLiteralExpression:
                        if (!scalar(node)) return false;
                        break;
                    case ast::AstNodeKind::AssignmentExpression:
                    {
                        const MethodExpressionTypeBinding* left = node->first() != nullptr
                            ? expressionTypeBinding(node->first(), ordinal) : nullptr;
                        const MethodExpressionTypeBinding* right = node->second() != nullptr
                            ? expressionTypeBinding(node->second(), ordinal) : nullptr;
                        if (node->tokenKind() != lexer::TokenKind::Equal || callBinding(node, ordinal) != nullptr ||
                            left == nullptr || right == nullptr || !isClassLocalScalarType(left->typeKey) ||
                            left->typeKey != right->typeKey) return false;
                        break;
                    }
                    case ast::AstNodeKind::BinaryExpression:
                        if (!scalar(node) || !scalar(node->first()) || !scalar(node->second()) ||
                            callBinding(node, ordinal) != nullptr) return false;
                        break;
                    case ast::AstNodeKind::UnaryExpression:
                        if ((node->tokenKind() != lexer::TokenKind::Plus && node->tokenKind() != lexer::TokenKind::Minus &&
                             node->tokenKind() != lexer::TokenKind::Bang && node->tokenKind() != lexer::TokenKind::Tilde) ||
                            !scalar(node) || !scalar(node->first()) || callBinding(node, ordinal) != nullptr) return false;
                        break;
                    case ast::AstNodeKind::CallExpression:
                    {
                        const MethodCallBinding* call = callBinding(node, ordinal);
                        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved ||
                            call->interfaceCall || call->baseReceiver ||
                            !call->selectedMemberOrdinal.has_value() || !call->selectedDefaultArguments.empty() ||
                            call->selectedParameterPackOrdinal.has_value() || call->selectedUsesExpandedParameterPack)
                            return false;
                        const ClassMemberSymbol* target = methodSymbol(*call->selectedMemberOrdinal);
                        if (target == nullptr || target->kind != ClassMemberKind::Method ||
                            target->isStatic != call->staticCall ||
                            target->isPrecompiled || target->isUnsafe ||
                            (call->staticCall && (target->isExternC || target->isImportedC)) ||
                            target->isVirtual || target->isOverride ||
                            target->isAbstract || target->isReferenceReturn || classLocalCallableMaySuspend(*target) ||
                            !target->declaredErrorTypeKey.empty() ||
                            target->declaredOwnership != sema::OwnershipKind::None ||
                            (target->declaredTypeKey != "builtin:void" && !isClassLocalScalarType(target->declaredTypeKey)) ||
                            call->selectedArgumentSyntaxes.size() != target->parameters.size() ||
                            call->selectedArgumentParameterOrdinals.size() != target->parameters.size() ||
                            call->selectedConversions.size() != target->parameters.size()) return false;
                        std::vector<bool> seen(target->parameters.size(), false);
                        for (std::size_t i = 0; i < call->selectedArgumentSyntaxes.size(); ++i)
                        {
                            const std::size_t parameter = call->selectedArgumentParameterOrdinals[i];
                            const MethodExpressionTypeBinding* value = call->selectedArgumentSyntaxes[i] != nullptr
                                ? expressionTypeBinding(call->selectedArgumentSyntaxes[i], ordinal) : nullptr;
                            if (parameter >= target->parameters.size() || seen[parameter] || value == nullptr ||
                                !isClassLocalScalarType(value->typeKey) ||
                                target->parameters[parameter].typeKey != value->typeKey ||
                                target->parameters[parameter].passKind != ParameterPassKind::Value ||
                                target->parameters[parameter].ownership != sema::OwnershipKind::None ||
                                target->parameters[parameter].isParameterPack ||
                                call->selectedConversions[i].kind != ProjectConversionKind::Identity) return false;
                            seen[parameter] = true;
                        }
                        const ast::AstNode* group = node->first();
                        if (group == nullptr) return false;
                        const MethodNameBinding* binding = nameBinding(group, ordinal);
                        if (binding == nullptr) return false;
                        if (call->staticCall)
                        {
                            // Only an unqualified static call with scalar value arguments can
                            // be proved here; an explicit receiver could carry this object's address.
                            if (call->receiverSyntax != nullptr || call->implicitThis ||
                                group->kind() != ast::AstNodeKind::IdentifierExpression ||
                                binding->kind != MethodNameBindingKind::StaticMethodGroup) return false;
                            permittedMethodGroups.insert(group);
                            break;
                        }
                        if (call->receiverSyntax != nullptr)
                        {
                            if (call->receiverSyntax->kind() != ast::AstNodeKind::ThisExpression ||
                                group->kind() != ast::AstNodeKind::MemberExpression ||
                                group->first() != call->receiverSyntax ||
                                binding->kind != MethodNameBindingKind::ExplicitThisMethodGroup) return false;
                            permittedThis.insert(call->receiverSyntax);
                        }
                        else if (!call->implicitThis || group->kind() != ast::AstNodeKind::IdentifierExpression ||
                                 binding->kind != MethodNameBindingKind::ImplicitMethodGroup) return false;
                        permittedMethodGroups.insert(group);
                        // A returned receiver is a hazard even when the scalar
                        // result is discarded: a user conversion may publish it.
                        scalarReceiverEdges.push_back({ordinal, call->selectedMemberOrdinal, true, true, true, {}});
                        break;
                    }
                    default: return false;
                }
                pending.push_back(node->first());
                pending.push_back(node->second());
                pending.push_back(node->third());
                for (const ast::AstNode* child : node->children()) pending.push_back(child);
            }
            return usedReceiverField || !scalarReceiverEdges.empty();
        };
        if (scalarReceiverOperations())
        {
            summary.unknown = false;
            edges.insert(edges.end(), scalarReceiverEdges.begin(), scalarReceiverEdges.end());
            continue;
        }
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
                // EN: An owned child created in the receiver's own null owned slot. The construction
                //     model still rejects the type unless the child constructor is closed.
                // ES: Un hijo owned creado en la propia ranura owned nula del receptor. El modelo de
                //     construccion rechaza el tipo salvo que el constructor del hijo sea cerrado.
                if (classLocalOwnedChildAssignmentField(*this, *memberModel_, *member, ordinal, statement) !=
                    nullptr) continue;
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
