#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "Internal/NamedConstructorArgumentMap.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ClassLocalScalarField.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
bool ProjectConstructionModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_)
    {
        return !diagnostics.hasErrors();
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() ||
        !interfaceModel.isBuilt() || !methodBodyModel.isBuilt())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            SourceRange(),
            "project construction binding requires completed name, member, inheritance, interface and callable-body models");
        built_ = true;
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;
    methodBodyModel_ = &methodBodyModel;
    namespaceFunctionModel_ = &namespaceFunctionModel;
    statistics_ = {};
    initializerBindingByMember_.resize(memberModel.members().size());
    implicitDefaultBindingByType_.resize(nameLookup.namedTypes().size());
    readSnapshotConstruction();

    // A source class with no explicit constructor still has a semantic constructor obligation.
    // Validate those chains first so explicit derived constructors and object constructions can
    // safely reference a completed implicit-default model.
    bindImplicitDefaultConstructors(diagnostics);

    // Constructor chaining is a declaration-level semantic obligation. Bind it before scanning
    // object-creation expressions so an invalid derived constructor cannot hide behind the fact
    // that no `new Derived(...)` expression happens to occur in this project.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& constructor = memberModel.members()[memberOrdinal];
        // EN: Freeze 3.0 (item 130a): a constructor of a closed generic type that nothing selected
        //     was never analyzed; its initializer is not bound and cannot invalidate the type.
        // ES: Congelamiento 3.0 (elemento 130a): un constructor de un tipo generico cerrado que nada
        //     eligio nunca se analizo; su inicializador no se enlaza y no puede invalidar el tipo.
        if (constructor.kind == ClassMemberKind::Constructor &&
            !constructor.isStatic &&
            constructor.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
            !nameLookup.namedTypes()[constructor.declaringTypeOrdinal].isPrecompiled &&
            !methodBodyModel.constructorBodyDeferred(memberOrdinal))
        {
            bindConstructorInitializer(memberOrdinal, diagnostics);
        }
    }
    validateThisConstructorCycles(diagnostics);

    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit != nullptr && unit->syntaxTree() != nullptr)
        {
            scanUnsupportedContexts(unit->syntaxTree(), diagnostics);
        }
    }

    std::vector<const ProjectConstructionSite*> orderedSites;
    orderedSites.reserve(methodBodyModel.constructionSites().size());
    for (const ProjectConstructionSite& site : methodBodyModel.constructionSites())
    {
        orderedSites.push_back(&site);
    }
    std::ranges::sort(
        orderedSites,
        [](const ProjectConstructionSite* left, const ProjectConstructionSite* right)
        {
            if (left->callableOrdinal != right->callableOrdinal)
            {
                return left->callableOrdinal < right->callableOrdinal;
            }
            return left->sequenceWithinCallable < right->sequenceWithinCallable;
        });
    for (const ProjectConstructionSite* site : orderedSites)
    {
        ++statistics_.typedSitesConsumed;
        statistics_.typedArgumentsConsumed += site->argumentTypes.size();
        bindConstruction(*site, diagnostics);
    }
    if (!methodBodyModel.classReceiverEffectSummaries().empty())
    {
        const auto types = nameLookup.namedTypes();
        std::vector<bool> hasUnprovenFields(types.size(), false);
        const auto trivialFieldInitializerCall = [&](const ClassMemberSymbol& field,
                                                       std::size_t fieldOrdinal)
        {
            const ast::AstNode* syntax = field.syntax != nullptr ? field.syntax->first() : nullptr;
            if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::CallExpression) return false;
            const MethodCallBinding* call = methodBodyModel.callBinding(syntax, fieldOrdinal);
            if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved ||
                !call->staticCall || call->interfaceCall || !call->selectedMemberOrdinal.has_value() ||
                *call->selectedMemberOrdinal >= memberModel.members().size() ||
                !call->selectedArgumentSyntaxes.empty() || !call->selectedDefaultArguments.empty() ||
                call->selectedParameterPackOrdinal.has_value()) return false;
            const ClassMemberSymbol& target = memberModel.members()[*call->selectedMemberOrdinal];
            if (target.kind != ClassMemberKind::Method || !target.isStatic || target.isPrecompiled ||
                target.isExternC || target.isImportedC || target.isUnsafe || target.isAbstract ||
                target.isVirtual || target.isOverride || !target.parameters.empty() ||
                !target.declaredErrorTypeKey.empty() || target.effects.contains(sema::Effect::Throws) ||
                classLocalCallableMaySuspend(target) ||
                target.declaredTypeKey != field.declaredTypeKey || target.syntax == nullptr ||
                target.syntax->first() == nullptr ||
                target.syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
                target.syntax->first()->children().size() != 1 ||
                target.declaringTypeOrdinal >= types.size() || types[target.declaringTypeOrdinal].isPrecompiled ||
                types[target.declaringTypeOrdinal].parts.empty())
                return false;
            for (const auto& part : types[target.declaringTypeOrdinal].parts)
                if (part.isPrecompiled || part.syntax == nullptr) return false;
            for (const std::size_t ordinal : memberModel.membersOfType(target.declaringTypeOrdinal))
            {
                if (ordinal >= memberModel.members().size()) return false;
                const ClassMemberSymbol& member = memberModel.members()[ordinal];
                if (member.declaringTypeOrdinal != target.declaringTypeOrdinal || !member.isStatic) continue;
                if (member.kind == ClassMemberKind::Constructor ||
                    (member.kind == ClassMemberKind::Field && !member.isConst &&
                        (member.isPrecompiled || member.syntax == nullptr || member.syntax->first() != nullptr)))
                    return false;
            }
            const ast::AstNode* statement = target.syntax->first()->children().front();
            if (statement == nullptr || statement->kind() != ast::AstNodeKind::ReturnStatement ||
                statement->isReference() || target.isReferenceReturn ||
                !isClassLocalScalarLiteral(statement->first(), target.declaredTypeKey)) return false;
            const auto* returnType = methodBodyModel.expressionTypeBinding(statement->first(), *call->selectedMemberOrdinal);
            const auto* initializerType = methodBodyModel.expressionTypeBinding(syntax, fieldOrdinal);
            return returnType != nullptr && initializerType != nullptr &&
                returnType->typeKey == target.declaredTypeKey && initializerType->typeKey == field.declaredTypeKey;
        };
        for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
        {
            const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
            const auto fieldType = member.kind == ClassMemberKind::Field && !member.isStatic
                ? inheritanceModel.typeOrdinalForTypeKey(member.declaredTypeKey) : std::nullopt;
            const bool fieldIsClass = fieldType && inheritanceModel.isClass(*fieldType);
            if (member.declaringTypeOrdinal < hasUnprovenFields.size() && !member.isStatic &&
                (member.kind == ClassMemberKind::Event ||
                    (member.kind == ClassMemberKind::Field && !isClassLocalScalarFieldCandidate(member) &&
                     !isClassLocalEmptyNativeArrayField(member, ProjectTypeConversion::isNativeArray(member.declaredTypeKey)) &&
                     !isClassLocalEmptyClassReferenceField(member, fieldIsClass) &&
                     !isClassLocalNullOwnedClassField(member, fieldIsClass) &&
                     !isClassLocalNewOwnedClassField(member, fieldIsClass))))
                hasUnprovenFields[member.declaringTypeOrdinal] = true;
            // EN: An owned child created in the field declaration must be a closed construction of the field's
            //     type or a derived class (M1-06, doc 15).
            // ES: Un hijo owned creado en la declaracion del campo debe ser una construccion cerrada del tipo
            //     del campo o de una clase derivada (M1-06, doc 15).
            if (member.declaringTypeOrdinal < hasUnprovenFields.size() &&
                isClassLocalNewOwnedClassField(member, fieldIsClass))
            {
                const ObjectConstructionBinding* creation = nullptr;
                for (const ObjectConstructionBinding& candidate : constructions_)
                    if (candidate.syntax == member.syntax->first()) creation = &candidate;
                const bool closed = creation != nullptr && fieldType.has_value() &&
                    (creation->targetTypeOrdinal == *fieldType ||
                     inheritanceModel.isDerivedFrom(creation->targetTypeOrdinal, *fieldType)) &&
                    classLocalChildConstructionIsClosed(*creation);
                if (!closed) hasUnprovenFields[member.declaringTypeOrdinal] = true;
            }
            if (member.declaringTypeOrdinal < hasUnprovenFields.size() && !member.isStatic &&
                member.kind == ClassMemberKind::Field && isClassLocalScalarFieldCandidate(member) &&
                member.syntax->first() != nullptr &&
                member.syntax->first()->kind() == ast::AstNodeKind::CallExpression &&
                !trivialFieldInitializerCall(member, memberOrdinal))
                hasUnprovenFields[member.declaringTypeOrdinal] = true;
            if (member.declaringTypeOrdinal < hasUnprovenFields.size() && !member.isStatic &&
                member.kind == ClassMemberKind::Field && isClassLocalScalarFieldCandidate(member) &&
                member.syntax->first() != nullptr &&
                member.syntax->first()->kind() == ast::AstNodeKind::FloatingLiteralExpression)
            {
                const MethodExpressionTypeBinding* type =
                    methodBodyModel.expressionTypeBinding(member.syntax->first(), memberOrdinal);
                if (type == nullptr || type->typeKey != member.declaredTypeKey)
                    hasUnprovenFields[member.declaringTypeOrdinal] = true;
            }
            if (member.declaringTypeOrdinal >= hasUnprovenFields.size() || member.isStatic ||
                member.kind != ClassMemberKind::Field || !isClassLocalPlainScalarField(member) ||
                member.syntax->first() == nullptr ||
                isClassLocalScalarLiteral(member.syntax->first(), member.declaredTypeKey)) continue;
            std::vector<const ast::AstNode*> pending{member.syntax->first()};
            while (!pending.empty())
            {
                const ast::AstNode* expression = pending.back();
                pending.pop_back();
                const MethodExpressionTypeBinding* type = methodBodyModel.expressionTypeBinding(expression, memberOrdinal);
                if (type == nullptr || type->typeKey != member.declaredTypeKey)
                { hasUnprovenFields[member.declaringTypeOrdinal] = true; break; }
                if (expression->first() != nullptr) pending.push_back(expression->first());
                if (expression->second() != nullptr) pending.push_back(expression->second());
            }
        }
        std::vector<bool> unclosedChildConstructor(memberModel.members().size(), false);
        markUnprovenConstructorChildren(unclosedChildConstructor);
        for (const ObjectConstructionBinding& binding : constructions_)
        {
            bool initializationProvenSafe = false;
            const std::optional<std::size_t> base = inheritanceModel.baseTypeOrdinal(binding.targetTypeOrdinal);
            if (base.has_value() && binding.resolution == ConstructionResolutionKind::ResolvedExplicit &&
                binding.selectedConstructorMemberOrdinal.has_value() &&
                binding.selectedArgumentParameterOrdinals.empty() && binding.selectedDefaultArguments.empty())
            {
                std::unordered_set<std::size_t> visited;
                std::size_t currentType = binding.targetTypeOrdinal;
                std::size_t currentConstructor = *binding.selectedConstructorMemberOrdinal;
                bool safeChain = true;
                while (true)
                {
                    if (currentType >= types.size() || !visited.insert(currentType).second ||
                        types[currentType].kind != NamedTypeDeclarationKind::Class || hasUnprovenFields[currentType] ||
                        currentConstructor >= memberModel.members().size()) { safeChain = false; break; }
                    const ClassMemberSymbol& constructor = memberModel.members()[currentConstructor];
                    if (types[currentType].isPrecompiled)
                    {
                        // The producer proves only the fieldless root and its generated
                        // parameterless entry. Both records must be guarded for the exact unit.
                        bool hasStorage = false;
                        for (const std::size_t memberOrdinal : memberModel.membersOfType(currentType))
                        {
                            if (memberOrdinal >= memberModel.members().size()) { hasStorage = true; break; }
                            const auto& member = memberModel.members()[memberOrdinal];
                            if (!member.isStatic && (member.kind == ClassMemberKind::Field ||
                                member.kind == ClassMemberKind::Event)) { hasStorage = true; break; }
                        }
                        safeChain = !hasStorage && !inheritanceModel.baseTypeOrdinal(currentType).has_value() &&
                            hasGuardedReceiverConstructor(currentType) && hasGuardedReceiverDestructor(currentType) &&
                            constructor.isPrecompiled && constructor.kind == ClassMemberKind::Constructor &&
                            constructor.declaringTypeOrdinal == currentType && constructor.parameters.empty();
                        break;
                    }
                    const ClassReceiverEffects effect = methodBodyModel.classReceiverEffectSummary(currentConstructor);
                    const ConstructorInitializerBinding* initializer = constructorInitializerBindingForMember(currentConstructor);
                    if (constructor.declaringTypeOrdinal != currentType || constructor.isPrecompiled ||
                        !constructor.parameters.empty() || effect.unknown || effect.published || effect.returned ||
                        unclosedChildConstructor[currentConstructor] ||
                        initializer == nullptr || initializer->kind != ConstructorInitializerKind::ImplicitBase)
                    { safeChain = false; break; }
                    const auto nextBase = inheritanceModel.baseTypeOrdinal(currentType);
                    if (!nextBase.has_value())
                    {
                        safeChain = initializer->resolution == ConstructorInitializerResolutionKind::NoChainingRequired;
                        break;
                    }
                    if (initializer->resolution != ConstructorInitializerResolutionKind::ResolvedExplicit ||
                        initializer->targetTypeOrdinal != nextBase ||
                        !initializer->selectedConstructorMemberOrdinal.has_value() ||
                        initializer->selectedEntryConstructorMemberOrdinal != initializer->selectedConstructorMemberOrdinal ||
                        !initializer->selectedDefaultArguments.empty()) { safeChain = false; break; }
                    currentType = *nextBase;
                    currentConstructor = *initializer->selectedConstructorMemberOrdinal;
                }
                initializationProvenSafe = safeChain;
            }
            // EN: A chain of implicit default constructors up to a root without a base: every type in the chain
            //     is a source class with proven fields and no explicit constructor (M1-03/M1-07, doc 12).
            // ES: Una cadena de constructores implicitos por omision hasta una raiz sin base: todo tipo de la
            //     cadena es una clase fuente con campos demostrados y sin constructor explicito (doc 12).
            // EN: A guarded precompiled root (NativeSharp.Core.Object): fieldless, without base, with guarded
            //     receiver contracts and entered through its parameterless precompiled constructor.
            // ES: Una raiz precompilada protegida (NativeSharp.Core.Object): sin campos ni base, con contratos de
            //     receptor protegidos y entrada por su constructor precompilado sin parametros.
            const auto precompiledRootProven = [&](std::size_t rootType, std::size_t rootConstructor)
            {
                if (rootType >= types.size() || !types[rootType].isPrecompiled ||
                    types[rootType].kind != NamedTypeDeclarationKind::Class ||
                    inheritanceModel.baseTypeOrdinal(rootType).has_value() ||
                    !hasGuardedReceiverConstructor(rootType) || !hasGuardedReceiverDestructor(rootType) ||
                    rootConstructor >= memberModel.members().size()) return false;
                for (const std::size_t memberOrdinal : memberModel.membersOfType(rootType))
                {
                    if (memberOrdinal >= memberModel.members().size()) return false;
                    const auto& member = memberModel.members()[memberOrdinal];
                    if (!member.isStatic && (member.kind == ClassMemberKind::Field ||
                        member.kind == ClassMemberKind::Event)) return false;
                }
                const ClassMemberSymbol& constructor = memberModel.members()[rootConstructor];
                return constructor.isPrecompiled && constructor.kind == ClassMemberKind::Constructor &&
                    constructor.declaringTypeOrdinal == rootType && constructor.parameters.empty();
            };
            const auto implicitDefaultChainProven = [&](std::size_t type)
            {
                std::optional<std::size_t> chainType = type;
                for (std::size_t depth = 0; chainType.has_value() && depth < 64; ++depth)
                {
                    const ImplicitDefaultConstructorBinding* initializer =
                        implicitDefaultConstructorBindingForType(*chainType);
                    if (initializer != nullptr && *chainType < hasUnprovenFields.size() &&
                        *chainType < types.size() && !types[*chainType].isPrecompiled &&
                        types[*chainType].kind == NamedTypeDeclarationKind::Class && !hasUnprovenFields[*chainType] &&
                        initializer->resolution ==
                            ImplicitDefaultConstructorResolutionKind::ResolvedExplicitBaseEntry &&
                        initializer->baseTypeOrdinal.has_value() &&
                        initializer->directBaseConstructorMemberOrdinal.has_value() &&
                        initializer->entryConstructorMemberOrdinal == initializer->directBaseConstructorMemberOrdinal)
                        return precompiledRootProven(*initializer->baseTypeOrdinal,
                            *initializer->directBaseConstructorMemberOrdinal);
                    if (initializer == nullptr || *chainType >= hasUnprovenFields.size() ||
                        *chainType >= types.size() || types[*chainType].isPrecompiled ||
                        types[*chainType].kind != NamedTypeDeclarationKind::Class || hasUnprovenFields[*chainType] ||
                        initializer->directBaseConstructorMemberOrdinal.has_value() ||
                        (initializer->entryConstructorMemberOrdinal.has_value() &&
                         initializer->resolution !=
                             ImplicitDefaultConstructorResolutionKind::ResolvedImplicitBaseChain)) return false;
                    if (initializer->resolution == ImplicitDefaultConstructorResolutionKind::NoBaseRequired &&
                        !initializer->baseTypeOrdinal.has_value()) return true;
                    if (initializer->resolution !=
                        ImplicitDefaultConstructorResolutionKind::ResolvedImplicitBaseChain) return false;
                    chainType = initializer->baseTypeOrdinal;
                }
                return false;
            };
            if (!initializationProvenSafe && binding.targetTypeOrdinal < types.size() &&
                inheritanceModel.baseTypeOrdinal(binding.targetTypeOrdinal).has_value() &&
                binding.resolution == ConstructionResolutionKind::ResolvedImplicitDefault &&
                binding.selectedImplicitDefault)
                initializationProvenSafe = implicitDefaultChainProven(binding.targetTypeOrdinal);
            if (binding.targetTypeOrdinal < types.size() &&
                !types[binding.targetTypeOrdinal].isPrecompiled &&
                types[binding.targetTypeOrdinal].kind == NamedTypeDeclarationKind::Class &&
                !inheritanceModel.baseTypeOrdinal(binding.targetTypeOrdinal).has_value())
            {
                if (binding.resolution == ConstructionResolutionKind::ResolvedExplicit &&
                    binding.selectedConstructorMemberOrdinal.has_value() &&
                    *binding.selectedConstructorMemberOrdinal < memberModel.members().size())
                {
                    const ClassMemberSymbol& constructor =
                        memberModel.members()[*binding.selectedConstructorMemberOrdinal];
                    const ConstructorInitializerBinding* initializer =
                        constructorInitializerBindingForMember(*binding.selectedConstructorMemberOrdinal);
                    bool defaultsSafe = binding.selectedArgumentParameterOrdinals.empty() &&
                        binding.selectedDefaultArguments.size() == constructor.parameters.size() &&
                        binding.selectedDefaultArgumentParameterOrdinals.size() == constructor.parameters.size();
                    std::vector<bool> seen(constructor.parameters.size(), false);
                    for (std::size_t i = 0; defaultsSafe && i < binding.selectedDefaultArguments.size(); ++i)
                    {
                        const std::size_t parameterOrdinal = binding.selectedDefaultArgumentParameterOrdinals[i];
                        if (parameterOrdinal >= constructor.parameters.size() || seen[parameterOrdinal])
                        { defaultsSafe = false; break; }
                        seen[parameterOrdinal] = true;
                        const MethodParameterSymbol& parameter = constructor.parameters[parameterOrdinal];
                        if ((parameter.typeKey != "builtin:int" && parameter.typeKey != "builtin:bool") ||
                            parameter.passKind != ParameterPassKind::Value ||
                            parameter.ownership != sema::OwnershipKind::None || parameter.isParameterPack ||
                            !isClassLocalScalarLiteral(binding.selectedDefaultArguments[i], parameter.typeKey))
                            defaultsSafe = false;
                        if (defaultsSafe && parameter.typeKey == "builtin:int")
                        {
                            const auto value = ProjectIntegerEvaluator(nullptr).evaluate(binding.selectedDefaultArguments[i]);
                            if (!value.has_value() || value->typeKey != parameter.typeKey) defaultsSafe = false;
                        }
                    }
                    initializationProvenSafe = !hasUnprovenFields[binding.targetTypeOrdinal] &&
                        !unclosedChildConstructor[*binding.selectedConstructorMemberOrdinal] &&
                        !constructor.isPrecompiled && defaultsSafe && initializer != nullptr &&
                        initializer->resolution == ConstructorInitializerResolutionKind::NoChainingRequired;
                }
                else if (binding.resolution == ConstructionResolutionKind::ResolvedImplicitDefault &&
                         binding.selectedImplicitDefault && !hasUnprovenFields[binding.targetTypeOrdinal] &&
                         !binding.implicitDefaultBaseConstructorMemberOrdinal.has_value())
                {
                    initializationProvenSafe = implicitDefaultChainProven(binding.targetTypeOrdinal);
                }
            }
            classConstructorReceiverAssessments_.push_back(assessClassConstructorReceiver(
                binding, methodBodyModel.classReceiverEffectSummaries(), initializationProvenSafe));
        }
    }
    if (!methodBodyModel.classReceiverEffectSummaries().empty())
    {
        for (std::size_t i = 0; i < constructions_.size(); ++i)
            indexClassLocalConstruction(i);
        const auto types = nameLookup.namedTypes();
        std::vector<bool> unsafeDestructor(types.size(), false);
        for (std::size_t ordinal = 0; ordinal < memberModel.members().size(); ++ordinal)
        {
            const ClassMemberSymbol& member = memberModel.members()[ordinal];
            if (member.kind != ClassMemberKind::Destructor || member.declaringTypeOrdinal >= types.size()) continue;
            const ClassReceiverEffects effect = methodBodyModel.classReceiverEffectSummary(ordinal);
            if (member.isPrecompiled || member.isVirtual || member.isOverride || member.isAbstract || member.isUnsafe ||
                !member.declaredErrorTypeKey.empty() || classLocalCallableMaySuspend(member) ||
                effect.unknown || effect.published || effect.returned)
                unsafeDestructor[member.declaringTypeOrdinal] = true;
        }
        // Every type is visited once. Cycles, missing bases and precompiled
        // entries fail closed; resolved parents are reused by later types.
        std::vector<unsigned char> state(types.size(), 0);
        classLocalDestructionChainSafe_.assign(types.size(), false);
        for (std::size_t start = 0; start < types.size(); ++start)
        {
            if (state[start] == 2) continue;
            std::vector<std::size_t> path;
            std::optional<std::size_t> current = start;
            bool result = true;
            while (current.has_value())
            {
                const std::size_t type = *current;
                if (type >= types.size() || state[type] == 1) { result = false; break; }
                if (state[type] == 2) { result = classLocalDestructionChainSafe_[type]; break; }
                state[type] = 1;
                path.push_back(type);
                if (types[type].isPrecompiled)
                {
                    bool hasStorage = false;
                    for (const std::size_t memberOrdinal : memberModel.membersOfType(type))
                    {
                        if (memberOrdinal >= memberModel.members().size()) { hasStorage = true; break; }
                        const auto& member = memberModel.members()[memberOrdinal];
                        if (!member.isStatic && (member.kind == ClassMemberKind::Field ||
                            member.kind == ClassMemberKind::Event)) { hasStorage = true; break; }
                    }
                    result = types[type].kind == NamedTypeDeclarationKind::Class && !hasStorage &&
                        !inheritanceModel.baseTypeOrdinal(type).has_value() &&
                        hasGuardedReceiverConstructor(type) && hasGuardedReceiverDestructor(type);
                    break;
                }
                if (unsafeDestructor[type]) { result = false; break; }
                current = inheritanceModel.baseTypeOrdinal(type);
            }
            for (auto i = path.rbegin(); i != path.rend(); ++i)
            {
                classLocalDestructionChainSafe_[*i] = result;
                state[*i] = 2;
            }
        }
    }
    std::unordered_map<std::size_t, std::vector<std::size_t>> implicitClassLocals;
    std::unordered_set<const ast::AstNode*> implicitClassSyntaxes;
    for (const ObjectConstructionBinding& binding : constructions_)
        if (!binding.targetIsStruct && binding.syntax != nullptr &&
            binding.syntax->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            binding.syntax->first() == nullptr)
            implicitClassSyntaxes.insert(binding.syntax);
    const auto localSymbols = methodBodyModel.locals();
    for (std::size_t i = 0; i < localSymbols.size(); ++i)
    {
        const MethodLocalSymbol& local = localSymbols[i];
        if (local.isScope && local.syntax != nullptr && implicitClassSyntaxes.contains(local.syntax))
            implicitClassLocals[local.methodMemberOrdinal].push_back(i);
    }
    std::vector<std::optional<bool>> classLocalSafety;
    if (!implicitClassLocals.empty()) classLocalSafety.resize(localSymbols.size());
    for (const auto& [callable, ordinals] : implicitClassLocals)
    {
        (void)callable;
        const std::vector<bool> safe = classLocalConstructionIsProvenSafe(ordinals);
        for (std::size_t i = 0; i < ordinals.size(); ++i)
        {
            classLocalSafety[ordinals[i]] = safe[i];
            if (!safe[i]) diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, localSymbols[ordinals[i]].range,
                "local class construction or destruction is not proven safe");
        }
    }
    classLocalSafetyByOrdinal_ = std::move(classLocalSafety);
    reportUndemandedConstructors(diagnostics);

    built_ = true;
    return !diagnostics.hasErrors();
}

