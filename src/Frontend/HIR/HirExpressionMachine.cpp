#include "HirLoweringSupport.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
bool isComparison(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::EqualEqual || kind == lexer::TokenKind::BangEqual ||
        kind == lexer::TokenKind::Less || kind == lexer::TokenKind::LessEqual ||
        kind == lexer::TokenKind::Greater || kind == lexer::TokenKind::GreaterEqual;
}


}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    std::vector<const ast::AstNode*> builtinUnaryFrames;
    const ast::AstNode* unaryLeaf = expression;
    while (unaryLeaf != nullptr && unaryLeaf->kind() == ast::AstNodeKind::UnaryExpression &&
        unaryLeaf->tokenKind() != lexer::TokenKind::Amp &&
        unaryLeaf->tokenKind() != lexer::TokenKind::Star &&
        unaryLeaf->tokenKind() != lexer::TokenKind::PlusPlus &&
        unaryLeaf->tokenKind() != lexer::TokenKind::MinusMinus &&
        callBinding(unaryLeaf) == nullptr && delegateBinding(unaryLeaf) == nullptr &&
        lambdaBinding(unaryLeaf) == nullptr)
    {
        builtinUnaryFrames.push_back(unaryLeaf);
        unaryLeaf = unaryLeaf->first();
    }

    std::unique_ptr<ProjectOopHirNode> lowered;
    if (!builtinUnaryFrames.empty())
    {
        lowered = lowerExpression(unaryLeaf, memberOrdinal, false, error);
        if (lowered != nullptr)
        {
            for (auto frame = builtinUnaryFrames.rbegin();
                 frame != builtinUnaryFrames.rend(); ++frame)
            {
                const MethodExpressionTypeBinding* frameType = expressionType(*frame);
                const MethodDelegateBinding* frameDelegate = delegateBinding(*frame);
                const std::string resultType = frameType != nullptr ? frameType->typeKey :
                    frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                auto unary = makeNode(ProjectOopHirNodeKind::Unary, *frame, resultType);
                unary->operation = (*frame)->tokenKind();
                unary->children.push_back(std::move(lowered));
                lowered = std::move(unary);
            }
        }
        else if (!lastErrorRange_.isValid() && !builtinUnaryFrames.empty())
        {
            lastErrorRange_ = builtinUnaryFrames.back()->range();
        }
    }
    else
    {
        lowered = lowerExpressionImpl(expression, memberOrdinal, addressContext, error);
    }
    const ClassMemberSymbol* activeMember = memberModel_ != nullptr &&
            memberOrdinal < memberModel_->members().size()
        ? &memberModel_->members()[memberOrdinal]
        : nullptr;
    const sema::EffectContext effects(activeMember != nullptr
        ? activeMember->effects
        : sema::EffectSet{});
    const auto directlyAllocatesHeap = [](ProjectOopHirNodeKind kind) noexcept
    {
        switch (kind)
        {
            case ProjectOopHirNodeKind::ArrayAllocation:
            case ProjectOopHirNodeKind::RangeArrayAllocation:
            case ProjectOopHirNodeKind::RawAllocation:
            case ProjectOopHirNodeKind::StringConcat:
            case ProjectOopHirNodeKind::StringSubstring:
            case ProjectOopHirNodeKind::StringFromCharArray:
            case ProjectOopHirNodeKind::DelegateCreate:
            case ProjectOopHirNodeKind::LambdaCreate:
            case ProjectOopHirNodeKind::EventAdd:
                return true;
            default:
                return false;
        }
    };
    if (lowered != nullptr && effects.allows(sema::Effect::NoHeap) &&
        directlyAllocatesHeap(lowered->kind))
    {
        error = "NS3030: noheap callable '" + activeMember->name +
            "' cannot contain direct heap allocation '" +
            std::string(projectOopHirNodeKindName(lowered->kind)) + "'";
        lowered.reset();
    }
    if (lowered != nullptr && effects.allows(sema::Effect::NoHeap))
    {
        const ClassMemberSymbol& caller = *activeMember;
        const MethodCallBinding* call = callBinding(expression);
        if (call != nullptr && call->resolution == MethodCallResolutionKind::Resolved &&
            call->selectedMemberOrdinal.has_value() &&
            *call->selectedMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& callee = memberModel_->members()[*call->selectedMemberOrdinal];
            if (!callee.isNoHeap)
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot call non-noheap callable '" + callee.signatureKey + "'";
                lowered.reset();
            }
        }
        if (lowered != nullptr &&
            (lowered->kind == ProjectOopHirNodeKind::DelegateInvoke ||
             lowered->kind == ProjectOopHirNodeKind::EventInvoke))
        {
            error = "NS3030: noheap callable '" + caller.signatureKey +
                "' cannot invoke an indirect target without a noheap contract";
            lowered.reset();
        }
        if (lowered != nullptr &&
            (lowered->kind == ProjectOopHirNodeKind::PlacementConstruction ||
             lowered->kind == ProjectOopHirNodeKind::ValueConstruction))
        {
            const ObjectConstructionBinding* binding = constructionBinding(expression);
            if (binding == nullptr || !binding->selectedConstructorMemberOrdinal.has_value() ||
                *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot invoke an implicit constructor without a noheap contract";
                lowered.reset();
            }
            else
            {
                const ClassMemberSymbol& constructor = memberModel_->members()[
                    *binding->selectedConstructorMemberOrdinal];
                if (!constructor.isNoHeap)
                {
                    error = "NS3030: noheap callable '" + caller.signatureKey +
                        "' cannot call non-noheap constructor '" +
                        constructor.signatureKey + "'";
                    lowered.reset();
                }
            }
        }
    }
    // Solo se apunta si NO hay nada apuntado: asi gana el nodo mas interno, que es el que de
    // verdad fallo, y no la expresion que lo contiene.
    if (lowered == nullptr && !error.empty() && !lastErrorRange_.isValid() && expression != nullptr)
    {
        lastErrorRange_ = expression->range();
    }
    return lowered;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionImpl(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (expression == nullptr)
    {
        error = "null expression reached Stage 34 OOP HIR";
        return nullptr;
    }
    const MethodLambdaBinding* stage43Lambda = lambdaBinding(expression);
    if (stage43Lambda != nullptr && expression->kind() == ast::AstNodeKind::LambdaExpression)
    {
        auto node = makeNode(ProjectOopHirNodeKind::LambdaCreate, expression, stage43Lambda->delegateTypeKey);
        node->elementTypeKey = stage43Lambda->delegateTypeKey;
        node->stringValue = stage43Lambda->generatedName;
        node->integerValue = stage43Lambda->lambdaOrdinal;
        for (const MethodLambdaCapture& capture : stage43Lambda->captures)
        {
            std::unique_ptr<ProjectOopHirNode> value;
            switch (capture.sourceKind)
            {
                case MethodLambdaCaptureSourceKind::OriginalLocal:
                case MethodLambdaCaptureSourceKind::ParentLocal:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                        value->localOrdinal = capture.sourceOrdinal;
                    }
                    break;
                case MethodLambdaCaptureSourceKind::OriginalParameter:
                case MethodLambdaCaptureSourceKind::ParentParameter:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                        value->parameterOrdinal = capture.sourceOrdinal;
                    }
                    break;
                case MethodLambdaCaptureSourceKind::OriginalThis:
                    value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                    break;
                case MethodLambdaCaptureSourceKind::ParentCapture:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(
                            ProjectOopHirNodeKind::ClosureCaptureCellReference,
                            expression,
                            capture.typeKey);
                        value->integerValue = *capture.sourceOrdinal;
                    }
                    break;
            }
            if (value == nullptr)
            {
                error = "Stage 43 lambda capture source is unavailable";
                return nullptr;
            }
            value->captureMode = capture.mode;
            value->booleanValue = capture.clearMovedSource;
            node->children.push_back(std::move(value));
        }
        return node;
    }

    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    if (stage42Delegate != nullptr && stage42Delegate->kind == MethodDelegateOperationKind::Create)
    {
        const bool memberTarget = stage42Delegate->selectedMemberOrdinal.has_value() &&
            memberModel_ != nullptr &&
            *stage42Delegate->selectedMemberOrdinal < memberModel_->members().size();
        const bool namespaceTarget = stage42Delegate->selectedMemberOrdinal.has_value() &&
            memberModel_ != nullptr && namespaceFunctionModel_ != nullptr &&
            *stage42Delegate->selectedMemberOrdinal >= memberModel_->members().size() &&
            *stage42Delegate->selectedMemberOrdinal - memberModel_->members().size() <
                namespaceFunctionModel_->functions().size();
        if (!memberTarget && !namespaceTarget)
        {
            error = "Stage 42 delegate creation has no selected callable";
            return nullptr;
        }
        auto node = makeNode(ProjectOopHirNodeKind::DelegateCreate, expression, stage42Delegate->delegateTypeKey);
        node->elementTypeKey = stage42Delegate->delegateTypeKey;
        node->memberOrdinal = stage42Delegate->selectedMemberOrdinal;
        node->booleanValue = stage42Delegate->virtualDispatch;
        node->interfaceDispatch = stage42Delegate->interfaceDispatch;
        node->ownership = stage42Delegate->ownsReceiver
            ? sema::OwnershipKind::Owned : sema::OwnershipKind::Borrowed;
        if (stage42Delegate->interfaceDispatch)
            node->targetTypeOrdinal = stage42Delegate->receiverTypeOrdinal;
        if (!stage42Delegate->staticTarget)
        {
            std::unique_ptr<ProjectOopHirNode> receiver;
            if (stage42Delegate->ownsReceiver &&
                (stage42Delegate->receiverSourceLocalOrdinal.has_value() ||
                 stage42Delegate->receiverSourceParameterOrdinal.has_value()))
            {
                receiver = makeNode(
                    ProjectOopHirNodeKind::MoveValue,
                    stage42Delegate->receiverSyntax,
                    stage42Delegate->receiverTypeKey);
                receiver->localOrdinal = stage42Delegate->receiverSourceLocalOrdinal;
                receiver->parameterOrdinal = stage42Delegate->receiverSourceParameterOrdinal;
                receiver->booleanValue = true;
            }
            else
            {
                receiver = lowerReceiver(
                    stage42Delegate->receiverSyntax, stage42Delegate->receiverTypeKey,
                    memberOrdinal, error);
            }
            if (receiver == nullptr) return nullptr;
            node->children.push_back(std::move(receiver));
        }
        return node;
    }

    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";

    // ID 82. `preloweredLast`, cuando no es nulo, SUSTITUYE la bajada del ultimo argumento. Tiene
    // un solo llamador: la asignacion compuesta sobre propiedad e indexador, donde el valor que
    // recibe el setter no es una expresion de la fuente sino `get_X(...) op derecho`, un arbol que
    // hay que construir en el sitio. Todo lo demas --receptor, conversiones seleccionadas,
    // borrows-- sigue siendo el mismo codigo para todas las llamadas resueltas, que es justamente
    // el motivo de entrar por aqui en vez de armar el nodo de llamada a mano.
    const auto lowerResolvedCall = [&](
        const MethodCallBinding* binding,
        const std::vector<const ast::AstNode*>& argumentSyntaxes,
        std::unique_ptr<ProjectOopHirNode> preloweredLast = nullptr,
        bool nullConditional = false)
        -> std::unique_ptr<ProjectOopHirNode>
    {
        return lowerBoundCall(
            expression,
            memberOrdinal,
            addressContext,
            error,
            typeBinding,
            binding,
            argumentSyntaxes,
            std::move(preloweredLast),
            nullConditional);
    };

    const auto lowerEventBacking = [&](
        const MethodEventBinding* binding,
        bool address) -> std::unique_ptr<ProjectOopHirNode>
    {
        return lowerEventBackingField(expression, memberOrdinal, error, binding, address);
    };

    switch (expression->kind())
    {
        case ast::AstNodeKind::LoopExpression:
        {
            if (addressContext)
            {
                error = "a loop expression produces a value, not addressable storage";
                return nullptr;
            }
            const MethodLoopExpressionBinding* binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->loopExpressionBinding(expression) : nullptr;
            if (binding == nullptr)
            {
                error = "T07-F05 loop expression has no semantic binding";
                return nullptr;
            }
            auto node = makeNode(
                ProjectOopHirNodeKind::LoopExpression, expression, binding->resultTypeKey);
            node->stringValue = binding->label;
            auto body = lowerBlock(expression->first(), memberOrdinal, error);
            if (body == nullptr) return nullptr;
            node->children.push_back(std::move(body));
            return node;
        }
        case ast::AstNodeKind::ThisExpression:
        {
            if (currentLambda_ != nullptr)
            {
                const std::optional<std::size_t> capture = currentLambdaThisCapture();
                if (!capture.has_value())
                {
                    error = "Stage 43 lambda this-reference has no captured receiver";
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext && isStructValueType(resultType)
                        ? ProjectOopHirNodeKind::ClosureCaptureAddress
                        : ProjectOopHirNodeKind::ClosureCaptureValue,
                    expression,
                    resultType);
                node->integerValue = *capture;
                return node;
            }
            // `this` inside a STRUCT method is the receiver's ADDRESS -- struct receivers are
            // passed by address and memory-homed since Stage 40.2 -- so in a VALUE context the
            // expression `this` has to load through it, exactly the way a `ref` parameter does a
            // few cases below. Handing back the bare ThisValue there was two reported bugs at
            // once, and they are the same line:
            //
            //   * `Get(this)` passed the POINTER where the callee's signature wanted the
            //     aggregate; the LLVM verifier rejected the module. Loud.
            //   * `S8 self = this;` stored the pointer BITS into the local's own bytes -- for an
            //     8-byte struct the carrier is `i64`, so `store ptr` fit and nothing complained --
            //     and every later field read decoded those bits as the struct's fields. Silent,
            //     no diagnostic, passes the verifier, and it is the one that reaches production.
            //     It was also, bitterly, the workaround people wrote for the loud one.
            //
            // Nothing changes for a CLASS: `this` IS the reference, and its value in both
            // contexts is that pointer. Nothing changes in an ADDRESS context either -- and every
            // existing consumer of ThisValue is one: `lowerReceiver` lowers a struct receiver
            // with addressContext = true (so `this.field` and `this.Method()` keep taking the
            // address), and the constructor-initializer and instance-initializer helpers build
            // ThisValue directly as a receiver.
            auto self = makeNode(ProjectOopHirNodeKind::ThisValue, expression, resultType);
            if (addressContext || !isStructValueType(resultType))
            {
                return self;
            }
            auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
            load->children.push_back(std::move(self));
            return load;
        }

        case ast::AstNodeKind::NullConditionalMemberExpression:
        {
            if (addressContext)
            {
                error = "a null-conditional access produces a value, not addressable storage";
                return nullptr;
            }
            const MethodExpressionTypeBinding* receiverFlow = expressionType(expression->first());
            const bool provenNonNull = receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
            if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
                accessorCall != nullptr)
            {
                auto call = lowerResolvedCall(accessorCall, {}, nullptr, !provenNonNull);
                if (call == nullptr) return nullptr;
                if (provenNonNull) call->stringValue = "flow-nonnull";
                return call;
            }
            const MethodNameBinding* binding = nameBinding(expression);
            if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
                binding->kind != MethodNameBindingKind::ExplicitObjectField)
            {
                error = "T01-F03 null-conditional access has no unique instance-field binding";
                return nullptr;
            }
            auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (receiver == nullptr) return nullptr;
            auto node = makeNode(
                provenNonNull ? ProjectOopHirNodeKind::FieldValue
                              : ProjectOopHirNodeKind::NullConditionalFieldValue,
                expression, resultType);
            node->memberOrdinal = binding->candidateMemberOrdinals.front();
            node->booleanValue = provenNonNull;
            node->volatileAccess = memberModel_ != nullptr &&
                *node->memberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*node->memberOrdinal].isVolatile;
            node->children.push_back(std::move(receiver));
            return node;
        }

        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            if (core != nullptr && core->kind == MethodCoreOperationKind::FixedArrayLength)
            {
                if (addressContext)
                {
                    error = "NativeSharp 3.0 fixed-array Length is a constant value and is not addressable";
                    return nullptr;
                }
                const std::optional<std::uint64_t> count =
                    ProjectMemberModel::fixedArrayCount(core->elementTypeKey);
                if (!count.has_value())
                {
                    error = "NativeSharp 3.0 fixed-array Length lost its canonical extent";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::IntegerConstant, expression, core->resultTypeKey);
                node->integerValue = *count;
                return node;
            }
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayLength ||
                 core->kind == MethodCoreOperationKind::StringLength))
            {
                if (addressContext)
                {
                    error = "Stage 41 Length is an intrinsic value and is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto node = makeNode(
                    core->kind == MethodCoreOperationKind::ArrayLength
                        ? ProjectOopHirNodeKind::ArrayLength
                        : ProjectOopHirNodeKind::StringLength,
                    expression,
                    core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                return node;
            }
            if (core != nullptr && core->kind == MethodCoreOperationKind::IteratorCurrent)
            {
                // NS304-T06-F05: a `ref:`/`refmut:` item Current IS a reference into the frame, so
                // taking it in address context is allowed (it yields the reference, not a value).
                const bool itemByReference = core->resultTypeKey.starts_with("ref:") ||
                    core->resultTypeKey.starts_with("refmut:");
                if (addressContext && !itemByReference)
                {
                    error = "iterator Current is an intrinsic value and is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::IteratorCurrent, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                return node;
            }
            // NSC-LIMIT-008: la misma pregunta que hace isNaturallyAddressable, hecha una sola
            // vez. Antes estaba escrita aqui a mano y alli no se hacia, y esa discrepancia era el
            // defecto: un getter se clasificaba como direccionable y lowerReceiver no le hacia
            // temporal.
            if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
                accessorCall != nullptr)
            {
                const std::size_t accessorOrdinal = *accessorCall->selectedMemberOrdinal;
                if (addressContext && !memberModel_->members()[accessorOrdinal].isReferenceReturn)
                {
                    error = "Stage 38 properties are semantic accessor calls and are not addressable";
                    return nullptr;
                }
                return lowerResolvedCall(accessorCall, {});
            }
            // NS304-T03-F03: a marked list-pattern length member that resolved to a
            // zero-argument `Length()` method was auto-invoked in Sema; lower it as the
            // ordinary instance call it became.
            if (const MethodCallBinding* lengthCall = callBinding(expression);
                lengthCall != nullptr && expression->auxiliaryText() == "list-pattern-length")
            {
                return lowerResolvedCall(lengthCall, {});
            }
            // NS304-T08-F02: extension property getter auto-invocado en Sema (namespace call con
            // receptor reenviado). Se baja igual que la llamada de namespace del path de Call.
            if (const NamespaceFunctionCallBinding* namespaceCall =
                    namespaceFunctionCallBinding(expression);
                namespaceCall != nullptr)
            {
                if (namespaceCall->resolution != MethodCallResolutionKind::Resolved ||
                    !namespaceCall->selectedFunctionOrdinal.has_value() ||
                    namespaceFunctionModel_ == nullptr ||
                    *namespaceCall->selectedFunctionOrdinal >=
                        namespaceFunctionModel_->functions().size())
                {
                    error = "extension property getter reached HIR without a resolved target";
                    return nullptr;
                }
                const std::size_t functionOrdinal = *namespaceCall->selectedFunctionOrdinal;
                const NamespaceFunctionSymbol& target =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::StaticCall, expression, namespaceCall->resultTypeKey);
                node->namespaceFunctionOrdinal = functionOrdinal;
                node->stringValue = target.mangledName;
                const std::size_t targetCallableOrdinal =
                    (memberModel_ != nullptr ? memberModel_->members().size() : 0) +
                    functionOrdinal;
                bool hasOwnedTemporary = false;
                if (namespaceCall->forwardedReceiver)
                {
                    if (namespaceCall->receiverSyntax == nullptr || target.parameters.empty() ||
                        namespaceCall->selectedArgumentParameterOrdinals.empty())
                    {
                        error = "extension property getter lost its receiver contract";
                        return nullptr;
                    }
                    const std::size_t parameterOrdinal =
                        namespaceCall->selectedArgumentParameterOrdinals.front();
                    if (parameterOrdinal >= target.parameters.size())
                    {
                        error = "extension receiver has no matching parameter";
                        return nullptr;
                    }
                    const bool addressReceiver =
                        target.parameters[parameterOrdinal].passKind != ParameterPassKind::Value;
                    auto receiver = lowerExpression(
                        namespaceCall->receiverSyntax, memberOrdinal, addressReceiver, error);
                    if (receiver == nullptr) return nullptr;
                    if (!addressReceiver && !namespaceCall->selectedConversions.empty() &&
                        namespaceCall->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        receiver = wrapImplicitConversion(
                            std::move(receiver), namespaceCall->selectedConversions.front(),
                            namespaceCall->receiverSyntax);
                    }
                    receiver = wrapCallArgumentBorrow(
                        std::move(receiver), namespaceCall->receiverSyntax,
                        targetCallableOrdinal, parameterOrdinal, hasOwnedTemporary);
                    node->children.push_back(std::move(receiver));
                    node->argumentParameterOrdinals.push_back(parameterOrdinal);
                    node->argumentTypeKeys.push_back(target.parameters[parameterOrdinal].typeKey);
                }
                return node;
            }
            if (currentLambda_ != nullptr)
            {
                const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(expression);
                if (lambdaValue != nullptr && lambdaValue->lambdaOrdinal == currentLambda_->lambdaOrdinal)
                {
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
                            addressContext ? ProjectOopHirNodeKind::ParameterAddress : ProjectOopHirNodeKind::ParameterValue,
                            expression, lambdaValue->typeKey);
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
            }
            const MethodNameBinding* binding = nameBinding(expression);
            if (binding == nullptr)
            {
                error = "Stage 34 cannot map identifier/member expression to Stage 27 binding";
                return nullptr;
            }
            if (binding->kind == MethodNameBindingKind::Parameter)
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
                if (binding->parameterOrdinal.has_value() && memberModel_ != nullptr)
                {
                    if (binding->methodMemberOrdinal < memberModel_->members().size())
                    {
                        const ClassMemberSymbol& owner =
                            memberModel_->members()[binding->methodMemberOrdinal];
                        if (*binding->parameterOrdinal < owner.parameters.size())
                            parameter = &owner.parameters[*binding->parameterOrdinal];
                    }
                    else if (namespaceFunctionModel_ != nullptr)
                    {
                        const std::size_t functionOrdinal =
                            binding->methodMemberOrdinal - memberModel_->members().size();
                        if (functionOrdinal < namespaceFunctionModel_->functions().size())
                        {
                            const NamespaceFunctionSymbol& owner =
                                namespaceFunctionModel_->functions()[functionOrdinal];
                            if (*binding->parameterOrdinal < owner.parameters.size())
                                parameter = &owner.parameters[*binding->parameterOrdinal];
                        }
                    }
                }
                if (parameter != nullptr && parameter->passKind != ParameterPassKind::Value)
                {
                    auto pointer = makeNode(
                        ProjectOopHirNodeKind::ParameterValue, expression, parameter->typeKey);
                    pointer->parameterOrdinal = binding->parameterOrdinal;
                    if (addressContext) return pointer;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad, expression, parameter->pointeeTypeKey);
                    value->children.push_back(std::move(pointer));
                    return value;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::ParameterAddress : ProjectOopHirNodeKind::ParameterValue,
                    expression,
                    binding->typeKey);
                node->parameterOrdinal = binding->parameterOrdinal;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::Local)
            {
                const MethodLocalSymbol* local = binding->localOrdinal.has_value() &&
                        *binding->localOrdinal < methodBodyModel_->locals().size()
                    ? &methodBodyModel_->locals()[*binding->localOrdinal]
                    : nullptr;
                if (local != nullptr && local->isReference)
                {
                    // The reference local's slot stores a pointer. Address context consumes that
                    // pointer directly; value context loads through it once more.
                    auto pointer = makeNode(
                        ProjectOopHirNodeKind::LocalValue,
                        expression,
                        binding->typeKey + "*");
                    pointer->localOrdinal = binding->localOrdinal;
                    if (addressContext) return pointer;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad,
                        expression,
                        binding->typeKey);
                    value->children.push_back(std::move(pointer));
                    return value;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::LocalAddress : ProjectOopHirNodeKind::LocalValue,
                    expression,
                    binding->typeKey);
                node->localOrdinal = binding->localOrdinal;
                node->volatileAccess = local != nullptr && local->isVolatile;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::ImplicitField ||
                binding->kind == MethodNameBindingKind::ExplicitThisField ||
                binding->kind == MethodNameBindingKind::ExplicitObjectField ||
                binding->kind == MethodNameBindingKind::ExplicitBaseField)
            {
                if (binding->candidateMemberOrdinals.empty())
                {
                    error = "Macro A field binding has no candidate member";
                    return nullptr;
                }
                const std::size_t fieldOrdinal = binding->candidateMemberOrdinals.front();
                auto receiver = lowerReceiver(
                    binding->receiverSyntax, binding->receiverTypeKey, memberOrdinal, error);
                if (receiver == nullptr)
                {
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::FieldAddress : ProjectOopHirNodeKind::FieldValue,
                    expression,
                    binding->typeKey);
                node->memberOrdinal = fieldOrdinal;
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                node->baseReceiver = binding->baseReceiver;
                node->children.push_back(std::move(receiver));
                return node;
            }
            if (binding->kind == MethodNameBindingKind::NamespaceState)
            {
                if (binding->candidateMemberOrdinals.size() != 1 ||
                    namespaceFunctionModel_ == nullptr ||
                    binding->candidateMemberOrdinals.front() >=
                        namespaceFunctionModel_->states().size())
                {
                    error = "namespace state binding has no unique state ordinal";
                    return nullptr;
                }
                const std::size_t stateOrdinal =
                    binding->candidateMemberOrdinals.front();
                const NamespaceStateSymbol& state =
                    namespaceFunctionModel_->states()[stateOrdinal];
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::NamespaceStateAddress
                                   : ProjectOopHirNodeKind::NamespaceStateValue,
                    expression,
                    binding->typeKey);
                node->namespaceStateOrdinal = stateOrdinal;
                node->memberOrdinal =
                    (memberModel_ != nullptr ? memberModel_->members().size() : 0) +
                    stateOrdinal;
                node->stringValue = state.storageName;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::StaticField)
            {
                if (binding->candidateMemberOrdinals.empty())
                {
                    error = "Stage 37 static-field binding has no candidate member";
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::StaticFieldAddress
                                   : ProjectOopHirNodeKind::StaticFieldValue,
                    expression,
                    binding->typeKey);
                node->memberOrdinal = binding->candidateMemberOrdinals.front();
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::TypeReference)
            {
                error = "Stage 37 type reference may only appear as the receiver of a static member access";
                return nullptr;
            }
            error = "Stage 34 method-group expression can only appear as call target";
            return nullptr;
        }

        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, resultType);
            node->integerValue = expression->integerValue();
            return node;
        }

        case ast::AstNodeKind::FloatingLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, resultType);
            node->floatingValue = expression->floatingValue();
            return node;
        }

        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::NameOfExpression:
        {
            if (addressContext)
            {
                error = "Stage 41 string literals are immutable references and are not addressable values";
                return nullptr;
            }
            if (expression->tokenKind() == lexer::TokenKind::Utf8StringLiteral)
            {
                // NS304-T10-F01: u8"..." is a ReadOnlySpan<byte> over the literal's UTF-8 bytes.
                const MethodCoreBinding* binding = coreBinding(expression);
                const std::string spanKey = binding != nullptr
                    ? binding->resultTypeKey : std::string{};
                auto node = makeNode(
                    ProjectOopHirNodeKind::Utf8StringLiteral, expression,
                    spanKey.empty() ? "builtin:byte" : spanKey);
                node->stringValue = expression->text();
                node->elementTypeKey = "builtin:byte";
                node->integerValue = node->stringValue.size();
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = expression->text();
            return node;
        }

        case ast::AstNodeKind::OverflowPolicyExpression:
        {
            auto operand = lowerExpression(expression->first(), memberOrdinal, addressContext, error);
            if (operand == nullptr) return nullptr;
            auto node = makeNode(
                ProjectOopHirNodeKind::OverflowPolicy, expression, operand->typeKey);
            node->stringValue = expression->auxiliaryText();
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::InterpolatedStringExpression:
        {
            if (const MethodCoreBinding* core = coreBinding(expression);
                core != nullptr && core->kind == MethodCoreOperationKind::InterpolationHandler)
            {
                if (addressContext)
                {
                    error = "an interpolation handler result is a value, not addressable storage";
                    return nullptr;
                }
                const std::string handlerType = core->resultTypeKey;
                const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;
                auto block = makeNode(
                    ProjectOopHirNodeKind::BlockExpression, expression, handlerType);

                auto declaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration, expression, handlerType);
                declaration->localOrdinal = tempOrdinal;
                declaration->stringValue = "handler";
                declaration->children.push_back(makeNode(
                    ProjectOopHirNodeKind::DefaultValue, expression, handlerType));
                block->children.push_back(std::move(declaration));

                std::vector<language::InterpolationPart> parts;
                std::string parseError;
                if (!language::parseStaticInterpolation(expression->text(), parts, parseError))
                {
                    error = std::move(parseError);
                    return nullptr;
                }
                std::size_t holeIndex = 0;
                for (const language::InterpolationPart& part : parts)
                {
                    const bool isHole = part.kind == language::InterpolationPartKind::Hole;
                    const std::size_t memberOrdinal =
                        isHole ? core->builderMemberOrdinals[1] : core->builderMemberOrdinals[0];
                    std::unique_ptr<ProjectOopHirNode> argument;
                    if (isHole)
                    {
                        if (holeIndex >= expression->children().size())
                        {
                            error = "interpolation handler hole has no expression";
                            return nullptr;
                        }
                        argument = lowerExpression(
                            expression->children()[holeIndex++], memberOrdinal, false, error);
                        if (argument == nullptr) return nullptr;
                    }
                    else
                    {
                        argument = makeNode(
                            ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
                        argument->stringValue = part.text;
                    }
                    auto call = makeNode(
                        ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
                    call->memberOrdinal = memberOrdinal;
                    auto receiver = makeNode(
                        ProjectOopHirNodeKind::LocalAddress, expression, handlerType + "*");
                    receiver->localOrdinal = tempOrdinal;
                    call->children.push_back(std::move(receiver));
                    call->children.push_back(std::move(argument));
                    auto statement = makeNode(
                        ProjectOopHirNodeKind::ExpressionStatement, expression, "builtin:void");
                    statement->children.push_back(std::move(call));
                    block->children.push_back(std::move(statement));
                }

                auto finish = makeNode(
                    ProjectOopHirNodeKind::InstanceCall, expression, handlerType);
                finish->memberOrdinal = core->builderMemberOrdinals[2];
                auto finishReceiver = makeNode(
                    ProjectOopHirNodeKind::LocalAddress, expression, handlerType + "*");
                finishReceiver->localOrdinal = tempOrdinal;
                finish->children.push_back(std::move(finishReceiver));
                block->children.push_back(std::move(finish));

                return block;
            }
            if (addressContext)
            {
                error = "a static interpolated string is immutable and not addressable";
                return nullptr;
            }
            const std::string* value = methodBodyModel_ != nullptr
                ? methodBodyModel_->staticInterpolationValue(expression) : nullptr;
            if (value == nullptr)
            {
                error = "T01-F06 static interpolation has no compile-time handler result";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = *value;
            return node;
        }

        case ast::AstNodeKind::BooleanLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::BooleanConstant, expression, "builtin:bool");
            node->booleanValue = expression->tokenKind() == lexer::TokenKind::KwTrue;
            return node;
        }

        case ast::AstNodeKind::NullLiteralExpression:
            return makeNode(ProjectOopHirNodeKind::NullConstant, expression, "builtin:null");

        case ast::AstNodeKind::DefaultValueExpression:
        {
            // Stage 52: default(T) -- the resolved concrete type (already monomorphized, if T
            // was a generic parameter -- see ProjectMethodBodyModel's DefaultValueExpression
            // case) was recorded during semantic analysis; LLVM lowering builds the all-zero-bits
            // value of that EXACT type (see the DefaultValue case in ProjectOopLlvmLowerer.cpp),
            // which is uniformly correct for every T: 0/false/'\0' for a numeric primitive, a
            // null pointer for a reference type or string, an all-zero-fields instance for a
            // value-typed struct.
            if (addressContext)
            {
                error = "Stage 52 default(T) value is not addressable";
                return nullptr;
            }
            const MethodExpressionTypeBinding* type = expressionType(expression);
            if (type == nullptr)
            {
                error = "Stage 52 default(T) expression has no resolved semantic type";
                return nullptr;
            }
            return makeNode(ProjectOopHirNodeKind::DefaultValue, expression, type->typeKey);
        }

        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
        {
            const MethodCallBinding* operatorCall = callBinding(expression);
            if (operatorCall != nullptr && operatorCall->resolution == MethodCallResolutionKind::Resolved &&
                operatorCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal) != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal)->isUserOperator)
            {
                return lowerResolvedCall(operatorCall, {expression->first()});
            }
            if (expression->tokenKind() == lexer::TokenKind::PlusPlus ||
                expression->tokenKind() == lexer::TokenKind::MinusMinus)
            {
                // ID 79. `++x` es `x += 1` y `x++` es lo mismo devolviendo el valor anterior, asi
                // que los dos salen por el MISMO nodo que la asignacion compuesta y no por una
                // ruta paralela. La tarea 62 pedia extender esto "para punteros" cuando no existia
                // para ningun tipo; aqui entra para los dos a la vez, que es la unica forma de que
                // `++p` y `++i` no se separen.
                if (addressContext)
                {
                    error = "the result of ++/-- is a value, not addressable storage";
                    return nullptr;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "Stage 34 ++/-- operand has no resolved semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;

                auto step = makeNode(ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
                step->operation = expression->tokenKind() == lexer::TokenKind::PlusPlus
                    ? lexer::TokenKind::Plus
                    : lexer::TokenKind::Minus;
                // La UNICA diferencia entre postfijo y prefijo.
                step->booleanValue = expression->kind() == ast::AstNodeKind::PostfixExpression;

                std::unique_ptr<ProjectOopHirNode> one;
                if (ProjectTypeConversion::isPointer(targetType->typeKey))
                {
                    // `++p` avanza UN ELEMENTO, no un byte. El tipo apuntado es lo que da el paso,
                    // y lo resuelve el MIR con el mismo servicio de tamano que todo lo demas.
                    step->elementTypeKey = targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
                    one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nint");
                    one->integerValue = 1;
                }
                else if (ProjectTypeConversion::isNumeric(targetType->typeKey) &&
                         !ProjectTypeConversion::isIntegral(targetType->typeKey))
                {
                    one = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, targetType->typeKey);
                    one->floatingValue = 1.0;
                }
                else
                {
                    // El uno se construye YA en el tipo del destino, no en `int` con una conversion
                    // detras: asi la operacion entera ocurre en un solo tipo y no hay una segunda
                    // regla de promocion que mantener de acuerdo con la de `Binary`.
                    one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, targetType->typeKey);
                    one->integerValue = 1;
                }
                step->children.push_back(std::move(address));
                step->children.push_back(std::move(one));
                return step;
            }
            // Interop Foundation: `&lvalue` and `*pointer` are now first-class in the OOP
            // pipeline. HISTORY: both used to be walled off ("keeps raw pointer
            // address/dereference in the procedural backend boundary"), which made every
            // out-parameter Win32 API unusable -- `GetMessage(&msg)` could not produce a MSG*
            // from a local at all. Since Stage 40.2 every local is memory-homed (an alloca,
            // with indirect structs carried as [N x i8]), so an address-of is LITERALLY the
            // operand lowered in address context -- no new machinery, just returning what the
            // assignment path always computed for its own targets.
            //
            // These are the language's RAW tier -- same third reference category the Bloque 1
            // docs pin for plain aliases: no lifetime tracking, dangles if it outlives its
            // target, by design. C APIs are expressed through exactly this shape.
            if (expression->tokenKind() == lexer::TokenKind::Amp)
            {
                if (addressContext)
                {
                    error = "Stage 34 address-of result is not itself addressable";
                    return nullptr;
                }
                // Bit fields are logical slices of an allocation unit, not independently
                // addressable objects.  Assignment lowering may request their allocation-unit
                // address internally, but source `&field` must never expose that address as a
                // pointer to the logical field type.
                if (const MethodNameBinding* target = nameBinding(expression->first());
                    target != nullptr && !target->candidateMemberOrdinals.empty() &&
                    memberModel_ != nullptr &&
                    target->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                    memberModel_->members()[target->candidateMemberOrdinals.front()].isBitField)
                {
                    error = "bit fields have no address";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                // WRAPPED, never mutated: overwriting the operand node's typeKey was measured
                // to miscompile -- a ParameterValue whose typeKey was rewritten to the pointee
                // struct loaded the [N x i8] aggregate where the pointer belonged ("GEP base
                // pointer is not a vector..."). The wrapper carries the expression's type (T*)
                // for consumers that derive ABI decisions from operand typeKeys (call-site
                // indirect-argument wrapping reads exactly that), while the operand keeps its
                // own meaning.
                auto node = makeNode(ProjectOopHirNodeKind::AddressValue, expression, resultType);
                node->children.push_back(std::move(address));
                return node;
            }
            if (expression->tokenKind() == lexer::TokenKind::Star)
            {
                if (addressContext)
                {
                    // The ADDRESS of `*p` is p's own value: `(*m).field = x` GEPs off it, and
                    // `*p = v` stores straight through it.
                    auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
                    if (pointer == nullptr) return nullptr;
                    // Returned AS-IS: the pointer's value IS the address, and its own typeKey
                    // (T*) is already the honest description of an address-of-T.
                    return pointer;
                }
                auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (pointer == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
                node->children.push_back(std::move(pointer));
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Unary, expression, resultType);
            node->operation = expression->tokenKind();
            auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::NullCoalescingExpression:
        {
            if (addressContext)
            {
                error = "a null-coalescing expression produces a value, not addressable storage";
                return nullptr;
            }
            auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
            auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (left == nullptr || right == nullptr) return nullptr;
            const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                right->typeKey,
                resultType,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            if (!conversion.exists())
            {
                error = "T01-F03 null-coalescing RHS cannot convert from '" + right->typeKey +
                    "' to '" + resultType + "'";
                return nullptr;
            }
            if (conversion.kind != ProjectConversionKind::Identity)
                right = wrapImplicitConversion(std::move(right), conversion, expression->second());
            const MethodOptionalCarrierBinding* optional = optionalCarrierBinding(expression);
            auto node = makeNode(
                optional != nullptr ? ProjectOopHirNodeKind::OptionalCoalescing
                                    : ProjectOopHirNodeKind::NullCoalescing,
                expression, resultType);
            if (optional != nullptr)
            {
                node->elementTypeKey = optional->carrierTypeKey;
                node->memberOrdinal = optional->hasValueMemberOrdinal;
                node->secondaryMemberOrdinal = optional->uncheckedValueMemberOrdinal;
            }
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            return node;
        }

        case ast::AstNodeKind::MatchValueExpression:
        {
            if (addressContext)
            {
                auto node = makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
                node->referenceResult = true;
                return node;
            }
            return makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
        }

        case ast::AstNodeKind::MatchExpression:
        {
            const MethodMatchExpressionBinding* binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->matchExpressionBinding(expression) : nullptr;
            if (binding == nullptr)
            {
                error = "T03-F03 match expression has no semantic binding";
                return nullptr;
            }
            if (addressContext && !binding->referenceResult)
            {
                error = "a value match expression does not produce addressable storage";
                return nullptr;
            }

            auto node = makeNode(ProjectOopHirNodeKind::MatchExpression, expression, resultType);
            node->referenceResult = binding->referenceResult;
            node->governingReference = binding->governingReference;
            node->secondaryMemberOrdinal = binding->switchTagMemberOrdinal;
            node->booleanValue = binding->switchTagMemberOrdinal.has_value();
            auto governing = lowerExpression(
                expression->first(), memberOrdinal, binding->governingReference, error);
            if (governing == nullptr) return nullptr;
            node->children.push_back(std::move(governing));

            for (std::size_t armIndex = 0; armIndex < expression->children().size(); ++armIndex)
            {
                const ast::AstNode* armSyntax = expression->children()[armIndex];
                if (armSyntax == nullptr || armSyntax->first() == nullptr ||
                    armSyntax->third() == nullptr)
                {
                    error = "T03-F03 malformed match expression arm";
                    return nullptr;
                }
                auto arm = makeNode(ProjectOopHirNodeKind::MatchArm, armSyntax, resultType);
                if (armIndex < binding->armTags.size())
                    arm->switchLabels = binding->armTags[armIndex];
                auto condition = lowerExpression(armSyntax->first(), memberOrdinal, false, error);
                if (condition == nullptr) return nullptr;
                arm->children.push_back(std::move(condition));

                if (armSyntax->second() != nullptr)
                {
                    auto guard = lowerExpression(armSyntax->second(), memberOrdinal, false, error);
                    if (guard == nullptr) return nullptr;
                    arm->children.push_back(std::move(guard));
                }
                else
                {
                    auto guard = makeNode(
                        ProjectOopHirNodeKind::BooleanConstant, armSyntax, "builtin:bool");
                    guard->booleanValue = true;
                    arm->children.push_back(std::move(guard));
                }

                const std::size_t cleanupDepth = activeHirCleanupRegions_.size();
                for (const ast::AstNode* declaration : armSyntax->children())
                {
                    auto lowered = lowerStatement(declaration, memberOrdinal, error);
                    if (lowered == nullptr)
                    {
                        activeHirCleanupRegions_.resize(cleanupDepth);
                        return nullptr;
                    }
                    if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                        lowered->cleanupRegionOrdinal.has_value())
                    {
                        activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
                    }
                    arm->children.push_back(std::move(lowered));
                }

                auto value = lowerExpression(
                    armSyntax->third(), memberOrdinal, binding->referenceResult, error);
                if (value == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepth);
                    return nullptr;
                }
                if (!binding->referenceResult && armIndex < binding->armConversions.size())
                {
                    const ProjectImplicitConversion& conversion = binding->armConversions[armIndex];
                    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                        value = wrapImplicitConversion(
                            std::move(value), conversion, armSyntax->third());
                }
                arm->children.push_back(std::move(value));
                arm->cleanupRegionOrdinals.assign(
                    activeHirCleanupRegions_.begin() + static_cast<std::ptrdiff_t>(cleanupDepth),
                    activeHirCleanupRegions_.end());
                activeHirCleanupRegions_.resize(cleanupDepth);
                node->children.push_back(std::move(arm));
            }

            if (!binding->referenceResult || addressContext) return node;
            auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
            load->children.push_back(std::move(node));
            return load;
        }

        case ast::AstNodeKind::MatchArmExpression:
            error = "a match arm reached HIR outside its match expression";
            return nullptr;

        case ast::AstNodeKind::ConditionalExpression:
        {
            if (addressContext)
            {
                error = "a conditional expression produces a value, not addressable storage";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Conditional, expression, resultType);
            auto condition = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (condition == nullptr) return nullptr;
            auto whenTrue = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (whenTrue == nullptr) return nullptr;
            auto whenFalse = lowerExpression(expression->third(), memberOrdinal, false, error);
            if (whenFalse == nullptr) return nullptr;

            // Both branches have to arrive at the phi AS THE RESULT TYPE. The front end already
            // decided that type -- the common numeric type when the branches differ -- but
            // deciding it is not the same as producing it: without these conversions, `n > 0 ? anInt
            // : aLong` lowers to a phi whose result is `i64` and whose first incoming value is
            // still `i32`, and the LLVM verifier rejects the module with "PHI node operands are not
            // the same type as the result". Same treatment the binary operators give their operands
            // once commonNumericType has spoken.
            const auto convertBranch = [&](std::unique_ptr<ProjectOopHirNode>& branch,
                                           const ast::AstNode* syntax)
            {
                if (branch->typeKey == resultType) return;
                const MethodExpressionTypeBinding* binding = expressionType(syntax);
                const ProjectImplicitConversion conversion = ProjectTypeConversion::classifyImplicit(
                    branch->typeKey,
                    resultType,
                    binding != nullptr ? binding->integerConstant : std::nullopt);
                if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                {
                    branch = wrapImplicitConversion(std::move(branch), conversion, syntax);
                }
            };
            convertBranch(whenTrue, expression->second());
            convertBranch(whenFalse, expression->third());

            node->children.push_back(std::move(condition));
            node->children.push_back(std::move(whenTrue));
            node->children.push_back(std::move(whenFalse));
            return node;
        }

        case ast::AstNodeKind::BinaryExpression:
        {
            const MethodCoreBinding* stringCore = coreBinding(expression);
            // NSC-LIMIT-004. `a == b` y `a != b` sobre dos structs. El modelo semantico registro
            // un `StructEquals` sobre el propio BinaryExpression, asi que aqui se construye el
            // MISMO nodo que produce `a.Equals(b)` -- no hay una segunda semantica de igualdad de
            // agregados, solo dos formas de escribirla. La clase del nodo de sintaxis es lo que
            // las distingue: alli es una CallExpression, aqui un BinaryExpression.
            //
            // Para `!=` se envuelve la igualdad en el `Unary`/`Bang` que ya existe, en vez de
            // dar al nodo de agregados un modo negado propio: negar un bool es una instruccion, y
            // un segundo modo seria un segundo sitio donde la regla puede discrepar.
            if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::StructEquals)
            {
                if (addressContext)
                {
                    error = "C8 struct comparison result is not addressable";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::AggregateEquals, expression, stringCore->resultTypeKey);
                node->elementTypeKey = stringCore->elementTypeKey;
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (left == nullptr) return nullptr;
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (right == nullptr) return nullptr;
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                node->argumentCount = node->children.size();
                if (!appendAggregateLeaves(
                        stringCore->elementTypeKey, 0, expression, node->children, error))
                {
                    return nullptr;
                }
                if (expression->tokenKind() == lexer::TokenKind::BangEqual)
                {
                    auto negated = makeNode(
                        ProjectOopHirNodeKind::Unary, expression, stringCore->resultTypeKey);
                    negated->operation = lexer::TokenKind::Bang;
                    negated->children.push_back(std::move(node));
                    return negated;
                }
                return node;
            }
            if (stringCore != nullptr &&
                (stringCore->kind == MethodCoreOperationKind::StringConcat ||
                 stringCore->kind == MethodCoreOperationKind::StringEquals))
            {
                if (addressContext)
                {
                    error = "Stage 51 string operation result is not addressable";
                    return nullptr;
                }
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (left == nullptr || right == nullptr) return nullptr;
                auto node = makeNode(
                    stringCore->kind == MethodCoreOperationKind::StringConcat
                        ? ProjectOopHirNodeKind::StringConcat
                        : ProjectOopHirNodeKind::StringEquals,
                    expression,
                    stringCore->resultTypeKey);
                node->operation = expression->tokenKind();
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                return node;
            }

            // CC1 fase 4D. El binding ya decidio que esto es aritmetica de punteros y con que
            // tipo de elemento; aqui solo se construye el nodo con los dos operandos EN EL ORDEN
            // EN QUE SE ESCRIBIERON. Ese orden importa: `i + p` y `p + i` dan lo mismo, pero
            // `p - i` y `i - p` no, y el backend necesita saber cual de los dos hijos es el
            // puntero. Lo deduce de los tipos de los hijos, no de una bandera aparte.
            if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::PointerArithmetic)
            {
                if (addressContext)
                {
                    error = "CC1 pointer arithmetic yields a value, not addressable storage";
                    return nullptr;
                }
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (left == nullptr) return nullptr;
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (right == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::PointerArithmetic, expression, stringCore->resultTypeKey);
                node->operation = expression->tokenKind();
                node->elementTypeKey = stringCore->elementTypeKey;
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                return node;
            }

            const MethodCallBinding* operatorCall = callBinding(expression);
            if (operatorCall != nullptr && operatorCall->resolution == MethodCallResolutionKind::Resolved &&
                operatorCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal) != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal)->isUserOperator)
            {
                return lowerResolvedCall(operatorCall, {expression->first(), expression->second()});
            }
            auto node = makeNode(ProjectOopHirNodeKind::Binary, expression, resultType);
            node->operation = expression->tokenKind();
            node->booleanValue = typeBinding != nullptr && typeBinding->flowRuntimeCheckElided;
            auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
            auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (left == nullptr || right == nullptr)
            {
                return nullptr;
            }

            // The semantic model admits exactly one implicit integer-to-enum form: literal zero
            // in ==/!=. Materialize that decision as the same Conversion node an explicit enum
            // cast uses. This matters for byte/short/long-backed enums: leaving the zero as i32
            // would make the LLVM comparison operands disagree even though the frontend accepted
            // them.
            const auto isEnumTypeKey = [&](std::string_view typeKey) noexcept
            {
                if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
                const std::string_view identity = typeKey.substr(5);
                return std::any_of(
                    nameLookup_->namedTypes().begin(),
                    nameLookup_->namedTypes().end(),
                    [&](const NamedTypeDeclaration& type)
                    {
                        return type.kind == NamedTypeDeclarationKind::Enum &&
                            type.identity.key() == identity;
                    });
            };
            const auto isZeroIntegerLiteral = [](const ast::AstNode* syntax) noexcept
            {
                return syntax != nullptr &&
                    syntax->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
                    syntax->integerValue() == 0;
            };
            if (expression->tokenKind() == lexer::TokenKind::EqualEqual ||
                expression->tokenKind() == lexer::TokenKind::BangEqual)
            {
                if (isEnumTypeKey(left->typeKey) && isZeroIntegerLiteral(expression->second()))
                {
                    auto converted = makeNode(
                        ProjectOopHirNodeKind::Conversion, expression->second(), left->typeKey);
                    converted->children.push_back(std::move(right));
                    right = std::move(converted);
                }
                else if (isEnumTypeKey(right->typeKey) && isZeroIntegerLiteral(expression->first()))
                {
                    auto converted = makeNode(
                        ProjectOopHirNodeKind::Conversion, expression->first(), right->typeKey);
                    converted->children.push_back(std::move(left));
                    left = std::move(converted);
                }
            }
            std::optional<std::string> numericOperandType;
            if (ProjectTypeConversion::isNumeric(resultType))
            {
                numericOperandType = resultType;
            }
            else if (isComparison(expression->tokenKind()) &&
                ProjectTypeConversion::isNumeric(left->typeKey) &&
                ProjectTypeConversion::isNumeric(right->typeKey))
            {
                const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
                const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
                numericOperandType = ProjectTypeConversion::commonNumericType(
                    left->typeKey,
                    right->typeKey,
                    leftType != nullptr ? leftType->integerConstant : std::nullopt,
                    rightType != nullptr ? rightType->integerConstant : std::nullopt);
            }
            if (numericOperandType.has_value())
            {
                const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
                const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
                const ProjectImplicitConversion leftConversion = ProjectTypeConversion::classifyImplicit(
                    left->typeKey,
                    *numericOperandType,
                    leftType != nullptr ? leftType->integerConstant : std::nullopt);
                const ProjectImplicitConversion rightConversion = ProjectTypeConversion::classifyImplicit(
                    right->typeKey,
                    *numericOperandType,
                    rightType != nullptr ? rightType->integerConstant : std::nullopt);
                if (leftConversion.exists() && leftConversion.kind != ProjectConversionKind::Identity)
                {
                    left = wrapImplicitConversion(std::move(left), leftConversion, expression->first());
                }
                if (rightConversion.exists() && rightConversion.kind != ProjectConversionKind::Identity)
                {
                    right = wrapImplicitConversion(std::move(right), rightConversion, expression->second());
                }
            }
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            return node;
        }

        case ast::AstNodeKind::IndexExpression:
        case ast::AstNodeKind::NullConditionalIndexExpression:
        {
            if (const MethodRangeSliceBinding* range = rangeSliceBinding(expression);
                range != nullptr)
            {
                if (addressContext)
                {
                    error = "T01-F04 a range slice is a value and cannot be used as assignable storage";
                    return nullptr;
                }
                if (memberModel_ == nullptr ||
                    range->lengthMemberOrdinal >= memberModel_->members().size() ||
                    range->sliceMemberOrdinal >= memberModel_->members().size())
                {
                    error = "T01-F04 range protocol lost its selected Length/Slice members";
                    return nullptr;
                }
                auto receiver = lowerReceiver(
                    range->receiverSyntax, range->receiverTypeKey, memberOrdinal, error);
                if (receiver == nullptr) return nullptr;

                const auto lowerEndpoint = [&](
                    const ast::AstNode* syntax,
                    const ProjectImplicitConversion& conversion)
                    -> std::unique_ptr<ProjectOopHirNode>
                {
                    if (syntax == nullptr)
                    {
                        auto zero = makeNode(
                            ProjectOopHirNodeKind::IntegerConstant,
                            expression,
                            "builtin:int");
                        zero->integerValue = 0;
                        return zero;
                    }
                    auto endpoint = lowerExpression(syntax, memberOrdinal, false, error);
                    if (endpoint == nullptr) return nullptr;
                    if (conversion.exists() &&
                        conversion.kind != ProjectConversionKind::Identity)
                    {
                        endpoint = wrapImplicitConversion(
                            std::move(endpoint), conversion, syntax);
                    }
                    return endpoint;
                };

                auto start = lowerEndpoint(range->startSyntax, range->startConversion);
                auto end = lowerEndpoint(range->endSyntax, range->endConversion);
                if (start == nullptr || end == nullptr) return nullptr;

                const ClassMemberSymbol& sliceMember =
                    memberModel_->members()[range->sliceMemberOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::RangeSlice,
                    expression,
                    range->resultTypeKey);
                node->elementTypeKey = range->receiverTypeKey;
                node->memberOrdinal = range->sliceMemberOrdinal;
                node->secondaryMemberOrdinal = range->lengthMemberOrdinal;
                node->referenceResult = sliceMember.isReferenceReturn;
                node->integerValue =
                    (range->hasStart ? 1U : 0U) |
                    (range->startFromEnd ? 2U : 0U) |
                    (range->hasEnd ? 4U : 0U) |
                    (range->endFromEnd ? 8U : 0U) |
                    (range->boundsStaticallyProven ? 16U : 0U);
                node->argumentCount = 2;
                node->argumentParameterOrdinals = {0U, 1U};
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(start));
                node->children.push_back(std::move(end));
                return node;
            }
            const MethodCoreBinding* core = coreBinding(expression);
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayElement ||
                 core->kind == MethodCoreOperationKind::RangeArrayElement ||
                 core->kind == MethodCoreOperationKind::StringElement))
            {
                if (core->kind == MethodCoreOperationKind::StringElement && addressContext)
                {
                    error = "Stage 41 strings are immutable; string elements are not assignable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                std::vector<const ast::AstNode*> indexSyntaxes;
                if (!expression->children().empty())
                    indexSyntaxes.assign(expression->children().begin(), expression->children().end());
                else indexSyntaxes.push_back(expression->second());
                ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::StringElementValue;
                if (core->kind == MethodCoreOperationKind::ArrayElement)
                {
                    kind = addressContext
                        ? ProjectOopHirNodeKind::ArrayElementAddress
                        : ProjectOopHirNodeKind::ArrayElementValue;
                }
                else if (core->kind == MethodCoreOperationKind::RangeArrayElement)
                {
                    kind = addressContext
                        ? ProjectOopHirNodeKind::RangeArrayElementAddress
                        : ProjectOopHirNodeKind::RangeArrayElementValue;
                }
                auto node = makeNode(kind, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->nativeArrayRank = core->kind == MethodCoreOperationKind::RangeArrayElement
                    ? indexSyntaxes.size() : 1U;
                node->children.push_back(std::move(receiver));
                for (std::size_t dimension = 0; dimension < indexSyntaxes.size(); ++dimension)
                {
                    const ast::AstNode* syntax = indexSyntaxes[dimension];
                    const bool fromEnd = syntax != nullptr &&
                        syntax->kind() == ast::AstNodeKind::FromEndExpression;
                    if (dimension == 0U) node->booleanValue = fromEnd;
                    if (fromEnd && dimension < 64U)
                        node->integerValue |= (std::uint64_t{1} << dimension);
                    auto index = lowerExpression(
                        fromEnd ? syntax->first() : syntax, memberOrdinal, false, error);
                    if (index == nullptr) return nullptr;
                    node->children.push_back(std::move(index));
                }
                return node;
            }
            // NS304-T10-F05: `receiver?[index]` — load condicional de elemento de array nativo.
            if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalArrayElement)
            {
                if (addressContext)
                {
                    error = "T10-F05 null-conditional array element is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                const ast::AstNode* indexSyntax = !expression->children().empty()
                    ? expression->children().front() : expression->second();
                auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                if (index == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalArrayLoad, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(index));
                return node;
            }
            // NS304-T10-F05: `receiver?[index]` sobre un indexador de usuario — null-check +
            // llamada al get_Item (receptor evaluado una vez por lowerResolvedCall).
            if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalUserIndex)
            {
                if (addressContext)
                {
                    error = "T10-F05 null-conditional user index is not addressable";
                    return nullptr;
                }
                const MethodCallBinding* indexerCall = callBinding(expression);
                if (indexerCall == nullptr ||
                    indexerCall->resolution != MethodCallResolutionKind::Resolved ||
                    !indexerCall->selectedMemberOrdinal.has_value())
                {
                    error = "T10-F05 null-conditional user index has no resolved get_Item accessor";
                    return nullptr;
                }
                std::vector<const ast::AstNode*> arguments;
                if (!expression->children().empty())
                    arguments.assign(expression->children().begin(), expression->children().end());
                else if (expression->second() != nullptr)
                    arguments.push_back(expression->second());
                return lowerResolvedCall(indexerCall, arguments, nullptr, true);
            }
            // C2.1 "fixed arrays": inline, unmanaged element access -- `fixedArrayField[i]` and
            // `rawPointer[i]`. The only difference between the two is how the BASE ADDRESS is
            // obtained, and it is a real difference: a fixed array IS its storage, so its address
            // comes from lowering the base in address context; a pointer HOLDS an address, so its
            // address comes from lowering the base as an ordinary value. Everything after that --
            // the scaled GEP, the load, the store -- is identical, which is why one node covers
            // both.
            if (core != nullptr && core->kind == MethodCoreOperationKind::RawElement)
            {
                const ast::AstNode* baseSyntax = expression->first();
                const MethodExpressionTypeBinding* baseType = expressionType(baseSyntax);
                const bool baseIsFixedArray = baseType != nullptr &&
                    ProjectMemberModel::fixedArrayElementTypeKey(baseType->typeKey).has_value();
                const bool baseIsFlexibleArray = baseType != nullptr &&
                    ProjectMemberModel::flexibleArrayElementTypeKey(baseType->typeKey).has_value();
                auto base = lowerExpression(
                    baseSyntax, memberOrdinal, baseIsFixedArray || baseIsFlexibleArray, error);
                if (base == nullptr) return nullptr;
                std::vector<const ast::AstNode*> indices;
                if (!expression->children().empty())
                    indices.assign(expression->children().begin(), expression->children().end());
                else if (expression->second() != nullptr)
                    indices.push_back(expression->second());

                const MethodVariableArrayIndexBinding* variableArrayIndex =
                    methodBodyModel_ != nullptr
                        ? methodBodyModel_->variableArrayIndexBinding(expression)
                        : nullptr;
                if (variableArrayIndex != nullptr)
                {
                    if (indices.size() != variableArrayIndex->extentLocalOrdinals.size())
                    {
                        error = "multidimensional VLA index lost its semantic rank";
                        return nullptr;
                    }
                    std::unique_ptr<ProjectOopHirNode> linearIndex;
                    for (std::size_t dimension = 0; dimension < indices.size(); ++dimension)
                    {
                        auto index = lowerExpression(indices[dimension], memberOrdinal, false, error);
                        if (index == nullptr) return nullptr;
                        if (index->typeKey != "builtin:nuint")
                        {
                            auto converted = makeNode(
                                ProjectOopHirNodeKind::Conversion, indices[dimension], "builtin:nuint");
                            converted->children.push_back(std::move(index));
                            index = std::move(converted);
                        }
                        if (linearIndex == nullptr)
                        {
                            linearIndex = std::move(index);
                            continue;
                        }

                        auto extent = makeNode(
                            ProjectOopHirNodeKind::LocalValue,
                            expression,
                            methodBodyModel_->locals()[
                                variableArrayIndex->extentLocalOrdinals[dimension]].typeKey);
                        extent->localOrdinal =
                            variableArrayIndex->extentLocalOrdinals[dimension];
                        if (extent->typeKey != "builtin:nuint")
                        {
                            auto convertedExtent = makeNode(
                                ProjectOopHirNodeKind::Conversion, expression, "builtin:nuint");
                            convertedExtent->children.push_back(std::move(extent));
                            extent = std::move(convertedExtent);
                        }
                        auto rowOffset = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
                        rowOffset->operation = lexer::TokenKind::Star;
                        rowOffset->children.push_back(std::move(linearIndex));
                        rowOffset->children.push_back(std::move(extent));
                        auto sum = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
                        sum->operation = lexer::TokenKind::Plus;
                        sum->children.push_back(std::move(rowOffset));
                        sum->children.push_back(std::move(index));
                        linearIndex = std::move(sum);
                    }

                    auto address = makeNode(
                        ProjectOopHirNodeKind::RawElementAddress,
                        expression,
                        core->elementTypeKey);
                    address->elementTypeKey = core->elementTypeKey;
                    address->children.push_back(std::move(base));
                    address->children.push_back(std::move(linearIndex));
                    if (addressContext) return address;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad,
                        expression,
                        core->resultTypeKey);
                    value->children.push_back(std::move(address));
                    return value;
                }

                std::string indexedType = baseType != nullptr ? baseType->typeKey : std::string();
                std::unique_ptr<ProjectOopHirNode> address;
                for (const ast::AstNode* indexSyntax : indices)
                {
                    const std::optional<std::string> fixedElement = baseIsFixedArray
                        ? ProjectMemberModel::fixedArrayElementTypeKey(indexedType)
                        : std::optional<std::string>();
                    const std::optional<std::string> flexibleElement = baseIsFlexibleArray
                        ? ProjectMemberModel::flexibleArrayElementTypeKey(indexedType)
                        : std::optional<std::string>();
                    const std::string elementType = fixedElement.has_value()
                        ? *fixedElement
                        : flexibleElement.has_value() ? *flexibleElement : core->elementTypeKey;
                    auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                    if (index == nullptr) return nullptr;
                    address = makeNode(
                        ProjectOopHirNodeKind::RawElementAddress, expression, elementType);
                    address->elementTypeKey = elementType;
                    // NS304-T13-F06: a flexible-array index bounds-checks against its `Length`
                    // member named by [TrailingStorage(nameof(Length), ...)]. Resolve its byte
                    // offset here so the raw element address can emit the runtime guard.
                    if (baseIsFlexibleArray && metaprogrammingModel_ != nullptr &&
                        baseSyntax != nullptr && baseSyntax->first() != nullptr &&
                        inheritanceModel_ != nullptr && memberModel_ != nullptr &&
                        objectLayoutModel_ != nullptr)
                    {
                        const MethodExpressionTypeBinding* receiverType =
                            expressionType(baseSyntax->first());
                        if (receiverType != nullptr)
                        {
                            const std::optional<std::size_t> structOrdinal =
                                inheritanceModel_->typeOrdinalForTypeKey(receiverType->typeKey);
                            if (structOrdinal.has_value())
                            {
                                for (const StaticAttributeInfo& attribute :
                                     metaprogrammingModel_->attributes())
                                {
                                    if (attribute.identity != "TrailingStorage" ||
                                        attribute.constantArguments.empty() ||
                                        !attribute.typeOrdinal.has_value() ||
                                        attribute.typeOrdinal != structOrdinal)
                                    {
                                        continue;
                                    }
                                    const std::span<const std::size_t> lengthMembers =
                                        memberModel_->findMembers(
                                            *structOrdinal, attribute.constantArguments[0]);
                                    if (!lengthMembers.empty() &&
                                        attribute.memberOrdinal.has_value())
                                    {
                                        const ObjectFieldLayout* lengthLayout =
                                            objectLayoutModel_->fieldLayoutForMember(
                                                lengthMembers.front());
                                        const ObjectFieldLayout* dataLayout =
                                            objectLayoutModel_->fieldLayoutForMember(
                                                *attribute.memberOrdinal);
                                        if (lengthLayout != nullptr && dataLayout != nullptr)
                                        {
                                            address->hasTrailingLengthBounds = true;
                                            address->trailingLengthRelativeOffset =
                                                static_cast<std::int64_t>(lengthLayout->offset) -
                                                static_cast<std::int64_t>(dataLayout->offset);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    address->children.push_back(std::move(base));
                    address->children.push_back(std::move(index));
                    base = std::move(address);
                    indexedType = elementType;
                }
                if (base == nullptr)
                {
                    error = "fixed-array/raw-pointer index expression has no index";
                    return nullptr;
                }
                if (addressContext) return base;
                auto value = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, core->resultTypeKey);
                value->children.push_back(std::move(base));
                return value;
            }

            const MethodCallBinding* indexerCall = callBinding(expression);
            if (indexerCall != nullptr && indexerCall->resolution == MethodCallResolutionKind::Resolved &&
                indexerCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *indexerCall->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*indexerCall->selectedMemberOrdinal].isIndexerAccessor &&
                memberModel_->members()[*indexerCall->selectedMemberOrdinal].isGetter)
            {
                const ClassMemberSymbol& getter =
                    memberModel_->members()[*indexerCall->selectedMemberOrdinal];
                if (addressContext && !getter.isReferenceReturn)
                {
                    error = "Stage 38 indexers are semantic accessor calls and are not directly addressable";
                    return nullptr;
                }
                std::vector<const ast::AstNode*> arguments;
                if (!expression->children().empty())
                {
                    arguments.insert(arguments.end(), expression->children().begin(), expression->children().end());
                }
                else if (expression->second() != nullptr)
                {
                    arguments.push_back(expression->second());
                }
                return lowerResolvedCall(indexerCall, arguments);
            }
            error = "Stage 38 OOP lowering requires class/interface index expressions to resolve to an indexer accessor";
            return nullptr;
        }

        case ast::AstNodeKind::AssignmentExpression:
        {
            if (expression->tokenKind() == lexer::TokenKind::QuestionQuestionEqual)
            {
                if (addressContext)
                {
                    error = "the result of null-coalescing assignment is a value, not addressable storage";
                    return nullptr;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "T01-F03 null-coalescing assignment target has no semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                auto fallback = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (address == nullptr || fallback == nullptr) return nullptr;
                const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                const ProjectImplicitConversion conversion = classifyImplicit(
                    fallback->typeKey,
                    targetType->typeKey,
                    rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                if (!conversion.exists())
                {
                    error = "T01-F03 null-coalescing assignment RHS cannot convert from '" +
                        fallback->typeKey + "' to '" + targetType->typeKey + "'";
                    return nullptr;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                    fallback = wrapImplicitConversion(
                        std::move(fallback), conversion, expression->second());
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullCoalescingAssignment,
                    expression,
                    targetType->typeKey);
                node->children.push_back(std::move(address));
                node->children.push_back(std::move(fallback));
                return node;
            }

            if (expression->tokenKind() == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::TupleExpression)
            {
                if (addressContext)
                {
                    error = "tuple deconstruction result is a value, not addressable storage";
                    return nullptr;
                }
                const MethodDeconstructionBinding* binding =
                    methodBodyModel_->deconstructionBinding(expression);
                if (binding == nullptr)
                {
                    error = "T02-F03 deconstruction assignment has no semantic binding";
                    return nullptr;
                }
                auto source = lowerDeconstructionSource(
                    expression, expression->second(), memberOrdinal, error);
                if (source == nullptr) return nullptr;
                const std::optional<std::vector<std::string>> sourceElements =
                    ProjectTypeConversion::tupleElementTypeKeys(binding->tupleTypeKey);
                if (!sourceElements.has_value() || objectLayoutModel_ == nullptr ||
                    nameLookup_ == nullptr)
                {
                    error = "T02-F03 deconstruction source has no tuple layout";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::TupleDeconstruct, expression, binding->tupleTypeKey);
                node->children.push_back(std::move(source));
                std::function<bool(const ast::AstNode*, const std::string&, std::uint64_t)>
                    appendTarget;
                appendTarget = [&](const ast::AstNode* targetSyntax,
                                   const std::string& sourceTypeKey,
                                   std::uint64_t byteOffset) -> bool
                {
                    if (targetSyntax == nullptr) return false;
                    if (targetSyntax->kind() == ast::AstNodeKind::TupleExpression)
                    {
                        const std::optional<std::vector<std::string>> nestedElements =
                            ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
                        const std::optional<language::AggregateLayout> nestedLayout =
                            objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, sourceTypeKey);
                        if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                            nestedElements->size() != targetSyntax->children().size() ||
                            nestedLayout->offsets.size() != nestedElements->size())
                        {
                            error = "T02-F03 nested assignment target lost its tuple layout";
                            return false;
                        }
                        for (std::size_t index = 0; index < nestedElements->size(); ++index)
                            if (!appendTarget(
                                    targetSyntax->children()[index], (*nestedElements)[index],
                                    byteOffset + nestedLayout->offsets[index])) return false;
                        return true;
                    }
                    if (targetSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                        targetSyntax->text() == "_") return true;
                    const MethodExpressionTypeBinding* targetType = expressionType(targetSyntax);
                    if (targetType == nullptr)
                    {
                        error = "T02-F03 deconstruction target has no semantic type";
                        return false;
                    }
                    auto address = lowerExpression(targetSyntax, memberOrdinal, true, error);
                    if (address == nullptr) return false;
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        sourceTypeKey, targetType->typeKey, std::nullopt);
                    if (!conversion.exists())
                    {
                        error = "T02-F03 deconstruction lost its validated element conversion";
                        return false;
                    }
                    auto element = makeNode(
                        ProjectOopHirNodeKind::TupleDeconstructElement,
                        targetSyntax,
                        targetType->typeKey);
                    element->elementTypeKey = sourceTypeKey;
                    element->integerValue = byteOffset;
                    element->conversion = conversion;
                    element->children.push_back(std::move(address));
                    node->children.push_back(std::move(element));
                    return true;
                };
                if (!appendTarget(expression->first(), binding->tupleTypeKey, 0)) return nullptr;
                return node;
            }

            const MethodEventBinding* stage42Event = eventBinding(expression);
            if (stage42Event != nullptr &&
                (stage42Event->kind == MethodEventOperationKind::Add ||
                 stage42Event->kind == MethodEventOperationKind::Remove))
            {
                if (addressContext)
                {
                    error = "Stage 42 event subscription expression is not addressable";
                    return nullptr;
                }
                if (stage42Event->customAccessorMemberOrdinal.has_value())
                {
                    // NS304-T09-F08: custom accessors -> instancia una llamada a add_/remove_.
                    auto node = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
                    node->memberOrdinal = *stage42Event->customAccessorMemberOrdinal;
                    if (!stage42Event->staticEvent)
                    {
                        auto receiver = lowerReceiver(
                            stage42Event->receiverSyntax, stage42Event->receiverTypeKey, memberOrdinal, error);
                        if (receiver == nullptr) return nullptr;
                        node->children.push_back(std::move(receiver));
                    }
                    auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
                    if (handler == nullptr) return nullptr;
                    if (!stage42Event->argumentConversions.empty() &&
                        stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
                    {
                        handler = wrapImplicitConversion(
                            std::move(handler), stage42Event->argumentConversions.front(), expression->second());
                    }
                    node->children.push_back(std::move(handler));
                    node->argumentParameterOrdinals.push_back(0);
                    node->argumentTypeKeys.push_back(stage42Event->delegateTypeKey);
                    return node;
                }
                auto backingAddress = lowerEventBacking(stage42Event, true);
                auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (backingAddress == nullptr || handler == nullptr) return nullptr;
                if (!stage42Event->argumentConversions.empty() &&
                    stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
                {
                    handler = wrapImplicitConversion(
                        std::move(handler), stage42Event->argumentConversions.front(), expression->second());
                }
                auto node = makeNode(
                    stage42Event->kind == MethodEventOperationKind::Add
                        ? ProjectOopHirNodeKind::EventAdd : ProjectOopHirNodeKind::EventRemove,
                    expression, "builtin:void");
                node->elementTypeKey = stage42Event->delegateTypeKey;
                node->memberOrdinal = stage42Event->backingMemberOrdinal;
                node->children.push_back(std::move(backingAddress));
                node->children.push_back(std::move(handler));
                return node;
            }

            const MethodCallBinding* setterCall = callBinding(expression);
            if (setterCall != nullptr && setterCall->resolution == MethodCallResolutionKind::Resolved &&
                setterCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *setterCall->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& setter = memberModel_->members()[*setterCall->selectedMemberOrdinal];
                if ((setter.isPropertyAccessor || setter.isIndexerAccessor) && setter.isSetter)
                {
                    std::vector<const ast::AstNode*> arguments;
                    if (setter.isIndexerAccessor && expression->first() != nullptr &&
                        expression->first()->kind() == ast::AstNodeKind::IndexExpression)
                    {
                        const ast::AstNode* indexExpression = expression->first();
                        if (!indexExpression->children().empty())
                        {
                            arguments.insert(
                                arguments.end(), indexExpression->children().begin(), indexExpression->children().end());
                        }
                        else if (indexExpression->second() != nullptr)
                        {
                            arguments.push_back(indexExpression->second());
                        }
                    }
                    arguments.push_back(expression->second());

                    // ID 82. `p.X op= e` es `set_X(p, get_X(p) op e)`. El valor que recibe el
                    // setter se arma AQUI, no se baja de la fuente.
                    //
                    // El operando izquierdo de esa operacion es la llamada al GETTER, y se obtiene
                    // bajando `expression->first()` en contexto de VALOR: los casos
                    // IdentifierExpression/MemberExpression e IndexExpression de esta misma
                    // funcion ya saben convertir ese nodo en la llamada al getter, con su receptor
                    // y sus argumentos de indice. No hay una segunda ruta de resolucion de
                    // accesores aqui; se reusa la unica que hay.
                    //
                    // EL RECEPTOR Y LOS INDICES SE BAJAN DOS VECES -- una dentro de la llamada al
                    // getter y otra para la del setter -- porque el arbol es de `unique_ptr` y un
                    // nodo no se comparte entre dos padres. Eso NO es un descuido: el modelo
                    // semantico ya rechazo toda forma en la que la segunda bajada pudiera dar algo
                    // distinto (isAccessorDestinationReevaluable), asi que lo que llega hasta aqui
                    // son locals, parametros, `this` y constantes. La diferencia con un destino
                    // CON direccion es que aquel si tiene una direccion que bajar una sola vez, y
                    // por eso ese caso lo resuelve `CompoundStore` unas lineas mas abajo.
                    //
                    // El operando derecho se baja UNA sola vez, dentro del `Binary`, DESPUES de la
                    // llamada al getter -- el mismo orden que C#: receptor, indices, getter, lado
                    // derecho, operacion, setter.
                    //
                    // La operacion ocurre entera en el tipo del destino, y la conversion del lado
                    // derecho se clasifica con la MISMA llamada que usa `CompoundStore`. El modelo
                    // semantico ya exigio que exista, asi que si no existe aqui es una discrepancia
                    // entre capas y se reporta como tal, no se ignora.
                    if (expression->tokenKind() != lexer::TokenKind::Equal)
                    {
                        if (addressContext)
                        {
                            error = "the result of a compound assignment is a value, not addressable storage";
                            return nullptr;
                        }
                        const lexer::TokenKind baseOperator =
                            lexer::compoundAssignmentBaseOperator(expression->tokenKind());
                        if (baseOperator == lexer::TokenKind::Invalid)
                        {
                            error = "unsupported compound assignment operator on a property/indexer";
                            return nullptr;
                        }
                        auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
                        if (current == nullptr) return nullptr;
                        auto operand = lowerExpression(expression->second(), memberOrdinal, false, error);
                        if (operand == nullptr) return nullptr;
                        const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                        const ProjectImplicitConversion conversion = classifyImplicit(
                            operand->typeKey,
                            current->typeKey,
                            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                        const bool shift = baseOperator == lexer::TokenKind::LessLess ||
                            baseOperator == lexer::TokenKind::GreaterGreater;
                        if (!shift && !conversion.exists())
                        {
                            error = "compound assignment to a property/indexer cannot convert its "
                                "right-hand side from '" + operand->typeKey + "' to '" +
                                current->typeKey + "'";
                            return nullptr;
                        }
                        if (!shift && conversion.kind != ProjectConversionKind::Identity)
                        {
                            operand = wrapImplicitConversion(
                                std::move(operand), conversion, expression->second());
                        }
                        auto combined = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, current->typeKey);
                        combined->operation = baseOperator;
                        combined->children.push_back(std::move(current));
                        combined->children.push_back(std::move(operand));
                        auto setterNode = lowerResolvedCall(
                            setterCall, arguments, std::move(combined));
                        if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
                        return setterNode;
                    }
                    auto setterNode = lowerResolvedCall(setterCall, arguments);
                    if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
                    return setterNode;
                }
            }

            // NS304-T08-F02: an extension-property assignment is a synthetic namespace setter call
            // recorded under the assignment node (`set_Name(receiver, value)`). Emit it as a
            // StaticCall whose first argument is the receiver and whose last argument is the RHS.
            const NamespaceFunctionCallBinding* namespaceSetter =
                namespaceFunctionCallBinding(expression);
            if (namespaceSetter != nullptr &&
                namespaceSetter->resolution == MethodCallResolutionKind::Resolved &&
                namespaceSetter->selectedFunctionOrdinal.has_value() &&
                namespaceFunctionModel_ != nullptr &&
                *namespaceSetter->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
            {
                if (addressContext)
                {
                    error = "the result of an extension-property assignment is a value, not addressable storage";
                    return nullptr;
                }
                const std::size_t functionOrdinal = *namespaceSetter->selectedFunctionOrdinal;
                const NamespaceFunctionSymbol& target =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::StaticCall, expression, namespaceSetter->resultTypeKey);
                node->namespaceFunctionOrdinal = functionOrdinal;
                node->stringValue = target.mangledName;
                if (namespaceSetter->forwardedReceiver)
                {
                    if (namespaceSetter->receiverSyntax == nullptr || target.parameters.empty())
                    {
                        error = "extension setter lost its synthetic receiver contract";
                        return nullptr;
                    }
                    auto receiver = lowerExpression(
                        namespaceSetter->receiverSyntax, memberOrdinal, false, error);
                    if (receiver == nullptr) return nullptr;
                    if (!namespaceSetter->selectedConversions.empty() &&
                        namespaceSetter->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        receiver = wrapImplicitConversion(
                            std::move(receiver), namespaceSetter->selectedConversions.front(),
                            namespaceSetter->receiverSyntax);
                    }
                    node->children.push_back(std::move(receiver));
                    node->argumentParameterOrdinals.push_back(0);
                    node->argumentTypeKeys.push_back(target.parameters[0].typeKey);
                }
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                const std::size_t valueParameterOrdinal = namespaceSetter->forwardedReceiver ? 1 : 0;
                if (valueParameterOrdinal >= target.parameters.size())
                {
                    error = "extension setter has no value parameter";
                    return nullptr;
                }
                if (valueParameterOrdinal < namespaceSetter->selectedConversions.size() &&
                    namespaceSetter->selectedConversions[valueParameterOrdinal].kind !=
                        ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(
                        std::move(value),
                        namespaceSetter->selectedConversions[valueParameterOrdinal],
                        expression->second());
                }
                node->children.push_back(std::move(value));
                node->argumentParameterOrdinals.push_back(valueParameterOrdinal);
                node->argumentTypeKeys.push_back(target.parameters[valueParameterOrdinal].typeKey);
                node->returnsAssignedValue = true;
                return node;
            }

            // NS304-T10-F05/F06: the receiver remains the first child so LLVM can branch before
            // lowering the RHS. booleanValue carries the semantic owning-destination contract.
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
            {
                const MethodNameBinding* binding = nameBinding(expression->first());
                if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
                    binding->kind != MethodNameBindingKind::ExplicitObjectField)
                {
                    error = "T10-F05 null-conditional assignment has no unique instance-field binding";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalFieldStore, expression, resultType);
                node->operation = expression->tokenKind();
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
                node->memberOrdinal = binding->candidateMemberOrdinals.front();
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(value));
                return node;
            }

            // NS304-T10-F05: receiver, index, and value remain separate children so only the
            // receiver is evaluated before the null branch.
            if (expression->tokenKind() == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
            {
                const MethodCoreBinding* core = coreBinding(expression->first());
                if (core == nullptr ||
                    core->kind != MethodCoreOperationKind::NullConditionalArrayElement)
                {
                    error = "T10-F05 null-conditional index store has no native-array element binding";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                const ast::AstNode* indexSyntax = !expression->first()->children().empty()
                    ? expression->first()->children().front() : expression->first()->second();
                auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                if (index == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalArrayStore, expression, resultType);
                node->elementTypeKey = core->elementTypeKey;
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(index));
                node->children.push_back(std::move(value));
                return node;
            }

            auto node = makeNode(ProjectOopHirNodeKind::Assignment, expression, resultType);
            node->operation = expression->tokenKind();
            if (node->operation == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const MethodNameBinding* targetBinding = nameBinding(expression->first());
                if (targetBinding != nullptr && targetBinding->localOrdinal.has_value() &&
                    *targetBinding->localOrdinal < methodBodyModel_->locals().size() &&
                    (methodBodyModel_->locals()[*targetBinding->localOrdinal].isRaii ||
                     methodBodyModel_->locals()[*targetBinding->localOrdinal].requiresValueDrop))
                {
                    node->booleanValue = true;
                    node->localOrdinal = *targetBinding->localOrdinal;
                }
            }
            // Owning replacement is a semantic property of the destination, not an AST pattern.
            // Fields and elements of owned arrays both lower to an address-based store; the
            // semantic binding records which stores own their current value so this layer can
            // request the existing release-before-store path without rediscovering declarations.
            if (!node->booleanValue)
            {
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
            }
            // T03-F01/T06-F08: a move-only aggregate owns its current active payload even when
            // the replacement is a freshly returned value rather than an explicit move(local).
            // Mark every ordinary store of a drop-bearing value so the backend destroys the
            // displaced value after evaluating the RHS and before writing the replacement.
            if (!node->booleanValue && node->operation == lexer::TokenKind::Equal &&
                memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(resultType))
            {
                node->booleanValue = true;
            }
            if (node->operation != lexer::TokenKind::Equal)
            {
                // ID 81. Los diez operadores compuestos. El destino se baja UNA VEZ en contexto de
                // direccion -- si se desazucarara a `x = x + e` se evaluaria dos veces y
                // `a[f()] += 1` llamaria a `f` dos veces.
                if (addressContext)
                {
                    error = "the result of a compound assignment is a value, not addressable storage";
                    return nullptr;
                }
                // T41: `raii string += value` is an ownership-aware replacement, not a numeric
                // CompoundStore. The RAII destination is bound specially by semantic analysis,
                // so its identifier does not need a separate expression-type record here; the
                // assignment result type is the authoritative resolved type.
                if (node->operation == lexer::TokenKind::PlusEqual &&
                    resultType == "builtin:string")
                {
                    const MethodNameBinding* targetBinding = nameBinding(expression->first());
                    if (targetBinding == nullptr || !targetBinding->localOrdinal.has_value())
                    {
                        error = "string += requires an identifier-backed RAII string destination";
                        return nullptr;
                    }
                    auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                    auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
                    auto suffix = lowerExpression(expression->second(), memberOrdinal, false, error);
                    if (address == nullptr || current == nullptr || suffix == nullptr) return nullptr;

                    auto concatenated = makeNode(
                        ProjectOopHirNodeKind::StringConcat, expression, "builtin:string");
                    concatenated->operation = lexer::TokenKind::Plus;
                    concatenated->children.push_back(std::move(current));
                    concatenated->children.push_back(std::move(suffix));

                    node->operation = lexer::TokenKind::Equal;
                    node->booleanValue = true;
                    node->localOrdinal = *targetBinding->localOrdinal;
                    node->children.push_back(std::move(address));
                    node->children.push_back(std::move(concatenated));
                    return node;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "Stage 34 compound assignment target has no resolved semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;

                auto compound = makeNode(
                    ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
                compound->operation = lexer::compoundAssignmentBaseOperator(node->operation);
                if (compound->operation == lexer::TokenKind::Invalid)
                {
                    error = "unsupported compound assignment operator";
                    return nullptr;
                }
                // El valor de `x += e` es el NUEVO, como en C#.
                compound->booleanValue = false;

                if (ProjectTypeConversion::isPointer(targetType->typeKey))
                {
                    compound->elementTypeKey =
                        targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
                }
                else
                {
                    // La operacion ocurre ENTERA en el tipo del destino. El modelo semantico ya
                    // exigio que el operando derecho convierta a ese tipo (ver analyzeAssignment),
                    // asi que aqui no hay ni un estrechamiento silencioso ni una segunda tabla de
                    // promociones distinta de la de `Binary`.
                    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey,
                        targetType->typeKey,
                        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                    const bool shift = compound->operation == lexer::TokenKind::LessLess ||
                        compound->operation == lexer::TokenKind::GreaterGreater;
                    if (!shift && !conversion.exists())
                    {
                        error = "compound assignment cannot convert its right-hand side from '" +
                            value->typeKey + "' to '" + targetType->typeKey + "'";
                        return nullptr;
                    }
                    if (!shift && conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(
                            std::move(value), conversion, expression->second());
                    }
                }
                compound->children.push_back(std::move(address));
                compound->children.push_back(std::move(value));
                return compound;
            }
            auto target = lowerExpression(expression->first(), memberOrdinal, true, error);
            auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (target == nullptr || value == nullptr)
            {
                return nullptr;
            }
            const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                value->typeKey,
                target->typeKey,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
            {
                value = wrapImplicitConversion(std::move(value), conversion, expression->second());
            }
            node->children.push_back(std::move(target));
            node->children.push_back(std::move(value));
            return node;
        }

        case ast::AstNodeKind::MoveExpression:
        {
            if (addressContext)
            {
                error = "Stage 49 move value is not addressable";
                return nullptr;
            }
            const MethodMoveBinding* move = moveBinding(expression);
            if (move == nullptr)
            {
                error = "Stage 49 move expression has no semantic move binding";
                return nullptr;
            }
            if (move->sourceIsArrayElement)
            {
                // Stage 52: move(items[i]) extracts an owned element out of native-array
                // backing storage (needed by Queue<T>.Dequeue()/Stack<T>.Pop()). Reuse the
                // existing IndexExpression address-lowering path (ArrayElementAddress) to
                // compute the slot's address, then wrap it in a dedicated node so LLVM
                // lowering can load the value and, only for reference-like elements,
                // null out the slot to prevent a later double-free/double-use. Value-typed
                // elements (int, bool, a plain struct, ...) have no ownership to transfer,
                // so the load degrades to a harmless plain read with no null-store.
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveArrayElement, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(address));
                return node;
            }
            if (move->sourceIsOwnedField)
            {
                // Bloque 1: move(holder.slot) / move(slot) -- take the owner OUT of an `owned`
                // field. Same address-shaped contract as the array-element case above: compute
                // the slot's address here, load-and-null in the backend. The null-store is what
                // makes the container's own discharge (destruction entry, overwrite release) see
                // an empty slot instead of double-freeing the value that just left.
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveOwnedField, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(address));
                return node;
            }
            if (move->sourceIsIteratorCurrent)
            {
                // NS304-T07-F04: move(it.Current) takes the move-only item out of the iterator's
                // item slot (field 2). The child is the iterator's ADDRESS; the backend loads the
                // item slot and clears it so the Drop never double-frees the transferred value.
                auto iterator = lowerExpression(expression->first()->first(), memberOrdinal, true, error);
                if (iterator == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveIteratorCurrent, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(iterator));
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::MoveValue, expression, move->typeKey);
            node->localOrdinal = move->sourceLocalOrdinal;
            node->parameterOrdinal = move->sourceParameterOrdinal;
            // Stage 52: whether the source is actually reference-like (needs the load+null-store
            // treatment) or a plain value type closed over a generic T -- see
            // MethodMoveBinding::clearSourceStorage's comment and the MoveValue LLVM
            // lowering case for the bug this fixes.
            node->booleanValue = move->clearSourceStorage;
            if (node->localOrdinal.has_value() == node->parameterOrdinal.has_value())
            {
                error = "Stage 50.8 move value must identify exactly one owner source";
                return nullptr;
            }
            return node;
        }

        case ast::AstNodeKind::ResultPropagationExpression:
        {
            if (addressContext)
            {
                error = "postfix '?' produces a value, not addressable storage";
                return nullptr;
            }
            const MethodResultPropagationBinding* binding =
                resultPropagationBinding(expression);
            const MethodStructuredExitBinding* exit = structuredExitBinding(expression);
            if (binding == nullptr || exit == nullptr ||
                exit->kind != MethodStructuredExitKind::Return)
            {
                error = "T07-F01 Result propagation has no complete semantic binding";
                return nullptr;
            }
            auto operand = lowerExpression(
                binding->operandSyntax, memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;

            auto node = makeNode(
                ProjectOopHirNodeKind::ResultPropagation, expression, resultType);
            node->resultSourceCarrierTypeKey = binding->sourceCarrierTypeKey;
            node->resultSourceErrorTypeKey = binding->sourceErrorTypeKey;
            node->resultReturnCarrierTypeKey = binding->returnCarrierTypeKey;
            node->resultReturnErrorTypeKey = binding->returnErrorTypeKey;
            node->resultSuccessTag = binding->sourceSuccessTag;
            node->resultErrorTag = binding->sourceErrorTag;
            node->resultReturnErrorTag = binding->returnErrorTag;
            node->resultSuccessTestMemberOrdinal =
                binding->sourceSuccessTestMemberOrdinal;
            node->resultSuccessExtractorMemberOrdinal =
                binding->sourceSuccessExtractorMemberOrdinal;
            node->resultErrorExtractorMemberOrdinal =
                binding->sourceErrorExtractorMemberOrdinal;
            node->resultErrorFactoryMemberOrdinal =
                binding->returnErrorFactoryMemberOrdinal;
            node->resultErrorConversionMemberOrdinal =
                binding->errorConversionMemberOrdinal;
            node->cleanupRegionOrdinals = exit->cleanupRegionOrdinals;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::CallExpression:
            return lowerCallExpression(expression, memberOrdinal, addressContext, error);

        // CC1 fase 4B -- `stackalloc T[n]`. Un alloca de arreglo y nada mas: sin cabecera de
        // longitud, sin comprobacion de rango, sin liberacion. Es el nivel crudo, igual que
        // `p[i]`, y la validacion de que el elemento puede vivir ahi ya la hizo el modelo
        // semantico (sin destructor y sin campos `owned`).
        case ast::AstNodeKind::StackallocExpression:
        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::CollectionExpression:
            return lowerArrayConstructionExpression(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::ObjectCreationExpression:
        case ast::AstNodeKind::PlacementConstructionExpression:
            return lowerObjectConstructionExpression(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::TupleExpression:
        case ast::AstNodeKind::AggregateInitializerExpression:
        case ast::AstNodeKind::WithExpression:
            return lowerAggregateOrLayoutExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::DeleteExpression:
        case ast::AstNodeKind::DestroyExpression:
            return lowerDestructionExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
        case ast::AstNodeKind::CastExpression:
            return lowerConversionExpression(
                expression, memberOrdinal, addressContext, error, typeBinding, resultType);

        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        case ast::AstNodeKind::OffsetofExpression:
            return lowerAggregateOrLayoutExpression(expression, memberOrdinal, error);
        // ID 80, CERRADA. Aqui habia un rechazo con mensaje propio para `p->campo`: era la ultima
        // forma de expresion con la mitad delantera del pipeline presente y la trasera ausente,
        // despues de sizeof, stackalloc, raw new y la aritmetica de punteros.
        //
        // Se cerro donde tocaba, que no era aqui: el parser convierte `p->x` en `(*p).x` (ver
        // Parser::parsePostfixExpression), asi que este nodo ya no se construye y este caso ya no
        // se alcanza. Anadir en su lugar un lowering propio habria significado una SEGUNDA ruta de
        // acceso a miembro que mantener de acuerdo con la de `.` -- que es como se abrio el hueco.
        //
        // NOTA sobre como se hizo el inventario que la encontro: un barrido de las 30 formas de
        // expresion del AST contra los `case` de esta funcion daba TAMBIEN `LambdaExpression`. Es
        // un falso positivo del barrido: las lambdas se bajan por binding ANTES del switch, no por
        // un `case`. Comprobado antes de escribirlo -- el barrido mide lo que mide, no lo que uno
        // quiere que mida.

        case ast::AstNodeKind::AwaitExpression:
            return lowerAwaitExpression(expression, memberOrdinal, error);

        default:
            error = "unsupported Stage 34 OOP expression kind: " +
                std::string(ast::astNodeKindName(expression->kind()));
            return nullptr;
    }
}


// NSC-LIMIT-008. Is this name actually a GETTER CALL?
//
// `source.Current` and `table[key]` are written like storage and parse like storage, but if the
// semantic model bound them to a property or indexer accessor they are calls: they RETURN a value,
// and that value has no home. Syntax cannot tell the two apart -- `point.X` is a field on one type
// and a property on another, and the difference is only known after member resolution.
//
// Two places need this answer and they used to disagree. lowerExpression asked it (the
// MemberExpression case, which rejects `addressContext` for a getter) while isNaturallyAddressable
// did not, and answered `true` for every MemberExpression. That disagreement is the whole defect:
// `source.Current.Read()` classified the getter as addressable, so lowerReceiver never entered its
// TemporaryAddress path and the struct method received the address of something that was never
// stored.
const MethodCallBinding* ProjectOopHirModel::accessorGetterBinding(
    const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr || memberModel_ == nullptr) return nullptr;
    switch (expression->kind())
    {
        // The three spellings a getter can wear. `Current` unqualified inside its own class is an
        // IdentifierExpression, `source.Current` a MemberExpression, `table[key]` an
        // IndexExpression -- and all three are calls once the model has resolved them.
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::NullConditionalMemberExpression:
        case ast::AstNodeKind::IndexExpression:
            break;
        default:
            return nullptr;
    }
    const MethodCallBinding* accessorCall = callBinding(expression);
    if (accessorCall == nullptr ||
        accessorCall->resolution != MethodCallResolutionKind::Resolved ||
        !accessorCall->selectedMemberOrdinal.has_value() ||
        *accessorCall->selectedMemberOrdinal >= memberModel_->members().size())
    {
        // No binding at all is the ORDINARY case, not an error: a real field, a real array element
        // and a real fixed-array element never produce one. They keep their own address.
        return nullptr;
    }
    const ClassMemberSymbol& selected = memberModel_->members()[*accessorCall->selectedMemberOrdinal];
    // PROPIEDAD *O* INDEXADOR. `isIndexerAccessor` es una bandera distinta de `isPropertyAccessor`,
    // no un caso suyo, y mirar solo la primera dejaba fuera `obj[i].MetodoDeStruct()`: el
    // IndexExpression entraba por el switch de arriba, resolvia a un getter de indexador, y este
    // filtro lo dejaba pasar como si fuera almacenamiento. Exactamente el mismo defecto que este
    // ID cierra, en la otra mitad de la sintaxis.
    if (!selected.isPropertyAccessor && !selected.isIndexerAccessor) return nullptr;
    if (!selected.isGetter) return nullptr;
    return accessorCall;
}

// C6 "struct members": does this expression denote storage that already exists? An identifier, a
// field of one, an element of one, and a dereference all do -- and a parenthesized form of any of
// them does too. A call, an operator, a conversion or a `new` does not: its result is a value with
// no home. Only the second group needs a temporary made for it.
//
// NSC-LIMIT-008 added the one case where syntax lies: a property or indexer getter is written like
// a field or an element and is a call. See accessorGetterBinding.
bool ProjectOopHirModel::isNaturallyAddressable(const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr) return false;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::IndexExpression:
        {
            // A real field or element is storage. An ordinary accessor returns a value with no
            // home, while a ref-returning accessor deliberately returns existing storage.
            const MethodCallBinding* accessor = accessorGetterBinding(expression);
            if (accessor == nullptr) return true;
            return accessor->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *accessor->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*accessor->selectedMemberOrdinal].isReferenceReturn;
        }
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::BaseExpression:
        case ast::AstNodeKind::MatchValueExpression:
            return true;
        case ast::AstNodeKind::UnaryExpression:
            // `*p` is storage; `-v` is a value.
            return expression->tokenKind() == lexer::TokenKind::Star;
        default:
            return false;
    }
}


}
