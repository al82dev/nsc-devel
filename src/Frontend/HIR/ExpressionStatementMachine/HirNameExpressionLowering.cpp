#include "../HirLoweringSupport.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

namespace nativesharp::frontend
{
// Baja los nombres core conservando diagnósticos, orden y addressability.
// Lowers core names while preserving diagnostics, order, and addressability.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCoreNameExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodCoreBinding& core,
    [[maybe_unused]] std::string& error)
{
    if (core.kind == MethodCoreOperationKind::FixedArrayLength)
    {
        if (addressContext)
        {
            error = "NativeSharp 3.0 fixed-array Length is a constant value and is not addressable";
            return nullptr;
        }
        const std::optional<std::uint64_t> count =
            ProjectMemberModel::fixedArrayCount(core.elementTypeKey);
        if (!count.has_value())
        {
            error = "NativeSharp 3.0 fixed-array Length lost its canonical extent";
            return nullptr;
        }
        auto node = makeNode(
            ProjectOopHirNodeKind::IntegerConstant, expression, core.resultTypeKey);
        node->integerValue = *count;
        return node;
    }
    if (core.kind == MethodCoreOperationKind::ArrayLength ||
        core.kind == MethodCoreOperationKind::StringLength)
    {
        if (addressContext)
        {
            error = "Stage 41 Length is an intrinsic value and is not addressable";
            return nullptr;
        }
        auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (receiver == nullptr) return nullptr;
        auto node = makeNode(
            core.kind == MethodCoreOperationKind::ArrayLength
                ? ProjectOopHirNodeKind::ArrayLength
                : ProjectOopHirNodeKind::StringLength,
            expression,
            core.resultTypeKey);
        node->elementTypeKey = core.elementTypeKey;
        node->children.push_back(std::move(receiver));
        return node;
    }
    // NS304-T06-F05: a `ref:`/`refmut:` item Current IS a reference into the frame, so
    // taking it in address context is allowed (it yields the reference, not a value).
    const bool itemByReference = core.resultTypeKey.starts_with("ref:") ||
        core.resultTypeKey.starts_with("refmut:");
    if (addressContext && !itemByReference)
    {
        error = "iterator Current is an intrinsic value and is not addressable";
        return nullptr;
    }
    auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (receiver == nullptr) return nullptr;
    auto node = makeNode(
        ProjectOopHirNodeKind::IteratorCurrent, expression, core.resultTypeKey);
    node->elementTypeKey = core.elementTypeKey;
    node->children.push_back(std::move(receiver));
    return node;
}

// Baja un nombre ligado al lambda activo sin cambiar su representación.
// Lowers a name bound to the active lambda without changing its representation.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerLambdaNameExpression(
    const ast::AstNode* expression,
    bool addressContext)
{
    const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(expression);
    if (lambdaValue->lambdaParameter)
    {
        if (lambdaValue->lambdaParameterByReference)
        {
            const std::string pointerType = lambdaValue->typeKey + "*";
            auto pointer = makeNode(
                ProjectOopHirNodeKind::ParameterValue, expression, pointerType);
            pointer->parameterOrdinal = lambdaValue->valueOrdinal;
            if (addressContext) return pointer;
            auto value = makeNode(
                ProjectOopHirNodeKind::PointerLoad, expression, lambdaValue->typeKey);
            value->children.push_back(std::move(pointer));
            return value;
        }
        auto node = makeNode(
            addressContext
                ? ProjectOopHirNodeKind::ParameterAddress
                : ProjectOopHirNodeKind::ParameterValue,
            expression,
            lambdaValue->typeKey);
        node->parameterOrdinal = lambdaValue->valueOrdinal;
        return node;
    }
    auto node = makeNode(
        addressContext
            ? ProjectOopHirNodeKind::ClosureCaptureAddress
            : ProjectOopHirNodeKind::ClosureCaptureValue,
        expression,
        lambdaValue->typeKey);
    node->integerValue = lambdaValue->valueOrdinal;
    return node;
}