// EN: A local class may create an owned child in a constructor (see
//     classLocalOwnedChildAssignmentField). The receiver-effect proof only shows that the receiver is
//     not observable by the child; this pass marks an instance constructor whose body contains any
//     object construction that is not closed. Only the constructors a local object actually runs are
//     consulted, so an unrelated overload keeps its previous behavior. Fails closed: a struct or an
//     unexpected binding marks the constructor.
// ES: Una clase local puede crear un hijo owned en un constructor (ver
//     classLocalOwnedChildAssignmentField). La prueba de efectos del receptor solo muestra que el hijo
//     no puede observar al receptor; esta pasada marca un constructor de instancia cuyo cuerpo
//     contiene alguna construccion no cerrada. Solo se consultan los constructores que un objeto
//     local ejecuta, asi una sobrecarga ajena conserva su comportamiento. Falla cerrado: un struct o
//     un binding inesperado marcan el constructor.
// Complexity: O(constructions * base depth). Effects: only sets entries of unclosedChildConstructor.
void ProjectConstructionModel::markUnprovenConstructorChildren(std::vector<bool>& unclosedChildConstructor) const
{
    for (const ObjectConstructionBinding& binding : constructions_)
    {
        if (binding.callerMemberOrdinal >= memberModel_->members().size() ||
            binding.callerMemberOrdinal >= unclosedChildConstructor.size()) continue;
        const ClassMemberSymbol& caller = memberModel_->members()[binding.callerMemberOrdinal];
        if (caller.kind != ClassMemberKind::Constructor || caller.isStatic) continue;
        if (binding.syntax == nullptr || binding.syntax->kind() != ast::AstNodeKind::ObjectCreationExpression)
            continue;
        if (!classLocalChildConstructionIsClosed(binding))
            unclosedChildConstructor[binding.callerMemberOrdinal] = true;
    }
}

