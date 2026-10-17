#include "nativesharp/Frontend/ProjectOopHirModel.h"

namespace nativesharp::frontend
{
const MethodNameBinding* ProjectOopHirModel::nameBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? nameBindingBySyntax_ : lookupSource_->nameBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodExpressionTypeBinding* ProjectOopHirModel::expressionType(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? expressionTypeBySyntax_ : lookupSource_->expressionTypeBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodCallBinding* ProjectOopHirModel::callBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? callBySyntax_ : lookupSource_->callBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const NamespaceFunctionCallBinding* ProjectOopHirModel::namespaceFunctionCallBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? namespaceFunctionCallBySyntax_
        : lookupSource_->namespaceFunctionCallBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodSwitchBinding* ProjectOopHirModel::switchBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? switchBindingBySyntax_ : lookupSource_->switchBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodRuntimeTypeBinding* ProjectOopHirModel::runtimeTypeBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? runtimeTypeBindingBySyntax_ : lookupSource_->runtimeTypeBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodLambdaBinding* ProjectOopHirModel::lambdaBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? lambdaBindingBySyntax_ : lookupSource_->lambdaBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodLambdaValueBinding* ProjectOopHirModel::lambdaValueBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? lambdaValueBindingBySyntax_ : lookupSource_->lambdaValueBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

std::optional<std::size_t> ProjectOopHirModel::currentLambdaThisCapture() const noexcept
{
    if (currentLambda_ == nullptr) return std::nullopt;
    for (std::size_t index = 0; index < currentLambda_->captures.size(); ++index)
    {
        if (currentLambda_->captures[index].kind == MethodLambdaCaptureKind::ThisReference)
            return index;
    }
    return std::nullopt;
}

const MethodDelegateBinding* ProjectOopHirModel::delegateBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? delegateBindingBySyntax_ : lookupSource_->delegateBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodEventBinding* ProjectOopHirModel::eventBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? eventBindingBySyntax_ : lookupSource_->eventBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodCoreBinding* ProjectOopHirModel::coreBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? coreBindingBySyntax_ : lookupSource_->coreBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodOptionalCarrierBinding* ProjectOopHirModel::optionalCarrierBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? optionalCarrierBindingBySyntax_ : lookupSource_->optionalCarrierBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodResultPropagationBinding* ProjectOopHirModel::resultPropagationBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? resultPropagationBindingBySyntax_ : lookupSource_->resultPropagationBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodRangeSliceBinding* ProjectOopHirModel::rangeSliceBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? rangeSliceBindingBySyntax_ : lookupSource_->rangeSliceBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodExceptionBinding* ProjectOopHirModel::exceptionBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? exceptionBindingBySyntax_ : lookupSource_->exceptionBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodCleanupRegion* ProjectOopHirModel::cleanupRegion(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? cleanupRegionBySyntax_ : lookupSource_->cleanupRegionBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodUsingBinding* ProjectOopHirModel::usingBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? usingBindingBySyntax_ : lookupSource_->usingBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodMoveBinding* ProjectOopHirModel::moveBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? moveBindingBySyntax_ : lookupSource_->moveBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodBorrowBinding* ProjectOopHirModel::borrowBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? borrowBindingBySyntax_ : lookupSource_->borrowBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodStructuredExitBinding* ProjectOopHirModel::structuredExitBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? structuredExitBySyntax_ : lookupSource_->structuredExitBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const ObjectConstructionBinding* ProjectOopHirModel::constructionBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? constructionBySyntax_ : lookupSource_->constructionBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodLocalSymbol* ProjectOopHirModel::localSymbol(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? localBySyntax_ : lookupSource_->localBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}
}