// Baja un parámetro, incluida la convención física de parámetros por referencia.
// Lowers a parameter, including the physical by-reference parameter convention.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerParameterNameBinding(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodNameBinding& binding)
{
    // C4 "language references": a by-reference parameter physically receives an
    // ADDRESS, and every use of it means the value on the other side. So the
    // parameter slot itself is the pointer:
    //
    //   value context   -> load the pointer, then load through it (PointerLoad)
    //   address context -> the pointer IS the address; hand it over untouched
    //
    // which is exactly how `*p` already behaves for an explicit pointer parameter.
    // Nothing new reaches MIR or the backend: `ref`/`out`/`in` are rules on top of
    // machinery C1 already certified, not a second calling convention.
    const MethodParameterSymbol* parameter = nullptr;
    if (binding.parameterOrdinal.has_value() && memberModel_ != nullptr)
    {
        if (binding.methodMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& owner =
                memberModel_->members()[binding.methodMemberOrdinal];
            if (*binding.parameterOrdinal < owner.parameters.size())
                parameter = &owner.parameters[*binding.parameterOrdinal];
        }
        else if (namespaceFunctionModel_ != nullptr)
        {
            const std::size_t functionOrdinal =
                binding.methodMemberOrdinal - memberModel_->members().size();
            if (functionOrdinal < namespaceFunctionModel_->functions().size())
            {
                const NamespaceFunctionSymbol& owner =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                if (*binding.parameterOrdinal < owner.parameters.size())
                    parameter = &owner.parameters[*binding.parameterOrdinal];
            }
        }
    }
    if (parameter != nullptr && parameter->passKind != ParameterPassKind::Value)
    {
        auto pointer = makeNode(
            ProjectOopHirNodeKind::ParameterValue, expression, parameter->typeKey);
        pointer->parameterOrdinal = binding.parameterOrdinal;
        if (addressContext) return pointer;
        auto value = makeNode(
            ProjectOopHirNodeKind::PointerLoad, expression, parameter->pointeeTypeKey);
        value->children.push_back(std::move(pointer));
        return value;
    }
    auto node = makeNode(
        addressContext
            ? ProjectOopHirNodeKind::ParameterAddress
            : ProjectOopHirNodeKind::ParameterValue,
        expression,
        binding.typeKey);
    node->parameterOrdinal = binding.parameterOrdinal;
    return node;
}

// Baja un local normal o por referencia y conserva volatile.
// Lowers a normal or reference local and preserves volatile access.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerLocalNameBinding(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodNameBinding& binding)
{
    const MethodLocalSymbol* local = binding.localOrdinal.has_value() &&
            *binding.localOrdinal < methodBodyModel_->locals().size()
        ? &methodBodyModel_->locals()[*binding.localOrdinal]
        : nullptr;
    if (local != nullptr && local->isReference)
    {
        // The reference local's slot stores a pointer. Address context consumes that
        // pointer directly; value context loads through it once more.
        auto pointer = makeNode(
            ProjectOopHirNodeKind::LocalValue,
            expression,
            binding.typeKey + "*");
        pointer->localOrdinal = binding.localOrdinal;
        if (addressContext) return pointer;
        auto value = makeNode(
            ProjectOopHirNodeKind::PointerLoad,
            expression,
            binding.typeKey);
        value->children.push_back(std::move(pointer));
        return value;
    }
    auto node = makeNode(
        addressContext
            ? ProjectOopHirNodeKind::LocalAddress
            : ProjectOopHirNodeKind::LocalValue,
        expression,
        binding.typeKey);
    node->localOrdinal = binding.localOrdinal;
    node->volatileAccess = local != nullptr && local->isVolatile;
    return node;
}