// EN: A closed child construction is a resolved, argument-free `new` of a class with the default
//     allocator, whose selected constructor can neither throw nor suspend, and whose destructor
//     chain can neither throw nor suspend either (it runs when the local object is cleaned up).
// ES: Una construccion de hijo cerrada es un `new` resuelto y sin argumentos de una clase con el
//     asignador predeterminado, cuyo constructor elegido no puede lanzar ni suspenderse, y cuya
//     cadena de destructores tampoco puede lanzar ni suspenderse (se ejecuta al limpiar el objeto
//     local).
// Complexity: O(base depth * members of each type). Effects: none.
bool ProjectConstructionModel::classLocalChildConstructionIsClosed(const ObjectConstructionBinding& binding) const
{
    // EN: Arguments may only be closed scalars with built-in conversions (M1-06, docs 14 and 16).
    // ES: Los argumentos solo pueden ser escalares cerrados con conversiones integradas (M1-06, docs 14 y 16).
    if (binding.syntax == nullptr || methodBodyModel_ == nullptr) return false;
    for (const ast::AstNode* argument : binding.syntax->children())
        if (!methodBodyModel_->classLocalClosedScalarArgument(argument, binding.callerMemberOrdinal)) return false;
    for (const ProjectImplicitConversion& conversion : binding.selectedConversions)
        if (conversion.kind != ProjectConversionKind::Identity &&
            conversion.kind != ProjectConversionKind::IntegralPromotion &&
            conversion.kind != ProjectConversionKind::Numeric &&
            conversion.kind != ProjectConversionKind::ConstantInteger) return false;
    if (binding.targetIsStruct || binding.allocatorSelection ||
        (binding.resolution != ConstructionResolutionKind::ResolvedExplicit &&
         binding.resolution != ConstructionResolutionKind::ResolvedImplicitDefault) ||
        binding.selectedArgumentParameterOrdinals.size() != binding.syntax->children().size() ||
        !binding.selectedDefaultArguments.empty() || binding.selectedParameterPackOrdinal.has_value() ||
        binding.selectedUsesExpandedParameterPack || binding.selectedSynthesizesEmptyParameterPack ||
        !binding.selectedOwnershipTransfers.empty() || !binding.selectedFullExpressionBorrowArgumentOrdinals.empty())
        return false;
    const std::optional<std::size_t> entry = binding.selectedConstructorMemberOrdinal.has_value()
        ? binding.selectedConstructorMemberOrdinal : binding.implicitDefaultBaseConstructorMemberOrdinal;
    if (entry.has_value())
    {
        if (*entry >= memberModel_->members().size()) return false;
        const ClassMemberSymbol& constructor = memberModel_->members()[*entry];
        if (!constructor.declaredErrorTypeKey.empty() || constructor.effects.contains(sema::Effect::Throws) ||
            classLocalCallableMaySuspend(constructor)) return false;
    }
    std::unordered_set<std::size_t> visited;
    std::optional<std::size_t> type = binding.targetTypeOrdinal;
    while (type.has_value())
    {
        if (*type >= nameLookup_->namedTypes().size() || !visited.insert(*type).second ||
            nameLookup_->namedTypes()[*type].kind != NamedTypeDeclarationKind::Class) return false;
        for (const std::size_t ordinal : memberModel_->membersOfType(*type))
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const ClassMemberSymbol& member = memberModel_->members()[ordinal];
            if (member.kind == ClassMemberKind::Destructor && member.declaringTypeOrdinal == *type &&
                (!member.declaredErrorTypeKey.empty() || member.effects.contains(sema::Effect::Throws) ||
                 classLocalCallableMaySuspend(member))) return false;
        }
        type = inheritanceModel_->baseTypeOrdinal(*type);
    }
    return true;
}

// EN: Freeze 3.0 (item 130a). Every constructor a binding selects must have been demanded while the
//     bodies were analyzed; one that was not would reach HIR without bindings. This is an internal
//     consistency check, not a language rule.
// ES: Congelamiento 3.0 (elemento 130a). Todo constructor que elige un binding debio demandarse mientras
//     se analizaban los cuerpos; uno que no llegaria a HIR sin bindings. Es un chequeo interno de
//     consistencia, no una regla del lenguaje.
void ProjectConstructionModel::reportUndemandedConstructors(diagnostics::DiagnosticEngine& diagnostics) const
{
    std::vector<std::pair<std::size_t, const ast::AstNode*>> selected;
    for (const ObjectConstructionBinding& construction : constructions_)
    {
        if (construction.selectedConstructorMemberOrdinal.has_value())
            selected.emplace_back(*construction.selectedConstructorMemberOrdinal, construction.syntax);
    }
    for (const ConstructorInitializerBinding& initializer : constructorInitializerBindings_)
    {
        if (initializer.selectedConstructorMemberOrdinal.has_value())
            selected.emplace_back(
                *initializer.selectedConstructorMemberOrdinal, initializer.initializerSyntax);
    }
    for (const auto& [constructorOrdinal, syntax] : selected)
    {
        if (!methodBodyModel_->constructorBodyDeferred(constructorOrdinal)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange(),
            "internal: constructor '" + memberModel_->members()[constructorOrdinal].name +
                "' of a closed generic type was selected but its body was never analyzed");
    }
}