// Baja un campo con el receptor y metadatos que ya resolvió Sema.
// Lowers a field with the receiver and metadata already resolved by Sema.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerFieldNameBinding(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodNameBinding& binding,
    [[maybe_unused]] std::string& error)
{
    if (binding.candidateMemberOrdinals.empty())
    {
        error = "Macro A field binding has no candidate member";
        return nullptr;
    }
    const std::size_t fieldOrdinal = binding.candidateMemberOrdinals.front();
    auto receiver = lowerReceiver(
        binding.receiverSyntax, binding.receiverTypeKey, memberOrdinal, error);
    if (receiver == nullptr) return nullptr;
    auto node = makeNode(
        addressContext
            ? ProjectOopHirNodeKind::FieldAddress
            : ProjectOopHirNodeKind::FieldValue,
        expression,
        binding.typeKey);
    node->memberOrdinal = fieldOrdinal;
    node->volatileAccess = memberModel_ != nullptr &&
        *node->memberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*node->memberOrdinal].isVolatile;
    node->baseReceiver = binding.baseReceiver;
    node->children.push_back(std::move(receiver));
    return node;
}

// Baja el estado de namespace preservando ordinal y nombre de almacenamiento.
// Lowers namespace state while preserving its ordinal and storage name.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNamespaceStateNameBinding(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodNameBinding& binding,
    [[maybe_unused]] std::string& error)
{
    if (binding.candidateMemberOrdinals.size() != 1 ||
        namespaceFunctionModel_ == nullptr ||
        binding.candidateMemberOrdinals.front() >= namespaceFunctionModel_->states().size())
    {
        error = "namespace state binding has no unique state ordinal";
        return nullptr;
    }
    const std::size_t stateOrdinal = binding.candidateMemberOrdinals.front();
    const NamespaceStateSymbol& state = namespaceFunctionModel_->states()[stateOrdinal];
    auto node = makeNode(
        addressContext
            ? ProjectOopHirNodeKind::NamespaceStateAddress
            : ProjectOopHirNodeKind::NamespaceStateValue,
        expression,
        binding.typeKey);
    node->namespaceStateOrdinal = stateOrdinal;
    node->memberOrdinal =
        (memberModel_ != nullptr ? memberModel_->members().size() : 0) + stateOrdinal;
    node->stringValue = state.storageName;
    return node;
}

// Baja un campo estático y conserva su semántica volatile.
// Lowers a static field and preserves its volatile semantics.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerStaticFieldNameBinding(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodNameBinding& binding,
    [[maybe_unused]] std::string& error)
{
    if (binding.candidateMemberOrdinals.empty())
    {
        error = "Stage 37 static-field binding has no candidate member";
        return nullptr;
    }
    auto node = makeNode(
        addressContext
            ? ProjectOopHirNodeKind::StaticFieldAddress
            : ProjectOopHirNodeKind::StaticFieldValue,
        expression,
        binding.typeKey);
    node->memberOrdinal = binding.candidateMemberOrdinals.front();
    node->volatileAccess = memberModel_ != nullptr &&
        *node->memberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*node->memberOrdinal].isVolatile;
    return node;
}

// Despacha una vinculación de nombre ya resuelta sin reordenar sus clases.
// Dispatches an already resolved name binding without reordering its classes.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBoundNameExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodNameBinding& binding,
    [[maybe_unused]] std::string& error)
{
    if (binding.kind == MethodNameBindingKind::Parameter)
        return lowerParameterNameBinding(expression, addressContext, binding);
    if (binding.kind == MethodNameBindingKind::Local)
        return lowerLocalNameBinding(expression, addressContext, binding);
    if (binding.kind == MethodNameBindingKind::ImplicitField ||
        binding.kind == MethodNameBindingKind::ExplicitThisField ||
        binding.kind == MethodNameBindingKind::ExplicitObjectField ||
        binding.kind == MethodNameBindingKind::ExplicitBaseField)
    {
        return lowerFieldNameBinding(
            expression, memberOrdinal, addressContext, binding, error);
    }
    if (binding.kind == MethodNameBindingKind::NamespaceState)
        return lowerNamespaceStateNameBinding(expression, addressContext, binding, error);
    if (binding.kind == MethodNameBindingKind::StaticField)
        return lowerStaticFieldNameBinding(expression, addressContext, binding, error);
    if (binding.kind == MethodNameBindingKind::TypeReference)
    {
        error = "Stage 37 type reference may only appear as the receiver of a static member access";
        return nullptr;
    }
    error = "Stage 34 method-group expression can only appear as call target";
    return nullptr;
}
}