std::span<const ObjectConstructionBinding> ProjectConstructionModel::constructions() const noexcept
{
    return constructions_;
}

const ObjectConstructionBinding* ProjectConstructionModel::uniqueClassLocalConstruction(
    const ast::AstNode* syntax, std::size_t callableOrdinal) const noexcept
{
    const auto ordinal = classLocalConstructionIndex_.latest(syntax, callableOrdinal);
    return ordinal.has_value() && *ordinal < constructions_.size() ? &constructions_[*ordinal] : nullptr;
}

void ProjectConstructionModel::indexClassLocalConstruction(std::size_t bindingOrdinal)
{
    if (bindingOrdinal >= constructions_.size()) return;
    const ObjectConstructionBinding& binding = constructions_[bindingOrdinal];
    const auto previous = classLocalConstructionIndex_.latest(binding.syntax, binding.callerMemberOrdinal);
    // Ambiguous repeated bindings must not become a safety proof.
    classLocalConstructionIndex_.record(binding.callerMemberOrdinal, binding.syntax,
        previous.has_value() ? constructions_.size() : bindingOrdinal);
}

void ProjectConstructionModel::setVirtualDispatchModel(const ProjectVirtualDispatchModel* model) noexcept
{
    if (virtualDispatchModel_ != model) classLocalSafetyByOrdinal_.clear();
    virtualDispatchModel_ = model;
}

std::span<const ClassConstructorReceiverAssessment>
ProjectConstructionModel::classConstructorReceiverAssessments() const noexcept
{
    return classConstructorReceiverAssessments_;
}

std::vector<bool> ProjectConstructionModel::classLocalConstructionIsProvenSafe(
    std::span<const std::size_t> localOrdinals) const
{
    std::vector<bool> rejected(localOrdinals.size(), false);
    if (methodBodyModel_ == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        classConstructorReceiverAssessments_.size() != constructions_.size() ||
        methodBodyModel_->classReceiverEffectSummaries().empty()) return rejected;
    bool allCached = !localOrdinals.empty();
    const auto cachedLocals = methodBodyModel_->locals();
    std::unordered_set<std::size_t> cachedOrdinals;
    const std::size_t cachedCallable = allCached && localOrdinals[0] < cachedLocals.size()
        ? cachedLocals[localOrdinals[0]].methodMemberOrdinal : std::size_t(-1);
    for (const std::size_t ordinal : localOrdinals)
        if (ordinal >= classLocalSafetyByOrdinal_.size() || ordinal >= cachedLocals.size() ||
            !classLocalSafetyByOrdinal_[ordinal].has_value() ||
            cachedLocals[ordinal].methodMemberOrdinal != cachedCallable || !cachedOrdinals.insert(ordinal).second)
            allCached = false;
    if (allCached)
    {
        std::vector<bool> result;
        result.reserve(localOrdinals.size());
        for (const std::size_t ordinal : localOrdinals) result.push_back(*classLocalSafetyByOrdinal_[ordinal]);
        return result;
    }
    std::vector<bool> safe = methodBodyModel_->classLocalUsesAreProvenSafe(localOrdinals, virtualDispatchModel_, false, this);
    const auto locals = methodBodyModel_->locals();
    for (std::size_t i = 0; i < localOrdinals.size(); ++i)
    {
        if (!safe[i] || localOrdinals[i] >= locals.size()) { safe[i] = false; continue; }
        const MethodLocalSymbol& local = locals[localOrdinals[i]];
        if (local.syntax == nullptr) { safe[i] = false; continue; }
        const ast::AstNode* syntax = local.syntax->first() != nullptr ? local.syntax->first() : local.syntax;
        const auto found = classLocalConstructionIndex_.latest(syntax, local.methodMemberOrdinal);
        if (!found.has_value() || *found >= constructions_.size()) { safe[i] = false; continue; }
        const ObjectConstructionBinding& binding = constructions_[*found];
        safe[i] = binding.callerMemberOrdinal == local.methodMemberOrdinal &&
            binding.targetTypeOrdinal < classLocalDestructionChainSafe_.size() &&
            classLocalDestructionChainSafe_[binding.targetTypeOrdinal] &&
            classConstructorReceiverAssessments_[*found] == ClassConstructorReceiverAssessment::NoEscape;
    }
    return safe;
}

std::span<const ConstructorInitializerBinding> ProjectConstructionModel::constructorInitializerBindings() const noexcept
{
    return constructorInitializerBindings_;
}

const ConstructorInitializerBinding* ProjectConstructionModel::constructorInitializerBindingForMember(
    std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= initializerBindingByMember_.size() || !initializerBindingByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *initializerBindingByMember_[memberOrdinal];
    return index < constructorInitializerBindings_.size() ? &constructorInitializerBindings_[index] : nullptr;
}

std::span<const ImplicitDefaultConstructorBinding>
ProjectConstructionModel::implicitDefaultConstructorBindings() const noexcept
{
    return implicitDefaultConstructorBindings_;
}

const ImplicitDefaultConstructorBinding*
ProjectConstructionModel::implicitDefaultConstructorBindingForType(
    std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= implicitDefaultBindingByType_.size() ||
        !implicitDefaultBindingByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *implicitDefaultBindingByType_[typeOrdinal];
    return index < implicitDefaultConstructorBindings_.size()
        ? &implicitDefaultConstructorBindings_[index]
        : nullptr;
}

const ProjectConstructionStatistics& ProjectConstructionModel::statistics() const noexcept
{
    return statistics_;
}

bool ProjectConstructionModel::isBuilt() const noexcept
{
    return built_;
}

void ProjectConstructionModel::scanUnsupportedContexts(
    const ast::AstNode* node,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<const ast::AstNode*> work;
    if (node != nullptr)
    {
        work.push_back(node);
    }
    while (!work.empty())
    {
        const ast::AstNode* current = work.back();
        work.pop_back();
        if (current == nullptr)
        {
            continue;
        }
        ++statistics_.unsupportedContextNodesVisited;
        if (current->kind() == ast::AstNodeKind::MethodDeclaration ||
            current->kind() == ast::AstNodeKind::ConstructorDeclaration ||
            current->kind() == ast::AstNodeKind::FieldDeclaration ||
            current->kind() == ast::AstNodeKind::FunctionDeclaration)
        {
            continue;
        }
        if (current->kind() == ast::AstNodeKind::ObjectCreationExpression ||
            current->kind() == ast::AstNodeKind::PlacementConstructionExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                current->range(),
                "object construction is not valid in this declaration context");
            continue;
        }

        const std::span<ast::AstNode* const> children = current->children();
        for (auto child = children.rbegin(); child != children.rend(); ++child)
        {
            work.push_back(*child);
        }
        work.push_back(current->third());
        work.push_back(current->second());
        work.push_back(current->first());
    }
}

const char* constructionResolutionKindName(ConstructionResolutionKind kind) noexcept
{
    switch (kind)
    {
        case ConstructionResolutionKind::ResolvedExplicit: return "resolved-explicit";
        case ConstructionResolutionKind::ResolvedImplicitDefault: return "resolved-implicit-default";
        case ConstructionResolutionKind::NoViableCandidate: return "no-viable-candidate";
        case ConstructionResolutionKind::Ambiguous: return "ambiguous";
        case ConstructionResolutionKind::Inaccessible: return "inaccessible";
        case ConstructionResolutionKind::InvalidTarget: return "invalid-target";
    }
    return "invalid-target";
}

const char* constructorInitializerKindName(ConstructorInitializerKind kind) noexcept
{
    switch (kind)
    {
        case ConstructorInitializerKind::ImplicitBase: return "implicit-base";
        case ConstructorInitializerKind::ExplicitBase: return "explicit-base";
        case ConstructorInitializerKind::ExplicitThis: return "explicit-this";
    }
    return "unknown";
}

const char* constructorInitializerResolutionKindName(ConstructorInitializerResolutionKind kind) noexcept
{
    switch (kind)
    {
        case ConstructorInitializerResolutionKind::NoChainingRequired: return "no-chaining-required";
        case ConstructorInitializerResolutionKind::ResolvedExplicit: return "resolved-explicit";
        case ConstructorInitializerResolutionKind::ResolvedImplicitDefault: return "resolved-implicit-default";
        case ConstructorInitializerResolutionKind::NoViableCandidate: return "no-viable-candidate";
        case ConstructorInitializerResolutionKind::Ambiguous: return "ambiguous";
        case ConstructorInitializerResolutionKind::Inaccessible: return "inaccessible";
        case ConstructorInitializerResolutionKind::Invalid: return "invalid";
    }
    return "invalid";
}
const char* implicitDefaultConstructorResolutionKindName(
    ImplicitDefaultConstructorResolutionKind kind) noexcept
{
    switch (kind)
    {
        case ImplicitDefaultConstructorResolutionKind::NoBaseRequired: return "no-base-required";
        case ImplicitDefaultConstructorResolutionKind::ResolvedExplicitBaseEntry: return "resolved-explicit-base-entry";
        case ImplicitDefaultConstructorResolutionKind::ResolvedImplicitBaseChain: return "resolved-implicit-base-chain";
        case ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor: return "no-viable-base-constructor";
        case ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor: return "inaccessible-base-constructor";
        case ImplicitDefaultConstructorResolutionKind::Invalid: return "invalid";
    }
    return "invalid";
}

}
