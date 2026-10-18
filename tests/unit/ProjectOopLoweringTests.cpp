#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "../../src/Frontend/HIR/HirLoweringSupport.h"
#include "../../src/Frontend/MIR/MirConstantEvaluator.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopCodePartition.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess final
{
    static std::vector<std::uint8_t> coordinatorStaticFieldMask(
        std::vector<ProjectOopMirStaticField> fields)
    {
        ProjectOopMirModel model;
        model.staticFields_ = std::move(fields);
        return llvm_backend::projectOopStaticFieldDeclarationMask(model, {}, true, true);
    }

    static bool configureAggregateTypes(
        ProjectNameLookup& nameLookup,
        ProjectInheritanceModel& inheritanceModel,
        std::size_t depth,
        std::vector<std::string>& typeKeys)
    {
        typeKeys.reserve(depth);
        nameLookup.namedTypes_.reserve(depth);
        for (std::size_t ordinal = 0; ordinal < depth; ++ordinal)
        {
            NamedTypeDeclaration declaration{
                compilation::TypeIdentity(
                    "AggregateDepth", {}, {}, "S" + std::to_string(ordinal)),
                NamedTypeDeclarationKind::Struct,
                NamedTypeAccessibility::Internal,
                std::nullopt,
                false,
                false,
                false,
                false,
                false,
                0U,
                0U,
                FileId{},
                SourceRange{},
                nullptr,
                {}};
            nameLookup.namedTypes_.push_back(std::move(declaration));
            typeKeys.push_back(
                "type:" + nameLookup.namedTypes_.back().identity.key());
        }
        nameLookup.built_ = true;

        diagnostics::DiagnosticEngine diagnostics;
        return inheritanceModel.build(nameLookup, diagnostics);
    }

    static void configureAggregateLayouts(
        ProjectObjectLayoutModel& objectLayoutModel,
        const std::vector<std::string>& typeKeys)
    {
        const std::size_t depth = typeKeys.size();
        objectLayoutModel.structLayouts_.resize(depth);
        objectLayoutModel.structLayoutIndexByType_.resize(depth);
        std::uint64_t childSize = 4U;
        std::uint64_t childAlignment = 4U;
        for (std::size_t remaining = depth; remaining > 0; --remaining)
        {
            const std::size_t ordinal = remaining - 1U;
            StructValueLayout& layout = objectLayoutModel.structLayouts_[ordinal];
            layout.typeOrdinal = ordinal;
            layout.typeKey = typeKeys[ordinal];
            layout.alignment = 8U;
            layout.minimumAlignment = 8U;
            layout.hasExplicitFieldOffsets = true;

            ObjectFieldLayout field;
            field.memberOrdinal = ordinal;
            field.offset = 8U;
            field.size = childSize;
            field.alignment = childAlignment;
            field.typeKey = ordinal + 1U < depth
                ? typeKeys[ordinal + 1U]
                : "builtin:int";
            layout.fields.push_back(std::move(field));
            layout.size = (8U + childSize + 7U) & ~std::uint64_t{7U};
            childSize = layout.size;
            childAlignment = layout.alignment;
            objectLayoutModel.structLayoutIndexByType_[ordinal] = ordinal;
        }
        objectLayoutModel.built_ = true;
    }

    static bool configureAggregateLeafChain(
        ProjectOopHirModel& model,
        ProjectNameLookup& nameLookup,
        ProjectInheritanceModel& inheritanceModel,
        ProjectObjectLayoutModel& objectLayoutModel,
        std::size_t depth,
        std::vector<std::string>& typeKeys)
    {
        if (!configureAggregateTypes(nameLookup, inheritanceModel, depth, typeKeys))
        {
            return false;
        }
        configureAggregateLayouts(objectLayoutModel, typeKeys);

        model.nameLookup_ = &nameLookup;
        model.inheritanceModel_ = &inheritanceModel;
        model.objectLayoutModel_ = &objectLayoutModel;
        return true;
    }

    static bool appendAggregateLeaves(
        ProjectOopHirModel& model,
        const std::string& typeKey,
        std::uint64_t baseOffset,
        const ast::AstNode* syntax,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& leaves,
        std::string& error)
    {
        return model.appendAggregateLeaves(
            typeKey, baseOffset, syntax, leaves, error);
    }

    static void setAggregateRootFields(
        ProjectObjectLayoutModel& objectLayoutModel,
        std::vector<ObjectFieldLayout> fields)
    {
        objectLayoutModel.structLayouts_.front().fields = std::move(fields);
    }

    static void configureAggregateElementBindings(
        ProjectOopHirModel& model,
        ProjectMethodBodyModel& methodBodyModel,
        const std::vector<ast::AstNode*>& aggregates,
        const ast::AstNode* literal,
        const std::vector<std::string>& typeKeys,
        const MethodExpressionTypeBinding& literalType)
    {
        methodBodyModel.aggregateInitializerBindings_.reserve(aggregates.size());
        for (std::size_t ordinal = 0; ordinal < aggregates.size(); ++ordinal)
        {
            MethodAggregateInitializerEntryBinding entry;
            entry.syntax = ordinal + 1U < aggregates.size()
                ? aggregates[ordinal + 1U]
                : literal;
            entry.targetElementOrdinals.push_back(0U);

            MethodAggregateInitializerBinding binding;
            binding.syntax = aggregates[ordinal];
            binding.targetTypeKey = typeKeys[ordinal];
            binding.entries.push_back(std::move(entry));
            methodBodyModel.aggregateInitializerBindingBySyntax_[aggregates[ordinal]] =
                methodBodyModel.aggregateInitializerBindings_.size();
            methodBodyModel.aggregateInitializerBindings_.push_back(std::move(binding));
        }
        model.methodBodyModel_ = &methodBodyModel;
        model.expressionTypeBySyntax_[literal] = &literalType;
    }

    static bool appendAggregateElements(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        const std::string& targetTypeKey,
        std::uint64_t baseOffset,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& elements,
        std::string& error)
    {
        return model.appendAggregateElements(
            syntax, targetTypeKey, baseOffset, 0U, elements, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerBlock(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        std::string& error)
    {
        return model.lowerBlock(syntax, 0U, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerDefaultArgument(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        std::string_view parameterTypeKey,
        std::string& error)
    {
        return model.lowerDefaultArgument(syntax, parameterTypeKey, error);
    }

    static void assignYieldStates(
        ProjectOopHirModel& model,
        ProjectOopHirNode* root,
        std::size_t& nextYieldState)
    {
        model.assignYieldStates(root, nextYieldState);
    }

    static void rewriteGeneratorThrows(
        ProjectOopHirModel& model,
        ProjectOopHirNode* root,
        std::string_view errorTypeKey,
        std::string_view iteratorKey)
    {
        model.rewriteGeneratorThrows(root, errorTypeKey, iteratorKey);
    }

    static std::unique_ptr<ProjectOopHirNode> rewriteAsyncPollReturns(
        ProjectOopHirModel& model,
        std::unique_ptr<ProjectOopHirNode> body,
        std::size_t selfParameterOrdinal,
        std::string_view resultTypeKey,
        std::string_view futureKey,
        std::string& error,
        bool topLevel)
    {
        return model.rewriteAsyncPollReturns(
            std::move(body),
            selfParameterOrdinal,
            resultTypeKey,
            futureKey,
            error,
            topLevel);
    }

    static std::vector<std::size_t> collectAwaitedMemberOrdinals(
        ProjectOopHirModel& model,
        const ast::AstNode* root,
        const ast::AstNode* callSyntax,
        const MethodCallBinding& binding)
    {
        model.callBySyntax_[callSyntax] = &binding;
        return model.collectAwaitedMemberOrdinals(root);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerExpression(
        ProjectOopHirModel& model,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const MethodExpressionTypeBinding& binding,
        std::string& error)
    {
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &binding;
        return model.lowerExpression(root, 0U, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerMatchExpression(
        ProjectOopHirModel& model,
        ProjectMethodBodyModel& methodBodyModel,
        const ast::AstNode* root,
        const std::vector<const ast::AstNode*>& matches,
        std::vector<MethodMatchExpressionBinding> bindings,
        const std::vector<std::pair<
            const ast::AstNode*, const MethodExpressionTypeBinding*>>& typedSyntax,
        bool addressContext,
        std::string& error)
    {
        methodBodyModel.matchExpressionBindings_ = std::move(bindings);
        methodBodyModel.matchExpressionBindingBySyntax_.clear();
        for (std::size_t index = 0; index < matches.size(); ++index)
            methodBodyModel.matchExpressionBindingBySyntax_[matches[index]] = index;
        model.methodBodyModel_ = &methodBodyModel;
        for (const auto& [syntax, type] : typedSyntax)
            model.expressionTypeBySyntax_[syntax] = type;
        return model.lowerExpression(root, 0U, addressContext, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerDeconstructionExpression(
        ProjectOopHirModel& model,
        ProjectMethodBodyModel& methodBodyModel,
        const ProjectNameLookup& nameLookup,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ast::AstNode* root,
        std::vector<MethodDeconstructionBinding> bindings,
        std::vector<MethodLocalSymbol> locals,
        const std::vector<MethodNameBinding>& names,
        const std::vector<std::pair<
            const ast::AstNode*, const MethodExpressionTypeBinding*>>& typedSyntax,
        std::string& error)
    {
        methodBodyModel.deconstructionBindings_ = std::move(bindings);
        methodBodyModel.deconstructionBindingBySyntax_.clear();
        for (std::size_t index = 0;
            index < methodBodyModel.deconstructionBindings_.size(); ++index)
        {
            methodBodyModel.deconstructionBindingBySyntax_[
                methodBodyModel.deconstructionBindings_[index].syntax] = index;
        }
        methodBodyModel.locals_ = std::move(locals);
        model.methodBodyModel_ = &methodBodyModel;
        model.nameLookup_ = &nameLookup;
        model.objectLayoutModel_ = &objectLayoutModel;
        for (const MethodNameBinding& name : names)
            model.nameBindingBySyntax_[name.syntax] = &name;
        for (const auto& [syntax, type] : typedSyntax)
            model.expressionTypeBySyntax_[syntax] = type;
        return model.lowerExpression(root, 0U, false, error);
    }

    static void configureMatchCleanupLocal(
        ProjectOopHirModel& model,
        ProjectMethodBodyModel& methodBodyModel,
        const ast::AstNode* declaration)
    {
        MethodLocalSymbol local;
        local.syntax = declaration;
        local.name = "resource";
        local.typeKey = "type:Match.Resource";
        local.isRaii = true;
        methodBodyModel.locals_.push_back(std::move(local));
        model.localBySyntax_[declaration] = &methodBodyModel.locals_.back();

        MethodCleanupRegion cleanup;
        cleanup.syntax = declaration;
        methodBodyModel.cleanupRegions_.push_back(std::move(cleanup));
        model.cleanupRegionBySyntax_[declaration] = &methodBodyModel.cleanupRegions_.back();
    }

    static std::size_t activeCleanupDepth(const ProjectOopHirModel& model)
    {
        return model.activeHirCleanupRegions_.size();
    }

    static std::unique_ptr<ProjectOopHirNode> lowerFieldExpression(
        ProjectOopHirModel& model,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodNameBinding>& names,
        const MethodExpressionTypeBinding& type,
        bool addressContext,
        std::string& error)
    {
        for (const auto& node : syntax)
        {
            model.expressionTypeBySyntax_[node.get()] = &type;
        }
        for (const MethodNameBinding& name : names)
        {
            model.nameBindingBySyntax_[name.syntax] = &name;
        }
        return model.lowerExpression(root, 0U, addressContext, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerPropertyGetterExpression(
        ProjectOopHirModel& model,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCallBinding>& calls,
        const MethodExpressionTypeBinding& type,
        std::string& error)
    {
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const MethodCallBinding& call : calls)
            model.callBySyntax_[call.syntax] = &call;
        return model.lowerExpression(root, callerMemberOrdinal, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerUserUnaryOperatorExpression(
        ProjectOopHirModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCallBinding>& calls,
        const MethodExpressionTypeBinding& type,
        std::string& error)
    {
        model.nameLookup_ = &nameLookup;
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const MethodCallBinding& call : calls)
            model.callBySyntax_[call.syntax] = &call;
        return model.lowerExpression(root, callerMemberOrdinal, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerUserBinaryOperatorExpression(
        ProjectOopHirModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCallBinding>& calls,
        const std::vector<MethodBorrowBinding>& borrows,
        const MethodExpressionTypeBinding& type,
        std::string& error)
    {
        model.nameLookup_ = &nameLookup;
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const MethodCallBinding& call : calls)
            model.callBySyntax_[call.syntax] = &call;
        for (const MethodBorrowBinding& borrow : borrows)
            model.borrowBindingBySyntax_[borrow.syntax] = &borrow;
        return model.lowerExpression(root, callerMemberOrdinal, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerEventSubscriptionExpression(
        ProjectOopHirModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<MethodEventBinding>& bindings,
        bool addressContext,
        const MethodLambdaBinding* currentLambda,
        std::string& error)
    {
        model.nameLookup_ = &nameLookup;
        model.memberModel_ = &memberModel;
        for (const MethodEventBinding& binding : bindings)
            model.eventBindingBySyntax_[binding.syntax] = &binding;
        model.currentLambda_ = currentLambda;
        auto result = model.lowerExpression(
            root, callerMemberOrdinal, addressContext, error);
        model.currentLambda_ = nullptr;
        return result;
    }

    static std::unique_ptr<ProjectOopHirNode> lowerPropertyIndexerSetterExpression(
        ProjectOopHirModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCallBinding>& calls,
        const MethodExpressionTypeBinding& valueType,
        const MethodExpressionTypeBinding& receiverType,
        bool addressContext,
        const MethodLambdaBinding* currentLambda,
        std::string& error)
    {
        model.nameLookup_ = &nameLookup;
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax)
        {
            model.expressionTypeBySyntax_[node.get()] =
                node->kind() == ast::AstNodeKind::ThisExpression
                    ? &receiverType : &valueType;
        }
        for (const MethodCallBinding& call : calls)
            model.callBySyntax_[call.syntax] = &call;
        model.currentLambda_ = currentLambda;
        auto result = model.lowerExpression(
            root, callerMemberOrdinal, addressContext, error);
        model.currentLambda_ = nullptr;
        return result;
    }

    static std::unique_ptr<ProjectOopHirNode> lowerExtensionPropertyGetterExpression(
        ProjectOopHirModel& model,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<NamespaceFunctionCallBinding>& calls,
        const MethodExpressionTypeBinding& type,
        std::string& error)
    {
        model.memberModel_ = &memberModel;
        model.namespaceFunctionModel_ = &namespaceFunctionModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const NamespaceFunctionCallBinding& call : calls)
            model.namespaceFunctionCallBySyntax_[call.syntax] = &call;
        return model.lowerExpression(root, callerMemberOrdinal, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerExtensionPropertySetterExpression(
        ProjectOopHirModel& model,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<NamespaceFunctionCallBinding>& calls,
        const MethodExpressionTypeBinding& type,
        bool addressContext,
        std::string& error)
    {
        model.memberModel_ = &memberModel;
        model.namespaceFunctionModel_ = &namespaceFunctionModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const NamespaceFunctionCallBinding& call : calls)
            model.namespaceFunctionCallBySyntax_[call.syntax] = &call;
        return model.lowerExpression(root, callerMemberOrdinal, addressContext, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerNullConditionalAssignmentExpression(
        ProjectOopHirModel& model,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodNameBinding>& names,
        const std::vector<MethodCoreBinding>& cores,
        const MethodExpressionTypeBinding& type,
        bool addressContext,
        std::string& error)
    {
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const MethodNameBinding& name : names)
            model.nameBindingBySyntax_[name.syntax] = &name;
        for (const MethodCoreBinding& core : cores)
            model.coreBindingBySyntax_[core.syntax] = &core;
        return model.lowerExpression(root, callerMemberOrdinal, addressContext, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerUserIndexerGetterExpression(
        ProjectOopHirModel& model,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCallBinding>& calls,
        const std::vector<MethodCoreBinding>& cores,
        const MethodExpressionTypeBinding& nodeType,
        const MethodExpressionTypeBinding& indexType,
        std::string& error)
    {
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax)
        {
            model.expressionTypeBySyntax_[node.get()] =
                node->kind() == ast::AstNodeKind::IntegerLiteralExpression
                    ? &indexType : &nodeType;
        }
        for (const MethodCallBinding& call : calls)
            model.callBySyntax_[call.syntax] = &call;
        for (const MethodCoreBinding& core : cores)
            model.coreBindingBySyntax_[core.syntax] = &core;
        return model.lowerExpression(root, callerMemberOrdinal, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerInterpolationHandlerExpression(
        ProjectOopHirModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCoreBinding>& cores,
        const MethodExpressionTypeBinding& type,
        bool addressContext,
        std::string& error)
    {
        model.nameLookup_ = &nameLookup;
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const MethodCoreBinding& core : cores)
            model.coreBindingBySyntax_[core.syntax] = &core;
        return model.lowerExpression(root, callerMemberOrdinal, addressContext, error);
    }

    static void setRuntimeTypeBinding(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        const MethodRuntimeTypeBinding& binding)
    {
        model.runtimeTypeBindingBySyntax_[syntax] = &binding;
    }

    static std::unique_ptr<ProjectOopHirNode> lowerStatement(
        ProjectOopHirModel& model,
        const ast::AstNode* statement,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const MethodExpressionTypeBinding& binding,
        std::string& error)
    {
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &binding;
        return model.lowerStatement(statement, 0U, error);
    }

    static void setDumperBody(
        ProjectOopHirModel& model,
        std::unique_ptr<ProjectOopHirNode> body)
    {
        ProjectOopHirFunction function;
        function.isStatic = true;
        function.mangledName = "deep_hir_dump";
        function.returnTypeKey = "builtin:void";
        function.body = std::move(body);
        model.functions_.push_back(std::move(function));
    }

    static std::unique_ptr<ProjectOopHirNode> takeDumperBody(ProjectOopHirModel& model)
    {
        if (model.functions_.empty()) return nullptr;
        return std::move(model.functions_.front().body);
    }

    static void setDumperFieldInitializer(
        ProjectOopHirModel& model,
        std::unique_ptr<ProjectOopHirNode> value)
    {
        ProjectOopHirFieldInitializer initializer;
        initializer.value = std::move(value);
        model.fieldInitializers_.push_back(std::move(initializer));
    }

    static bool stackClassLocalUsesAreSafe(
        const ProjectOopHirNode* node,
        const ProjectOopHirNode* declaration,
        std::size_t localOrdinal)
    {
        return ProjectOopHirModel::stackClassLocalUsesAreSafe(
            node, declaration, localOrdinal);
    }

    static void collectLocalDeclarations(
        ProjectOopHirNode* node,
        std::vector<ProjectOopHirNode*>& declarations)
    {
        ProjectOopHirModel::collectLocalDeclarations(node, declarations);
    }

    static const ProjectOopHirNode* firstUnpromotedObjectConstruction(
        const ProjectOopHirNode* node)
    {
        return ProjectOopHirModel::firstUnpromotedObjectConstruction(node);
    }

    static void scanParallelOrdinals(
        const ProjectOopHirNode* node,
        std::size_t localFloor,
        std::size_t cleanupFloor,
        std::size_t& localCount,
        std::size_t& cleanupCount,
        std::size_t& regionCount)
    {
        ProjectOopHirModel::scanParallelOrdinals(
            node, localFloor, cleanupFloor, localCount, cleanupCount, regionCount);
    }

    static void applyParallelOrdinalOffsets(
        ProjectOopHirNode* node,
        std::size_t localFloor,
        std::size_t cleanupFloor,
        std::size_t localOffset,
        std::size_t cleanupOffset,
        std::size_t regionOffset)
    {
        ProjectOopHirModel::applyParallelOrdinalOffsets(
            node, localFloor, cleanupFloor, localOffset, cleanupOffset, regionOffset);
    }

    static std::unique_ptr<ProjectOopMirNode> lowerMirNode(
        ProjectOopMirModel& model,
        const ProjectOopHirNode* node,
        std::string& error)
    {
        return model.lowerNode(node, error);
    }

    static bool collectMirCleanupContract(
        ProjectOopMirFunction& function,
        const ProjectOopMirNode* root,
        language::PanicProfile panicProfile,
        std::string& error)
    {
        return ProjectOopMirModel::collectCleanupContract(
            function, root, panicProfile, error);
    }

    static bool validateMirCleanupReferences(
        const ProjectOopMirFunction& function,
        const ProjectOopMirNode* root,
        std::string& error)
    {
        return ProjectOopMirModel::validateCleanupReferences(function, root, error);
    }

    static std::uint64_t sumMirAwaitedFrameSize(
        const ProjectOopMirNode* root,
        std::span<const ProjectOopMirStructLayout> layouts)
    {
        std::uint64_t frameSize = 0;
        ProjectOopMirModel::sumAwaitedFrameSize(root, layouts, frameSize);
        return frameSize;
    }

    static void collectMirPollLiveness(
        const ProjectOopMirNode* root,
        std::uint64_t& order,
        std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::unordered_map<std::size_t, std::uint64_t>& parameterLastUse,
        std::vector<std::uint64_t>& suspendOrders)
    {
        ProjectOopMirModel::collectPollLiveness(
            root, order, localDefinition, localLastUse, parameterLastUse, suspendOrders);
    }

    static void collectMirMoveNextLiveness(
        const ProjectOopMirNode* root,
        std::uint64_t& order,
        std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::unordered_map<std::size_t, std::uint64_t>& parameterLastUse,
        std::vector<std::uint64_t>& yieldOrders)
    {
        ProjectOopMirModel::collectMoveNextLiveness(
            root, order, localDefinition, localLastUse, parameterLastUse, yieldOrders);
    }

    static void collectMirBorrowSources(
        const ProjectOopMirNode* root,
        std::unordered_map<std::size_t, std::size_t>& borrowSource,
        std::unordered_set<std::size_t>& borrowNonLocalSource)
    {
        ProjectOopMirModel::collectBorrowSources(root, borrowSource, borrowNonLocalSource);
    }

    static void collectMirLivePollLocals(
        const ProjectOopMirNode* root,
        const std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        const std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::span<const std::uint64_t> suspendOrders,
        std::unordered_set<std::size_t>& liveSet)
    {
        ProjectOopMirModel::collectLivePollLocals(
            root, localDefinition, localLastUse, suspendOrders, liveSet);
    }

    static void appendMirPollFrameLocals(
        const ProjectOopMirNode* root,
        const std::unordered_set<std::size_t>& liveSet,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectNameLookup& nameLookup,
        ProjectOopMirFunction& function,
        std::uint64_t& frameSize)
    {
        ProjectOopMirModel::appendPollFrameLocals(
            root, liveSet, objectLayoutModel, nameLookup, function, frameSize);
    }

    static void appendMirMoveNextFrameLocals(
        const ProjectOopMirNode* root,
        const std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
        const std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
        std::span<const std::uint64_t> yieldOrders,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectNameLookup& nameLookup,
        ProjectOopMirFunction& function,
        std::uint64_t& frameSize)
    {
        ProjectOopMirModel::appendMoveNextFrameLocals(
            root, localDefinition, localLastUse, yieldOrders,
            objectLayoutModel, nameLookup, function, frameSize);
    }

    static std::string serializeMirNodeTree(const ProjectOopMirNode* root)
    {
        return ProjectOopMirModel::serializeMirNodeTreeForTesting(root);
    }

    static bool restoreMirNodeTree(
        std::string_view bytes,
        std::unique_ptr<ProjectOopMirNode>& root)
    {
        return ProjectOopMirModel::restoreMirNodeTreeForTesting(bytes, root);
    }

    static std::string dumpMirNode(const ProjectOopMirNode* root, int depth)
    {
        return ProjectOopMirModelDumper::dumpNodeForTesting(root, depth);
    }

    static std::size_t mirNodeTraversalDepth(
        const ProjectOopMirNode* root,
        std::size_t& visitedNodes)
    {
        return ProjectOopMirModelDumper::nodeTraversalDepthForTesting(root, visitedNodes);
    }

    static bool importMirLayouts(
        ProjectOopMirModel& model,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectOopHirModel& hirModel,
        std::string& error)
    {
        return model.importLayouts(
            nameLookup, memberModel, objectLayoutModel, hirModel, error);
    }

    static std::vector<std::string> collectFutureLayoutKeys(
        const ProjectOopHirNode* root)
    {
        std::vector<std::string> keys;
        ProjectOopMirModel::collectFutureLayoutKeys(root, keys);
        return keys;
    }

    static std::vector<std::string> collectIteratorLayoutKeys(
        const ProjectOopHirNode* root)
    {
        std::vector<std::string> keys;
        ProjectOopMirModel::collectIteratorLayoutKeys(root, keys);
        return keys;
    }
};
}

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirModel;
using nativesharp::frontend::ProjectOopMirModel;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return 0;
    }
    std::size_t count = node->kind == kind ? 1u : 0u;
    for (const auto& child : node->children)
    {
        count += countMirKind(child.get(), kind);
    }
    return count;
}

const ProjectOopMirNode* findMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind == kind)
    {
        return node;
    }
    for (const auto& child : node->children)
    {
        if (const ProjectOopMirNode* found = findMirKind(child.get(), kind); found != nullptr)
        {
            return found;
        }
    }
    return nullptr;
}

// EL GLOBAL QUE GUARDA UNA CADENA CONCRETA. Los nombres `@__nsc_str_N` son POSICIONALES: buscar
// `@__nsc_str_1` a mano ataria la prueba al orden en que hoy se emiten los literales, y ese orden
// no es el contrato. Se busca el global por su CONTENIDO y se devuelve el nombre que tenga.
std::string stringGlobalHolding(std::string_view ir, std::string_view content)
{
    const std::string needle = "c\"" + std::string(content) + "\\00\"";
    const std::size_t at = ir.find(needle);
    if (at == std::string_view::npos) return {};
    const std::size_t lineStart = ir.rfind('\n', at);
    const std::string_view line =
        ir.substr(lineStart == std::string_view::npos ? 0 : lineStart + 1);
    const std::size_t nameEnd = line.find(' ');
    if (line.empty() || line.front() != '@' || nameEnd == std::string_view::npos) return {};
    return std::string(line.substr(0, nameEnd));
}

// EL CUERPO DE UNA FUNCION, para no confundir lo que hace una con lo que hace otra. Buscar
// `ret ptr @X` en el modulo entero encontraria cualquier funcion que devolviera esa cadena.
std::string functionBody(std::string_view ir, std::string_view mangledFragment)
{
    const std::size_t at = ir.find(mangledFragment);
    if (at == std::string_view::npos) return {};
    const std::size_t open = ir.find('{', at);
    if (open == std::string_view::npos) return {};
    const std::size_t close = ir.find("\n}", open);
    if (close == std::string_view::npos) return {};
    return std::string(ir.substr(open, close - open));
}

// NSC-LIMIT-005 -- EL ENUM ESTRECHO CON SIGNO, FIJADO PARA QUE NO SE PUEDA DESHACER EN SILENCIO.
//
// EL DEFECTO QUE ESTO CONSERVA. `c38_enum_ancho` devolvia 42 a -O0 y 39 a -O2, y `b9` reprodujo
// el motivo: `ToString()` de un `enum : sbyte` con valor NEGATIVO devolvia la RESERVA --el nombre
// del tipo-- despues de optimizar. El IR sin optimizar era correcto y el modulo verificaba; lo que
// fallaba era la decision del optimizador sobre una constante negativa estrecha.
//
// EL ARREGLO, y por que hay que fijarlo: `lowerEnumName` promueve el receptor con `zext` a i64 y
// compara contra el PATRON DE BITS enmascarado al ancho del tipo subyacente. Con `sext` --que es
// el cambio de una letra que cualquiera haria «limpiando» un tipo con signo-- `-3` se convierte en
// `i64 -3` mientras el caso sigue siendo `i64 253`, no casa ninguno, y se elige la reserva. MEDIDO:
// con esa unica letra cambiada, esta misma sonda a O2 pasa de devolver el global de "MenosTres" a
// devolver el de la reserva. Es exactamente el fallo original.
//
// POR ESO SE COMPRUEBAN LAS DOS CAPAS. La forma del IR sola no bastaria --describe la
// implementacion de hoy-- y el resultado optimizado solo tampoco --pasaria por accidente si el
// plegado cambiara--. Juntas dicen que la eleccion es correcta Y por que lo es.
void testNarrowSignedEnumNameLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "NarrowSignedEnumName";
    writeProject(directory, "NarrowSignedEnumName");
    writeFile(directory / "Main.ns",
        "namespace Sonda; public class Prueba { "
        "public enum ConSigno : sbyte { MenosTres = -3, Cero = 0, Tope = 127 } "
        "public string Nombre() { return ConSigno.MenosTres.ToString(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok =
        compilation.compileProjectOopMir(directory / "NarrowSignedEnumName.nsproj", error);
    tests.expect(ok && compilation.oopMirModel() != nullptr,
        "a narrow signed enum with a negative enumerator compiles through project OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "the narrow signed enum lowers to LLVM without backend errors");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError),
        "the enum-name switch is verifier-clean, so no two cases collapsed to the same value");

    const std::string plain = module->text();
    const std::string body = functionBody(plain, "Prueba__Nombre");
    tests.expect(!body.empty(), "the enum-name function is found in the unoptimized module");

    // CAPA 1 -- LA FORMA. Promocion SIN SIGNO a 64 bits y un `switch`, no una cadena de `select`.
    tests.expect(body.find("zext i8 ") != std::string::npos &&
            body.find(" to i64") != std::string::npos,
        "the narrow receiver is promoted to i64 without sign");
    tests.expect(body.find("sext i8 ") == std::string::npos,
        "and never sign-extended: that is the one-letter change that reintroduces the defect");
    tests.expect(body.find("switch i64 ") != std::string::npos,
        "the enumerators are decided by a switch on the 64-bit bit pattern");
    tests.expect(body.find("select ") == std::string::npos,
        "and not by the select chain this replaced");

    // CAPA 2 -- EL PATRON DE BITS. -3 en ocho bits es 253, no -3. Si el caso se escribiera con el
    // numero fuente en vez de con sus bits, no casaria nunca con el receptor promovido sin signo.
    tests.expect(body.find("i64 253, label") != std::string::npos,
        "the negative enumerator is cased as its 8-bit pattern (253), not as -3");
    tests.expect(body.find("i64 -3, label") == std::string::npos,
        "the signed spelling of the same enumerator is not what the switch compares against");

    // CAPA 3 -- EL RESULTADO DESPUES DE OPTIMIZAR, que es donde el defecto se manifestaba. Con un
    // receptor constante, O2 pliega la funcion entera a un `ret` del global correcto. Que sea el
    // de "MenosTres" y no el de la reserva es la prueba de que la decision sobrevive al pipeline.
    std::string targetError;
    auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(targetMachine != nullptr, "the O2 target machine is available for the enum probe");
    if (targetMachine == nullptr) return;

    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    std::string optimizationError;
    tests.expect(
        optimizer.optimize(*module, *targetMachine,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2, optimizationError),
        "the narrow signed enum module survives the O2 pipeline");

    const std::string optimized = module->text();
    const std::string chosen = stringGlobalHolding(optimized, "MenosTres");
    const std::string fallback = stringGlobalHolding(optimized, "Prueba+ConSigno");
    tests.expect(!chosen.empty() && (fallback.empty() || chosen != fallback),
        "the enumerator name survives and cannot alias the private type-name fallback");
    if (chosen.empty()) return;

    const std::string optimizedBody = functionBody(optimized, "Prueba__Nombre");
    tests.expect(optimizedBody.find("ret ptr " + chosen) != std::string::npos,
        "after O2 the constant receiver folds to the enumerator name");
    tests.expect(fallback.empty() || optimizedBody.find("ret ptr " + fallback) == std::string::npos,
        "and not to the type-name fallback, which may now be removed as a private unused literal");
}

void testEndToEndOopLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "EndToEnd";
    writeProject(directory, "Stage30Test");
    writeFile(directory / "A.ns",
        "namespace Demo; public partial class Counter { byte Tag; int Value; "
        "public Counter(int start) { this.Value = start; } "
        "public int Add(int delta) { this.Value = this.Value + delta; return this.Value; } "
        "public int SumTo(int n) { int i = 0; int total = 0; while (i < n) { total = total + i; i = i + 1; } return total; } }");
    writeFile(directory / "B.ns",
        "namespace Demo; public partial class Counter { "
        "public int AddTwice(int delta) { return Add(delta) + Add(delta); } "
        "public Counter Make(int start) { return new Counter(start); } "
        "public Counter MakeAfterAdd(int delta) { return new Counter(Add(delta)); } } "
        "public class Empty { } public class Factory { public Empty MakeEmpty() { return new Empty(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage30Test.nsproj", error);
    tests.expect(ok, "Stage 30 project compiles through OOP MIR");
    tests.expect(error.empty(), "Stage 30 OOP MIR build has no loader/lowering error");
    tests.expect(compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr,
        "ProjectCompilation retains both OOP HIR and OOP MIR models");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const ProjectOopHirModel& hir = *compilation.oopHirModel();
    const ProjectOopMirModel& mir = *compilation.oopMirModel();
    tests.expect(hir.functions().size() == 9 && mir.functions().size() == 9,
        "seven written callables plus two publicable implicit constructors lower through both OOP IR layers");
    tests.expect(hir.functions()[0].mangledName.starts_with("__nsc_ctor_") &&
            hir.functions()[0].parameterTypeKeys.size() == 1,
        "constructor HIR has deterministic mangling and excludes implicit this from source parameter list");
    tests.expect(mir.functions()[0].constructor && mir.functions()[0].returnTypeKey == "builtin:void",
        "constructor MIR freezes void-return internal ABI");

    std::size_t fieldLoads = 0;
    std::size_t fieldAddresses = 0;
    std::size_t directCalls = 0;
    std::size_t constructions = 0;
    std::size_t loops = 0;
    for (const auto& function : mir.functions())
    {
        fieldLoads += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldLoad);
        fieldAddresses += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldAddress);
        directCalls += countMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        constructions += countMirKind(function.body.get(), ProjectOopMirNodeKind::AllocateAndConstruct);
        loops += countMirKind(function.body.get(), ProjectOopMirNodeKind::While);
    }
    tests.expect(fieldLoads >= 2 && fieldAddresses >= 2,
        "field reads/writes survive into OOP MIR as explicit address/load operations");
    tests.expect(directCalls == 3, "three Add(delta) calls become direct instance-call MIR operations");
    tests.expect(constructions == 3, "two explicit and one implicit-default new expressions become allocation MIR operations");
    tests.expect(loops == 1, "structured while control flow survives into OOP MIR");

    const ProjectOopMirNode* fieldAddress = findMirKind(mir.functions()[0].body.get(), ProjectOopMirNodeKind::FieldAddress);
    tests.expect(fieldAddress != nullptr && fieldAddress->immediate == 12 && fieldAddress->alignment == 4,
        "MIR consumes the Macro A object header and resolves Counter.Value to byte offset twelve");

    const ProjectOopMirNode* explicitConstruction = nullptr;
    const ProjectOopMirNode* defaultConstruction = nullptr;
    for (const auto& function : mir.functions())
    {
        const ProjectOopMirNode* construction = findMirKind(function.body.get(), ProjectOopMirNodeKind::AllocateAndConstruct);
        if (construction != nullptr)
        {
            if (construction->calleeName.empty()) defaultConstruction = construction;
            else explicitConstruction = construction;
        }
    }
    tests.expect(explicitConstruction != nullptr && explicitConstruction->immediate == 16 &&
            explicitConstruction->zeroInitialize && explicitConstruction->allocationFailureReturnsNull,
        "explicit construction MIR consumes the header-inclusive object size plus zero-init/null-on-failure contract");
    tests.expect(defaultConstruction != nullptr && defaultConstruction->immediate == 8 && defaultConstruction->calleeName.empty(),
        "root implicit default construction allocates the descriptor-only object without synthesizing a constructor entry point");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("abi=this:first") != std::string::npos &&
            hirDump.find("object-construction") != std::string::npos,
        "OOP HIR dump exposes internal ABI and semantic construction nodes");
    tests.expect(mirDump.find("offset=12 align=4") != std::string::npos &&
            mirDump.find("zero=true failure=null") != std::string::npos,
        "OOP MIR dump exposes header-adjusted field offsets and allocation contract");

    ProjectCompilation parallelCompilation;
    parallelCompilation.setCompilationJobs(4);
    std::string parallelError;
    const bool parallelOk = parallelCompilation.compileProjectOopMir(
        directory / "Stage30Test.nsproj", parallelError);
    tests.expect(parallelOk && parallelError.empty() && parallelCompilation.oopHirModel() != nullptr &&
            parallelCompilation.oopMirModel() != nullptr,
        "four-worker per-function HIR and MIR lowering succeeds on the same project");
    if (parallelCompilation.oopHirModel() != nullptr && parallelCompilation.oopMirModel() != nullptr)
    {
        const std::string parallelHirDump =
            nativesharp::frontend::ProjectOopHirModelDumper::dump(
                *parallelCompilation.oopHirModel());
        tests.expect(parallelHirDump == hirDump,
            "parallel per-function HIR is exactly equivalent to serial HIR");
        const std::string parallelMirDump =
            nativesharp::frontend::ProjectOopMirModelDumper::dump(
                *parallelCompilation.oopMirModel());
        tests.expect(parallelMirDump == mirDump,
            "parallel per-function MIR is exactly equivalent to serial MIR");

        nativesharp::llvm_backend::ProjectOopCodePartition serialPartition;
        nativesharp::llvm_backend::ProjectOopCodePartition parallelPartition;
        std::string partitionError;
        const bool serialPartitionOk = serialPartition.build(mir, 4, partitionError);
        const bool parallelPartitionOk = parallelPartition.build(
            *parallelCompilation.oopMirModel(), 4, partitionError);
        tests.expect(serialPartitionOk && parallelPartitionOk && partitionError.empty(),
            "module code partitioning accepts equivalent serial and parallel MIR");
        tests.expect(serialPartition.dump() == parallelPartition.dump(),
            "module code partitioning is deterministic across MIR scheduling modes");

        std::vector<bool> assigned(mir.functions().size(), false);
        bool validCoverage = true;
        for (const auto& unit : serialPartition.units())
        {
            validCoverage = validCoverage && !unit.stableKey.empty() &&
                !unit.functionOrdinals.empty();
            for (const std::size_t functionOrdinal : unit.functionOrdinals)
            {
                validCoverage = validCoverage && functionOrdinal < assigned.size() &&
                    !assigned[functionOrdinal] &&
                    serialPartition.unitForFunction(functionOrdinal) == unit.ordinal;
                if (functionOrdinal < assigned.size()) assigned[functionOrdinal] = true;
            }
        }
        for (std::size_t functionOrdinal = 0; functionOrdinal < mir.functions().size(); ++functionOrdinal)
        {
            const auto& function = mir.functions()[functionOrdinal];
            const bool shouldBeAssigned = !function.isImportedC && !function.isImportedNative;
            validCoverage = validCoverage && assigned[functionOrdinal] == shouldBeAssigned;
        }
        tests.expect(validCoverage,
            "every defined MIR function belongs to exactly one stable code unit and imports belong to none");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer globalLowerer;
        auto globalModule = globalLowerer.lowerPartition(
            *compilation.nameLookup(), *compilation.memberModel(), mir,
            compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, {}, true,
            "Stage30Test.globals");
        bool partitionModulesValid = globalModule != nullptr && globalLowerer.error().empty();
        std::string partitionFailure = globalLowerer.error();
        std::string partitionVerificationError;
        if (globalModule != nullptr && !globalModule->verify(partitionVerificationError))
        {
            partitionModulesValid = false;
            partitionFailure = partitionVerificationError;
        }
        for (const auto& unit : serialPartition.units())
        {
            nativesharp::llvm_backend::ProjectOopLlvmLowerer unitLowerer;
            auto unitModule = unitLowerer.lowerPartition(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                unit.functionOrdinals, false, "Stage30Test." + unit.stableKey);
            partitionModulesValid = partitionModulesValid && unitModule != nullptr &&
                unitLowerer.error().empty();
            if (unitModule == nullptr && partitionFailure.empty())
                partitionFailure = unitLowerer.error();
            if (unitModule != nullptr)
            {
                partitionVerificationError.clear();
                if (!unitModule->verify(partitionVerificationError))
                {
                    partitionModulesValid = false;
                    if (partitionFailure.empty()) partitionFailure = partitionVerificationError;
                }
            }
        }
        tests.expect(partitionModulesValid,
            "global and per-code-unit LLVM modules independently pass the verifier" +
                (partitionFailure.empty() ? std::string{} : ": " + partitionFailure));

        nativesharp::llvm_backend::ProjectOopLlvmLowerer debugGlobalLowerer;
        auto debugGlobalModule = debugGlobalLowerer.lowerPartition(
            *compilation.nameLookup(), *compilation.memberModel(), mir,
            compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::Full,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, {}, true,
            "Stage30Test.debug.globals");
        bool partitionDebugValid = debugGlobalModule != nullptr &&
            debugGlobalLowerer.error().empty();
        std::string partitionDebugFailure = debugGlobalLowerer.error();
        for (const auto& unit : serialPartition.units())
        {
            nativesharp::llvm_backend::ProjectOopLlvmLowerer debugUnitLowerer;
            auto debugUnitModule = debugUnitLowerer.lowerPartition(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::Full,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                unit.functionOrdinals, false, "Stage30Test.debug." + unit.stableKey);
            partitionDebugValid = partitionDebugValid && debugUnitModule != nullptr &&
                debugUnitLowerer.error().empty();
            if (debugUnitModule == nullptr && partitionDebugFailure.empty())
                partitionDebugFailure = debugUnitLowerer.error();
        }
        tests.expect(partitionDebugValid,
            "partitioned debug metadata belongs only to defining modules and verifies cleanly" +
                (partitionDebugFailure.empty() ? std::string{} : ": " + partitionDebugFailure));

        nativesharp::llvm_backend::ProjectOopObjectGenerator objectGenerator;
        nativesharp::llvm_backend::CompilationArtifactCache artifactCache;
        artifactCache.setRoot(directory / "object-cache");
        artifactCache.setCodegenIdentity("project-oop-lowering-test");
        nativesharp::llvm_backend::CompilationArtifactCacheScope artifactCacheScope(artifactCache);
        std::string objectGenerationError;
        auto objectBundle = objectGenerator.generate(
            *compilation.nameLookup(), *compilation.memberModel(), mir,
            compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, objectGenerationError);
        tests.expect(objectBundle != nullptr && objectGenerationError.empty() &&
                objectBundle->codeUnitCount() == serialPartition.units().size() &&
                objectBundle->objectFiles().size() == serialPartition.units().size() + 1 &&
                objectBundle->cacheHitCount() == 0 &&
                objectBundle->cacheMissCount() == objectBundle->objectFiles().size(),
            "four-worker code generation emits one verified coordinator object and one object per code unit");
        if (objectBundle != nullptr)
        {
            objectGenerationError.clear();
            auto cachedBundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {},
                objectGenerationError);
            tests.expect(cachedBundle != nullptr && objectGenerationError.empty() &&
                    cachedBundle->cacheHitCount() == cachedBundle->objectFiles().size() &&
                    cachedBundle->cacheMissCount() == 0,
                "second equivalent generation restores every verified code unit from content cache" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));

            objectGenerationError.clear();
            auto coldO0Bundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 16, {},
                objectGenerationError);
            tests.expect(coldO0Bundle != nullptr && objectGenerationError.empty() &&
                    coldO0Bundle->codeUnitCount() <= 8 &&
                    coldO0Bundle->objectFiles().size() == coldO0Bundle->codeUnitCount() + 1,
                "O0 keeps the requested worker pool but caps cold LLVM code-unit granularity at eight" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));

            nativesharp::linker::WindowsLibraryBuilder partitionedBuilder;
            const std::vector<std::string> exports = {mir.functions()[1].mangledName};
            std::string linkError;
            const bool linked = partitionedBuilder.buildDynamicObjects(
                objectBundle->objectFiles(), objectBundle->combinedLlvmText(), exports,
                directory / "Stage30Partitioned.dll", directory / "Stage30Partitioned.lib",
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                nativesharp::llvm_backend::DebugInformationKind::None, linkError);
            tests.expect(linked && linkError.empty(),
                "partitioned code units link into one native DLL without duplicate or missing symbols" +
                    (linkError.empty() ? std::string{} : ": " + linkError));

            objectGenerationError.clear();
            auto optimizedBundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O2, 4, {},
                objectGenerationError);
            tests.expect(optimizedBundle != nullptr && objectGenerationError.empty() &&
                    optimizedBundle->usedParallelOptimization(),
                "O2 optimization runs as independent code-unit tasks" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));
            if (optimizedBundle != nullptr)
            {
                linkError.clear();
                const bool optimizedLinked = partitionedBuilder.buildDynamicObjects(
                    optimizedBundle->objectFiles(), optimizedBundle->combinedLlvmText(), exports,
                    directory / "Stage30PartitionedO2.dll",
                    directory / "Stage30PartitionedO2.lib",
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O2,
                    nativesharp::llvm_backend::DebugInformationKind::None, linkError);
                tests.expect(optimizedLinked && linkError.empty(),
                    "parallel O2 code units retain all cross-unit and exported symbols at link" +
                        (linkError.empty() ? std::string{} : ": " + linkError));
            }

            objectGenerationError.clear();
            auto optimizedDebugBundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::Full,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O2, 4, {},
                objectGenerationError);
            tests.expect(optimizedDebugBundle != nullptr && objectGenerationError.empty() &&
                    optimizedDebugBundle->codeUnitCount() == 1 &&
                    optimizedDebugBundle->objectFiles().size() == 1 &&
                    !optimizedDebugBundle->usedParallelOptimization(),
                "full-debug optimized generation remains monolithic so LLVM retains inline sites" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));
        }
    }

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), mir);
    tests.expect(module != nullptr, "Stage 30 lowers project OOP MIR to an LLVM module");
    tests.expect(lowerer.error().empty(), "LLVM OOP lowering reports no internal error for valid project");
    if (module == nullptr)
    {
        return;
    }
    std::string verificationError;
    tests.expect(module->verify(verificationError), "LLVM verifier accepts Stage 30 OOP module");
    tests.expect(verificationError.empty(), "verified Stage 30 LLVM module has no verifier diagnostics");

    const std::string llvm = module->text();
    tests.expect(llvm.find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos,
        "OOP LLVM module uses NativeSharp Windows x64 target triple");
    tests.expect(llvm.find("declare ptr @__nativesharp_alloc(i64)") != std::string::npos,
        "OOP LLVM module declares the existing NativeSharp allocation hook");
    tests.expect(llvm.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
        "OOP LLVM module declares the existing NativeSharp deallocation hook");
    tests.expect(llvm.find("@llvm.memset.p0.i64") != std::string::npos,
        "OOP object initialization materializes zero-initialization with LLVM memset");
    tests.expect(llvm.find("getelementptr i8, ptr %this, i64 12") != std::string::npos,
        "field addressing lowers exactly to the explicit receiver plus the Macro A byte offset");
    // NSC-LIMIT-009. LA BUSQUEDA LLEVA `@\"` Y NO `@`, y no es un adorno: todo simbolo que sale
    // de una identidad lleva el `$3A$3A` del `::` del modulo, y LLVM entrecomilla cualquier
    // nombre con un byte que no sea alfanumerico, `-`, `.` o `_`. Lo impreso es
    // `@\"__nsc_...\"`. Escrito a mano sin la comilla encuentra CERO, y en una asercion de
    // ausencia eso seria un verde falso. Ver TypeIdentity.h e `irTextSymbolReference`.
    tests.expect(llvm.find("call void @\"__nsc_ctor_") != std::string::npos,
        "explicit object construction lowers selected constructor call after allocation");
    tests.expect(
        llvm.find("define weak_odr void @\"__nsc_ctor_Stage30Test$3A$3ADemo$2EEmpty__Empty") !=
            std::string::npos &&
        llvm.find("define weak_odr void @\"__nsc_ctor_Stage30Test$3A$3ADemo$2EFactory__Factory") !=
            std::string::npos,
        "compiler-synthesized implicit constructors are per-symbol ODR definitions");
    tests.expect(
        llvm.find("define weak_odr void @\"__nsc_ctor_Stage30Test$3A$3ADemo$2ECounter__Counter") ==
            std::string::npos,
        "written constructors remain strong definitions rather than hiding user ODR conflicts");
    tests.expect(llvm.find("call i32 @\"__nsc_method_") != std::string::npos,
        "instance method calls lower to direct internal-ABI calls with implicit this");
    tests.expect(llvm.find("while.cond") != std::string::npos && llvm.find("while.body") != std::string::npos,
        "structured OOP while lowers to explicit LLVM control-flow blocks");
    tests.expect(llvm.find("alloc.failed") != std::string::npos && llvm.find("object.init") != std::string::npos,
        "allocation failure is checked before zero-init/constructor execution");

    const std::size_t makeAfterStart = llvm.find(
        "define ptr @\"__nsc_method_Stage30Test$3A$3ADemo$2ECounter__MakeAfterAdd__builtin$3Aint");
    const std::size_t makeAfterEnd = makeAfterStart == std::string::npos ? std::string::npos : llvm.find("\n}", makeAfterStart);
    const std::size_t argumentCall = makeAfterStart == std::string::npos ? std::string::npos :
        llvm.find(
        "call i32 @\"__nsc_method_Stage30Test$3A$3ADemo$2ECounter__Add__builtin$3Aint",
        makeAfterStart);
    const std::size_t allocationCall = makeAfterStart == std::string::npos ? std::string::npos :
        llvm.find("call ptr @__nativesharp_alloc", makeAfterStart);
    tests.expect(
        makeAfterStart != std::string::npos && makeAfterEnd != std::string::npos &&
            argumentCall != std::string::npos && allocationCall != std::string::npos &&
            argumentCall < allocationCall && allocationCall < makeAfterEnd,
        "object-construction arguments are evaluated before allocation and dominate the success branch");

    std::string targetError;
    auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(targetMachine != nullptr && targetError.empty(), "Windows x64 target machine is available for OOP codegen");
    if (targetMachine == nullptr)
    {
        return;
    }
    tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
        "target data layout is applied to verified OOP LLVM module");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "standard LLVM O2 pipeline accepts project OOP module");

    const auto objectPath = directory / "stage30.obj";
    tests.expect(targetMachine->emit(*module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "optimized project OOP LLVM module emits a COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier objectVerifier;
    tests.expect(objectVerifier.verify(objectPath), "Stage 30 emitted object passes structural COFF verification");
    tests.expect(objectVerifier.info().machine == 0x8664 && objectVerifier.info().sectionCount > 0,
        "Stage 30 OOP object targets AMD64 and contains sections");
}

void testDeallocationContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Deallocation";
    writeProject(directory, "Deallocation");
    writeFile(directory / "Main.ns",
        "class Payload { public void Release(Payload value) { delete value; delete null; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Deallocation.nsproj", error);
    tests.expect(ok, "class-reference delete lowers through Stage 30 OOP MIR");
    tests.expect(error.empty(), "valid class-reference delete has no project lowering error");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto& hir = *compilation.oopHirModel();
    const auto& mir = *compilation.oopMirModel();
    tests.expect(hir.functions().size() == 2 && mir.functions().size() == 2,
        "deallocation fixture produces its written method plus the publicable implicit constructor");
    tests.expect(countMirKind(mir.functions().front().body.get(), ProjectOopMirNodeKind::Deallocate) == 2,
        "delete reference and delete null both become explicit OOP MIR deallocations");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("deallocation type=builtin:void") != std::string::npos,
        "OOP HIR dump exposes deallocation as a first-class lifetime operation");
    tests.expect(mirDump.find("deallocate type=builtin:void") != std::string::npos,
        "OOP MIR dump preserves executable deallocation intent");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), mir);
    tests.expect(module != nullptr && lowerer.error().empty(),
        "class-reference deallocation lowers to verified LLVM");
    if (module != nullptr)
    {
        std::string verificationError;
        tests.expect(module->verify(verificationError) && verificationError.empty(),
            "LLVM verifier accepts class-reference deallocation");
        const std::string llvm = module->text();
        tests.expect(llvm.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
            "Stage 30 LLVM declares the NativeSharp free hook");
        std::size_t calls = 0;
        std::size_t position = 0;
        while ((position = llvm.find("call void @__nativesharp_free(ptr", position)) != std::string::npos)
        {
            ++calls;
            ++position;
        }
        tests.expect(calls == 2,
            "delete reference and delete null each materialize one runtime free call; runtime free owns null no-op semantics");

        std::string targetError;
        auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
        tests.expect(targetMachine != nullptr && targetError.empty(),
            "deallocation fixture has Windows x64 target machine");
        if (targetMachine != nullptr)
        {
            tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
                "deallocation LLVM receives Windows x64 data layout");
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
                "deallocation LLVM survives O2 optimization");
            const auto objectPath = directory / "deallocation.obj";
            tests.expect(targetMachine->emit(
                    *module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
                "deallocation LLVM emits Windows x64 COFF");
            nativesharp::llvm_backend::CoffObjectVerifier objectVerifier;
            tests.expect(objectVerifier.verify(objectPath) && objectVerifier.info().machine == 0x8664,
                "deallocation object passes AMD64 COFF verification");
        }
    }

    const auto invalidDirectory = root / "DeleteArrayClass";
    writeProject(invalidDirectory, "DeleteArrayClass");
    writeFile(invalidDirectory / "Main.ns",
        "class Payload { public void Release(Payload value) { delete[] value; } }");
    ProjectCompilation invalidCompilation;
    error.clear();
    const bool invalidOk = invalidCompilation.compileProjectOopMir(
        invalidDirectory / "DeleteArrayClass.nsproj", error);
    tests.expect(!invalidOk, "Stage 41 still rejects delete[] on a plain class reference; only T[] is a dynamic-array reference");
    tests.expect(error.find("delete[] requires a native dynamic array reference") != std::string::npos,
        "plain class-reference delete[] boundary remains explicit after dynamic arrays were introduced");
}

void testAggregateCallableBoundary(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "AggregateBoundary";
    writeProject(directory, "AggregateBoundary");
    writeFile(directory / "Main.ns",
        "struct Large { int A; int B; int Z; } class Box { public Large Echo(Large value) { return value; } }");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "AggregateBoundary.nsproj", error);
    tests.expect(ok, "large aggregate-by-value callable remains semantically valid through project OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr)
    {
        return;
    }
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    // HISTORY: this test used to assert the OPPOSITE -- that a 12-byte aggregate crossing a
    // callable boundary was REJECTED ("Stage 40 direct struct value ABI supports only 1, 2, 4
    // or 8-byte aggregates"), pinning the documented limitation so it could not silently turn
    // into a miscompile. The limitation is now implemented instead of documented: any aggregate
    // whose size is not 1/2/4/8 travels indirectly, exactly the way clang lowers it for
    // x86_64-pc-windows-msvc -- the parameter as a pointer to a caller-owned per-call copy, the
    // return through a leading sret pointer -- so the same fixture must now LOWER, and the IR
    // must show the indirect shape rather than any direct aggregate value in the signature.
    tests.expect(module != nullptr && lowerer.error().empty(),
        "a 12-byte aggregate-by-value callable now lowers through the Win64 indirect ABI (" +
            lowerer.error() + ")");
    if (module != nullptr)
    {
        std::string verifyError;
        tests.expect(module->verify(verifyError),
            "indirect-aggregate module verifies clean (" + verifyError + ")");
        const std::string llvm = module->text();
        const std::size_t echoAt = llvm.find("define ");
        tests.expect(echoAt != std::string::npos &&
                llvm.find("sret([12 x i8])") != std::string::npos,
            "the Large return travels through an sret([12 x i8]) slot, not a direct value");
    }
}

void testPreciseIncrementalInvalidation(TestContext& tests, const std::filesystem::path& root)
{
    const auto firstDirectory = root / "IncrementalFirst";
    const auto changedDirectory = root / "IncrementalChanged";
    writeProject(firstDirectory, "IncrementalCache");
    writeProject(changedDirectory, "IncrementalCache");
    const std::string commonPrefix = R"NS(
public class Work
{
    public static int Alpha() { return 1; }
    public static int Beta() { return 2; }
    public static int Gamma() { return 3; }
    public static int Delta() { return 4; }
    public static int Epsilon() { return 5; }
    public static int Zeta() { return 6; }
)NS";
    writeFile(firstDirectory / "Work.ns", commonPrefix +
        "    public static int Changed() { return 7; }\n}\n");
    writeFile(changedDirectory / "Work.ns", commonPrefix +
        "    public static int Changed() { return 700; }\n}\n");

    ProjectCompilation first;
    first.setCompilationJobs(4);
    ProjectCompilation changed;
    changed.setCompilationJobs(4);
    std::string error;
    const bool firstOk = first.compileProjectOopMir(
        firstDirectory / "IncrementalCache.nsproj", error);
    std::string changedError;
    const bool changedOk = changed.compileProjectOopMir(
        changedDirectory / "IncrementalCache.nsproj", changedError);
    tests.expect(firstOk && changedOk,
        "body-only incremental fixtures both produce OOP MIR (" + error + changedError + ")");
    if (!firstOk || !changedOk || first.oopMirModel() == nullptr ||
        changed.oopMirModel() == nullptr)
        return;

    nativesharp::llvm_backend::ProjectOopCodePartition firstPartition;
    nativesharp::llvm_backend::ProjectOopCodePartition changedPartition;
    error.clear();
    const bool firstPartitionOk = firstPartition.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::StableIncremental);
    changedError.clear();
    const bool changedPartitionOk = changedPartition.build(
        *changed.oopMirModel(), 4, changedError,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::StableIncremental);
    tests.expect(firstPartitionOk && changedPartitionOk,
        "stable incremental partitions build for both revisions (" + error + changedError + ")");

    std::unordered_map<std::string, std::size_t> firstBuckets;
    const auto firstFunctions = first.oopMirModel()->functions();
    for (std::size_t ordinal = 0; ordinal < firstFunctions.size(); ++ordinal)
    {
        const auto bucket = firstPartition.unitForFunction(ordinal);
        if (bucket.has_value()) firstBuckets.emplace(firstFunctions[ordinal].mangledName, *bucket);
    }
    bool stableAssignments = firstFunctions.size() == changed.oopMirModel()->functions().size();
    const auto changedFunctions = changed.oopMirModel()->functions();
    for (std::size_t ordinal = 0; stableAssignments && ordinal < changedFunctions.size(); ++ordinal)
    {
        const auto bucket = changedPartition.unitForFunction(ordinal);
        const auto expected = firstBuckets.find(changedFunctions[ordinal].mangledName);
        stableAssignments = bucket.has_value() && expected != firstBuckets.end() &&
            *bucket == expected->second;
    }
    tests.expect(stableAssignments,
        "body changes preserve every physical function-to-bucket assignment");

    nativesharp::llvm_backend::ProjectOopCodePartition cacheAwareColdPartition;
    error.clear();
    const bool cacheAwareColdOk = cacheAwareColdPartition.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::CacheAwareBalanced);
    const std::string coldPlanPayload = cacheAwareColdPartition.persistentPlanPayload();
    tests.expect(cacheAwareColdOk && !coldPlanPayload.empty(),
        "cache-aware cold partition creates a balanced persistent plan (" + error + ")");

    nativesharp::llvm_backend::ProjectOopCodePartition referenceBalancedPartition;
    error.clear();
    const bool referenceBalancedOk = referenceBalancedPartition.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::Balanced);
    tests.expect(referenceBalancedOk &&
            referenceBalancedPartition.dump() == cacheAwareColdPartition.dump(),
        "cache-aware cold partition preserves the deterministic LPT balance (" + error + ")");

    nativesharp::llvm_backend::ProjectOopCodePartition invalidPlanFallback;
    error.clear();
    const bool invalidFallbackOk = invalidPlanFallback.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::CacheAwareBalanced,
        "invalid partition payload");
    tests.expect(invalidFallbackOk &&
            invalidPlanFallback.dump() == cacheAwareColdPartition.dump(),
        "an invalid private plan falls back to the same deterministic cold balance (" +
            error + ")");

    nativesharp::llvm_backend::CompilationArtifactCache cache;
    cache.setRoot(root / "incremental-object-cache");
    cache.setCodegenIdentity("precise-invalidation-test");
    nativesharp::llvm_backend::CompilationArtifactCacheScope cacheScope(cache);
    nativesharp::llvm_backend::ProjectOopObjectGenerator generator;
    error.clear();
    auto firstBundle = generator.generate(
        *first.nameLookup(), *first.memberModel(), *first.oopMirModel(),
        first.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, error);
    tests.expect(firstBundle != nullptr && firstBundle->cacheHitCount() == 0 &&
            firstBundle->cacheMissCount() == firstBundle->objectFiles().size(),
        "cold incremental build emits and stores every code unit (" + error + ")");
    if (firstBundle == nullptr) return;

    const std::string planKey =
        nativesharp::llvm_backend::ProjectOopCodePartition::persistentPlanKey(
            *first.oopMirModel(), 4);
    const std::string changedPlanKey =
        nativesharp::llvm_backend::ProjectOopCodePartition::persistentPlanKey(
            *changed.oopMirModel(), 4);
    tests.expect(planKey == changedPlanKey,
        "a body-only edit preserves the private code-partition topology key");
    std::string restoredPlanPayload;
    bool restoredPlanHit = false;
    error.clear();
    const bool restoredPlanOk = cache.restoreCodePartitionPlan(
        planKey, restoredPlanPayload, restoredPlanHit, error);
    tests.expect(restoredPlanOk && restoredPlanHit && !restoredPlanPayload.empty(),
        "cold generation atomically publishes its private code-partition plan (" + error + ")");

    nativesharp::llvm_backend::ProjectOopCodePartition restoredChangedPartition;
    changedError.clear();
    const bool restoredChangedOk = restoredChangedPartition.build(
        *changed.oopMirModel(), 4, changedError,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::CacheAwareBalanced,
        restoredPlanPayload);
    bool restoredAssignments = restoredChangedOk;
    std::unordered_map<std::string, std::size_t> cacheAwareBuckets;
    for (std::size_t ordinal = 0; ordinal < firstFunctions.size(); ++ordinal)
    {
        const auto bucket = cacheAwareColdPartition.unitForFunction(ordinal);
        if (bucket.has_value())
            cacheAwareBuckets.emplace(firstFunctions[ordinal].mangledName, *bucket);
    }
    for (std::size_t ordinal = 0;
        restoredAssignments && ordinal < changedFunctions.size(); ++ordinal)
    {
        const auto bucket = restoredChangedPartition.unitForFunction(ordinal);
        const auto expected = cacheAwareBuckets.find(changedFunctions[ordinal].mangledName);
        restoredAssignments = bucket.has_value() && expected != cacheAwareBuckets.end() &&
            *bucket == expected->second;
    }
    tests.expect(restoredAssignments,
        "a body-only edit restores the exact cold-balanced function-to-unit plan (" +
            changedError + ")");

    changedError.clear();
    auto changedBundle = generator.generate(
        *changed.nameLookup(), *changed.memberModel(), *changed.oopMirModel(),
        changed.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, changedError);
    tests.expect(changedBundle != nullptr && changedBundle->cacheMissCount() == 1 &&
            changedBundle->cacheHitCount() + changedBundle->cacheMissCount() ==
                changedBundle->objectFiles().size(),
        "one body edit invalidates exactly its code bucket while reusing coordinator and peers (" +
            changedError + ")");
    if (changedBundle == nullptr) return;

    changedError.clear();
    auto repeatedBundle = generator.generate(
        *changed.nameLookup(), *changed.memberModel(), *changed.oopMirModel(),
        changed.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, changedError);
    tests.expect(repeatedBundle != nullptr && repeatedBundle->cacheMissCount() == 0 &&
            repeatedBundle->cacheHitCount() == repeatedBundle->objectFiles().size(),
        "unchanged incremental rebuild restores every unit after the precise miss (" +
            changedError + ")");

    std::size_t evictedEntries = 0;
    std::uintmax_t evictedBytes = 0;
    error.clear();
    const bool pruneOk = cache.prune(0, evictedEntries, evictedBytes, error);
    std::string prunedPlanPayload;
    bool prunedPlanHit = true;
    const bool prunedPlanRestoreOk = cache.restoreCodePartitionPlan(
        planKey, prunedPlanPayload, prunedPlanHit, error);
    tests.expect(pruneOk && evictedEntries != 0U && evictedBytes != 0U &&
            prunedPlanRestoreOk && !prunedPlanHit,
        "the artifact-cache byte budget evicts private partition plans with objects (" +
            error + ")");
}

void testPartitionStaticFieldReachability(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "PartitionStaticFieldReachability";
    writeProject(directory, "PartitionStaticFieldReachability");
    writeFile(directory / "Fields.ns", R"NS(
class Catalog
{
    public static int Used = 41;
    public static int Unused = 99;
    public static int Read() { return Used; }
    public static int Other() { return Unused; }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(
        directory / "PartitionStaticFieldReachability.nsproj", error);
    tests.expect(built && compilation.oopMirModel() != nullptr,
        "static-field reachability fixture compiles through MIR (" + error + ")");
    if (!built || compilation.oopMirModel() == nullptr) return;

    const ProjectOopMirModel& mir = *compilation.oopMirModel();
    nativesharp::llvm_backend::ProjectOopCodePartition partition;
    tests.expect(partition.build(mir, 2, error),
        "static-field reachability fixture partitions into code units (" + error + ")");

    std::optional<std::size_t> readOrdinal;
    for (std::size_t ordinal = 0; ordinal < mir.functions().size(); ++ordinal)
        if (mir.functions()[ordinal].mangledName.find("Catalog__Read") != std::string::npos)
            readOrdinal = ordinal;
    const auto readUnit = readOrdinal.has_value()
        ? partition.unitForFunction(*readOrdinal) : std::nullopt;
    tests.expect(readUnit.has_value(), "Catalog.Read belongs to one stable code unit");
    if (!readUnit.has_value() || *readUnit >= partition.units().size()) return;

    const auto findField = [&](std::string_view name)
        -> const nativesharp::frontend::ProjectOopMirStaticField*
    {
        for (const auto& field : mir.staticFields())
            if (field.name == name) return &field;
        return nullptr;
    };
    const auto* used = findField("Used");
    const auto* unused = findField("Unused");
    tests.expect(used != nullptr && unused != nullptr,
        "fixture exposes distinct used and unused static fields in MIR");
    if (used == nullptr || unused == nullptr) return;

    const auto& unit = partition.units()[*readUnit];
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(nativesharp::llvm_backend::ProjectOopLlvmLoweringRequest::partition(
        *compilation.nameLookup(), *compilation.memberModel(), mir,
        compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
        unit.functionOrdinals, false, "PartitionStaticFieldReachability.read", {},
        unit.declarationOrdinals));
    tests.expect(module != nullptr && lowerer.error().empty(),
        "the Read code unit lowers with its reachable static-field closure (" +
            lowerer.error() + ")");
    if (module == nullptr) return;
    std::string verificationError;
    tests.expect(module->verify(verificationError),
        "the pruned static-field code unit remains verifier-clean (" + verificationError + ")");
    const std::string ir = module->text();
    tests.expect(ir.find(used->storageSymbol) != std::string::npos &&
            ir.find(unused->storageSymbol) == std::string::npos &&
            ir.find(unused->accessorName) == std::string::npos,
        "a code unit declares the static field it reads and omits the unrelated field");
}

void testImportedConstantCoordinatorReachability(TestContext& tests)
{
    nativesharp::frontend::ProjectOopMirStaticField localMutable;
    localMutable.name = "LocalMutable";

    nativesharp::frontend::ProjectOopMirStaticField importedMutable;
    importedMutable.name = "ImportedMutable";
    importedMutable.isImported = true;

    nativesharp::frontend::ProjectOopMirStaticField importedConstant;
    importedConstant.name = "ImportedConstant";
    importedConstant.isConst = true;
    importedConstant.isImported = true;

    std::vector<nativesharp::frontend::ProjectOopMirStaticField> fields;
    fields.push_back(std::move(localMutable));
    fields.push_back(std::move(importedMutable));
    fields.push_back(std::move(importedConstant));
    const std::vector<std::uint8_t> mask =
        nativesharp::frontend::ProjectOopHirModelTestAccess::coordinatorStaticFieldMask(
            std::move(fields));

    tests.expect(mask == std::vector<std::uint8_t>{1U, 0U, 1U},
        "the coordinator owns local fields and imported constants but not imported mutable storage");
}

void testDeepOptionalDefaultHir(TestContext& tests, const std::filesystem::path& root)
{
    constexpr std::size_t unaryDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(unaryDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < unaryDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::UnaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Minus);
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    ProjectOopHirModel model;
    std::string error;
    std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::lowerDefaultArgument(
            model, current, "builtin:int", error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level unary optional default lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    const nativesharp::frontend::ProjectOopHirNode* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Unary)
    {
        ++observedDepth;
        cursor = cursor->children.size() == 1U ? cursor->children.front().get() : nullptr;
    }
    tests.expect(observedDepth == unaryDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant &&
            cursor->integerValue == 1U,
        "the iterative optional-default lowering preserves all 32768 unary nodes and the leaf");

    // Do not let unique_ptr destruction itself become the recursive operation this test excludes.
    while (lowered != nullptr)
    {
        std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> child;
        if (!lowered->children.empty()) child = std::move(lowered->children.front());
        lowered->children.clear();
        lowered = std::move(child);
    }

    const std::filesystem::path directory = root / "OptionalDefaultSourceContract";
    writeProject(directory, "OptionalDefaultSourceContract");
    writeFile(directory / "Main.ns",
        "namespace Deep; public class Probe { "
        "public int Value(int input = -1) { return input; } "
        "public int Run() { return Value(); } }");
    ProjectCompilation compilation;
    error.clear();
    const bool built = compilation.compileProjectOopHir(
        directory / "OptionalDefaultSourceContract.nsproj", error);
    tests.expect(built && compilation.oopHirModel() != nullptr,
        "the language-permitted unary optional default still reaches OOP HIR (" + error + ")");
    if (!built || compilation.oopHirModel() == nullptr) return;

    std::size_t unaryCount = 0;
    std::vector<const nativesharp::frontend::ProjectOopHirNode*> pending;
    for (const auto& function : compilation.oopHirModel()->functions())
        if (function.body != nullptr) pending.push_back(function.body.get());
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        unaryCount += node->kind == nativesharp::frontend::ProjectOopHirNodeKind::Unary ? 1U : 0U;
        for (const auto& child : node->children)
            if (child != nullptr) pending.push_back(child.get());
    }
    tests.expect(unaryCount == 1U,
        "the permitted -1 default preserves exactly one unary HIR node at its call site");
}

void testDeepYieldStateAssignment(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    nativesharp::frontend::ProjectOopHirNode* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto yield = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        yield->kind = nativesharp::frontend::ProjectOopHirNodeKind::YieldValue;
        cursor->children.push_back(std::move(yield));
        if (index + 1U != nestingDepth)
        {
            auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
            block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
            cursor->children.push_back(std::move(block));
            cursor = cursor->children.back().get();
        }
    }

    ProjectOopHirModel model;
    std::size_t nextYieldState = 0;
    nativesharp::frontend::ProjectOopHirModelTestAccess::assignYieldStates(
        model, root.get(), nextYieldState);
    tests.expect(nextYieldState == nestingDepth,
        "iterative async yield numbering reaches all 32768 nested blocks");

    bool ordered = true;
    cursor = root.get();
    for (std::size_t expected = 1; expected <= nestingDepth; ++expected)
    {
        ordered = ordered && !cursor->children.empty() &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::YieldValue &&
            cursor->children.front()->integerValue == expected;
        if (expected != nestingDepth)
        {
            ordered = ordered && cursor->children.size() == 2U;
            cursor = cursor->children.back().get();
        }
    }
    tests.expect(ordered,
        "iterative async yield numbering preserves recursive preorder exactly");

    while (root != nullptr)
    {
        std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> next;
        if (root->children.size() == 2U) next = std::move(root->children.back());
        root->children.clear();
        root = std::move(next);
    }
}

void testDeepGeneratorThrowRewrite(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(block));
        cursor = cursor->children.front().get();
    }
    auto throwing = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    throwing->kind = nativesharp::frontend::ProjectOopHirNodeKind::Throw;
    auto payload = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    payload->kind = nativesharp::frontend::ProjectOopHirNodeKind::NullConstant;
    payload->typeKey = "type:Deep.Error";
    throwing->children.push_back(std::move(payload));
    cursor->children.push_back(std::move(throwing));

    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::rewriteGeneratorThrows(
        model, root.get(), "type:Deep.Error", "iterator:builtin:int:type:Deep.Error");
    tests.expect(cursor->children.size() == 1U &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            cursor->children.front()->children.size() == 3U &&
            cursor->children.front()->children.back()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Return,
        "iterative generator throw rewrite reaches and replaces a node at depth 32768");

    std::size_t removedDepth = 0;
    while (root != nullptr && root->children.size() == 1U &&
        root->children.front()->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block &&
        root->children.front()->children.size() == 1U)
    {
        auto next = std::move(root->children.front());
        root->children.clear();
        root = std::move(next);
        ++removedDepth;
    }
    tests.expect(removedDepth == nestingDepth,
        "the deep generator throw fixture retains every enclosing block");
}

void testDeepAsyncPollReturnRewrite(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(block));
        cursor = cursor->children.front().get();
    }
    auto returning = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    returning->kind = nativesharp::frontend::ProjectOopHirNodeKind::Return;
    returning->cleanupRegionOrdinals = {7U, 11U};
    returning->cleanupContinuationKind =
        nativesharp::frontend::MethodCleanupContinuationKind::Return;
    auto payload = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    payload->kind = nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant;
    payload->typeKey = "builtin:int";
    payload->integerValue = 42U;
    returning->children.push_back(std::move(payload));
    cursor->children.push_back(std::move(returning));

    ProjectOopHirModel model;
    std::string error;
    root = nativesharp::frontend::ProjectOopHirModelTestAccess::rewriteAsyncPollReturns(
        model,
        std::move(root),
        3U,
        "builtin:int",
        "future:builtin:int",
        error,
        true);
    tests.expect(root != nullptr && error.empty(),
        "a 32768-level async poll return rewrites without process-stack recursion (" + error + ")");

    cursor = root.get();
    bool enclosingBlocksPreserved = cursor != nullptr;
    for (std::size_t index = 0; index < nestingDepth && enclosingBlocksPreserved; ++index)
    {
        enclosingBlocksPreserved = !cursor->children.empty() &&
            cursor->children.front() != nullptr &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Block;
        if (enclosingBlocksPreserved) cursor = cursor->children.front().get();
    }
    tests.expect(enclosingBlocksPreserved && cursor != nullptr &&
            cursor->children.size() == 1U &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            cursor->children.front()->children.size() == 3U &&
            cursor->children.front()->children.back()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Return &&
            cursor->children.front()->children.back()->cleanupRegionOrdinals ==
                std::vector<std::size_t>({7U, 11U}) &&
            cursor->children.front()->children.back()->cleanupContinuationKind ==
                nativesharp::frontend::MethodCleanupContinuationKind::Return,
        "iterative async poll return rewrite preserves every block and cleanup continuation");

    tests.expect(root != nullptr && root->children.size() == 2U &&
            root->children.back()->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            root->children.back()->children.size() == 3U &&
            root->children.back()->children.back()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Return,
        "top-level fallthrough still appends exactly one implicit ready return");
    destroyHirTreeIteratively(root);

    auto throwRoot = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    throwRoot->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    cursor = throwRoot.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(block));
        cursor = cursor->children.front().get();
    }
    auto throwing = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    throwing->kind = nativesharp::frontend::ProjectOopHirNodeKind::Throw;
    throwing->cleanupRegionOrdinals = {13U};
    throwing->cleanupContinuationKind =
        nativesharp::frontend::MethodCleanupContinuationKind::Exception;
    auto errorPayload = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    errorPayload->kind = nativesharp::frontend::ProjectOopHirNodeKind::NullConstant;
    errorPayload->typeKey = "type:Deep.Error";
    throwing->children.push_back(std::move(errorPayload));
    cursor->children.push_back(std::move(throwing));

    error.clear();
    throwRoot = nativesharp::frontend::ProjectOopHirModelTestAccess::rewriteAsyncPollReturns(
        model,
        std::move(throwRoot),
        3U,
        "builtin:int",
        "future:builtin:int",
        error,
        false);
    tests.expect(throwRoot != nullptr && error.empty(),
        "a 32768-level async poll throw rewrites without process-stack recursion (" + error + ")");

    cursor = throwRoot.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
        cursor = cursor->children.front().get();
    const auto* errorBlock = cursor->children.front().get();
    tests.expect(errorBlock->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            errorBlock->children.size() == 3U &&
            errorBlock->children[0]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Assignment &&
            errorBlock->children[0]->children[0]->memberOrdinal == 4U &&
            errorBlock->children[0]->children[1]->typeKey == "type:Deep.Error" &&
            errorBlock->children[1]->children[1]->integerValue == (~0ULL - 1ULL) &&
            errorBlock->children[2]->cleanupRegionOrdinals == std::vector<std::size_t>({13U}) &&
            errorBlock->children[2]->cleanupContinuationKind ==
                nativesharp::frontend::MethodCleanupContinuationKind::Exception,
        "iterative async poll throw rewrite preserves error field, state -2, payload, and cleanup");
    destroyHirTreeIteratively(throwRoot);
}

void testDeepAsyncAwaitScan(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 2U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IdentifierExpression,
        nativesharp::SourceRange{}));
    nativesharp::ast::AstNode* callSyntax = syntax.back().get();
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::AwaitExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setFirst(callSyntax);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::UnaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodCallBinding binding;
    binding.selectedMemberOrdinal = 17U;
    ProjectOopHirModel model;
    const auto ordinals =
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectAwaitedMemberOrdinals(
            model, current, callSyntax, binding);
    tests.expect(ordinals == std::vector<std::size_t>({17U}),
        "iterative async await scan reaches a bound await through 32768 AST levels");
}

void testDeepBuiltinUnaryExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::UnaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Minus);
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level builtin unary expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Unary)
    {
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative builtin unary lowering preserves every operator and its literal leaf");
    destroyHirTreeIteratively(lowered);
}

struct UserUnaryOperatorTarget final
{
    std::size_t operatorOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string nodeType;
};

bool prepareUserUnaryOperatorFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    UserUnaryOperatorTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepUserUnaryOperator";
    writeProject(directory, "DeepUserUnaryOperator");
    writeFile(directory / "Main.ns", R"ns(
namespace Deep {
public trait IAdd<T> {
    public static T operator +(in T left, in T right);
}
public struct Node : IAdd<Node> {
    public static Node operator -(Node value) { return value; }
    public Node Run() { return -this; }
}
}
)ns");
    if (!compilation.compileProjectOopHir(directory / "DeepUserUnaryOperator.nsproj", error) ||
        compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return false;
    }
    bool foundOperator = false;
    bool foundCaller = false;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const auto& member = members[ordinal];
        if (member.isUserOperator && member.parameters.size() == 1U &&
            member.parameters.front().passKind == nativesharp::frontend::ParameterPassKind::Value)
        {
            output.operatorOrdinal = ordinal;
            output.nodeType = member.declaredTypeKey;
            foundOperator = true;
        }
        if (!member.isUserOperator && member.name == "Run")
        {
            output.callerOrdinal = ordinal;
            foundCaller = true;
        }
    }
    return foundOperator && foundCaller && !output.nodeType.empty();
}

struct DeepUserUnaryOperatorSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepUserUnaryOperatorSyntax makeDeepUserUnaryOperatorSyntax(
    std::size_t depth,
    const UserUnaryOperatorTarget& target)
{
    DeepUserUnaryOperatorSyntax fixture;
    fixture.nodes.reserve(depth + 1U);
    fixture.calls.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            index % 2U == 0U ? nativesharp::ast::AstNodeKind::UnaryExpression
                             : nativesharp::ast::AstNodeKind::PostfixExpression,
            nativesharp::SourceRange{}));
        auto* operation = fixture.nodes.back().get();
        operation->setTokenKind(nativesharp::lexer::TokenKind::Minus);
        operation->setFirst(current);
        fixture.calls.emplace_back();
        auto& call = fixture.calls.back();
        call.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        call.syntax = operation;
        call.staticCall = true;
        call.selectedMemberOrdinal = target.operatorOrdinal;
        call.resultTypeKey = target.nodeType;
        current = operation;
    }
    fixture.root = current;
    return fixture;
}

bool userUnaryOperatorChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    const UserUnaryOperatorTarget& target)
{
    std::size_t observedDepth = 0;
    while (node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::StaticCall)
    {
        const std::size_t sourceIndex = expectedDepth - observedDepth - 1U;
        const auto expectedSyntax = sourceIndex % 2U == 0U
            ? nativesharp::ast::AstNodeKind::UnaryExpression
            : nativesharp::ast::AstNodeKind::PostfixExpression;
        if (node->syntax == nullptr || node->syntax->kind() != expectedSyntax ||
            node->memberOrdinal != target.operatorOrdinal || node->typeKey != target.nodeType ||
            node->children.size() != 1U || node->interfaceDispatch || node->baseReceiver ||
            node->nullConditionalCall || node->referenceResult)
        {
            return false;
        }
        ++observedDepth;
        node = node->children.front().get();
    }
    return observedDepth == expectedDepth && node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::PointerLoad &&
        node->typeKey == target.nodeType && node->children.size() == 1U &&
        node->children.front()->kind ==
            nativesharp::frontend::ProjectOopHirNodeKind::ThisValue &&
        node->children.front()->typeKey == target.nodeType;
}

void testDeepUserUnaryOperatorLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    UserUnaryOperatorTarget target;
    std::string error;
    const bool prepared = prepareUserUnaryOperatorFixture(root, compilation, target, error);
    tests.expect(prepared, "the user-unary fixture builds a by-value operator target (" + error + ")");
    if (!prepared) return;

    DeepUserUnaryOperatorSyntax syntax =
        makeDeepUserUnaryOperatorSyntax(nestingDepth, target);
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.nodeType;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerUserUnaryOperatorExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(), target.callerOrdinal,
            syntax.root, syntax.nodes, syntax.calls, type, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level unary/postfix user-operator chain lowers without stack recursion (" +
            error + ")");
    tests.expect(userUnaryOperatorChainMatches(lowered.get(), nestingDepth, target),
        "iterative user-operator lowering preserves passKind, member ordinal, order, and syntax");
    destroyHirTreeIteratively(lowered);
}

struct UserBinaryOperatorTarget final
{
    std::size_t operatorOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string nodeType;
};

bool prepareUserBinaryOperatorFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    UserBinaryOperatorTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepUserBinaryOperator";
    writeProject(directory, "DeepUserBinaryOperator");
    writeFile(directory / "Main.ns", R"ns(
namespace Deep {
public struct Node {
    public int Value;
    public Node(int value) { Value = value; }
    public static Node operator +(Node left, Node right) {
        return new Node(left.Value + right.Value);
    }
    public Node Run(Node other) { return this + other; }
}
}
)ns");
    if (!compilation.compileProjectOopHir(directory / "DeepUserBinaryOperator.nsproj", error) ||
        compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return false;
    }
    bool foundOperator = false;
    bool foundCaller = false;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const auto& member = members[ordinal];
        if (member.isUserOperator && member.parameters.size() == 2U &&
            member.parameters[0].passKind == nativesharp::frontend::ParameterPassKind::Value &&
            member.parameters[1].passKind == nativesharp::frontend::ParameterPassKind::Value)
        {
            output.operatorOrdinal = ordinal;
            output.nodeType = member.declaredTypeKey;
            foundOperator = true;
        }
        if (!member.isUserOperator && member.name == "Run")
        {
            output.callerOrdinal = ordinal;
            foundCaller = true;
        }
    }
    return foundOperator && foundCaller && !output.nodeType.empty();
}

struct DeepUserBinaryOperatorSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepUserBinaryOperatorSyntax makeDeepUserBinaryOperatorSyntax(
    std::size_t depth,
    const UserBinaryOperatorTarget& target)
{
    DeepUserBinaryOperatorSyntax fixture;
    fixture.nodes.reserve(depth * 2U + 1U);
    fixture.calls.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
        auto* right = fixture.nodes.back().get();
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BinaryExpression, nativesharp::SourceRange{}));
        auto* operation = fixture.nodes.back().get();
        operation->setTokenKind(nativesharp::lexer::TokenKind::Plus);
        operation->setFirst(current);
        operation->setSecond(right);
        fixture.calls.emplace_back();
        auto& call = fixture.calls.back();
        call.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        call.syntax = operation;
        call.staticCall = true;
        call.selectedMemberOrdinal = target.operatorOrdinal;
        call.resultTypeKey = target.nodeType;
        current = operation;
    }
    fixture.root = current;
    return fixture;
}

bool userBinaryOperatorChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    const UserBinaryOperatorTarget& target)
{
    std::size_t observedDepth = 0;
    while (node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::StaticCall)
    {
        if (node->syntax == nullptr ||
            node->syntax->kind() != nativesharp::ast::AstNodeKind::BinaryExpression ||
            node->memberOrdinal != target.operatorOrdinal || node->typeKey != target.nodeType ||
            node->children.size() != 2U || node->interfaceDispatch || node->baseReceiver ||
            node->nullConditionalCall || node->referenceResult ||
            node->children[1]->kind != nativesharp::frontend::ProjectOopHirNodeKind::PointerLoad ||
            node->children[1]->children.size() != 1U ||
            node->children[1]->children.front()->kind !=
                nativesharp::frontend::ProjectOopHirNodeKind::ThisValue)
        {
            return false;
        }
        ++observedDepth;
        node = node->children.front().get();
    }
    return observedDepth == expectedDepth && node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::PointerLoad &&
        node->typeKey == target.nodeType && node->children.size() == 1U &&
        node->children.front()->kind ==
            nativesharp::frontend::ProjectOopHirNodeKind::ThisValue &&
        node->children.front()->typeKey == target.nodeType;
}

bool userBinaryPreparationMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    const UserBinaryOperatorTarget& target)
{
    if (node == nullptr ||
        node->kind != nativesharp::frontend::ProjectOopHirNodeKind::FullExpressionCleanup ||
        node->children.size() != 1U)
        return false;
    node = node->children.front().get();
    if (node->kind != nativesharp::frontend::ProjectOopHirNodeKind::StaticCall ||
        node->memberOrdinal != target.operatorOrdinal || node->children.size() != 2U)
        return false;
    for (std::size_t index = 0; index < node->children.size(); ++index)
    {
        const auto* temporary = node->children[index].get();
        if (temporary->kind != nativesharp::frontend::ProjectOopHirNodeKind::OwnedTemporaryBorrow ||
            temporary->localOrdinal != index || temporary->children.size() != 1U)
            return false;
        const auto* borrow = temporary->children.front().get();
        if (borrow->kind != nativesharp::frontend::ProjectOopHirNodeKind::BorrowValue ||
            borrow->borrowKind != nativesharp::frontend::MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter ||
            borrow->children.size() != 1U)
            return false;
        const auto* conversion = borrow->children.front().get();
        if (conversion->kind != nativesharp::frontend::ProjectOopHirNodeKind::Conversion ||
            !conversion->conversion.has_value() ||
            conversion->conversion->kind != nativesharp::frontend::ProjectConversionKind::Numeric ||
            conversion->children.size() != 1U ||
            conversion->children.front()->kind !=
                nativesharp::frontend::ProjectOopHirNodeKind::ThisValue)
            return false;
    }
    return true;
}

void testUserBinaryArgumentPreparation(
    TestContext& tests,
    ProjectCompilation& compilation,
    const UserBinaryOperatorTarget& target,
    const nativesharp::frontend::MethodExpressionTypeBinding& type)
{
    auto& operatorMember = const_cast<nativesharp::frontend::ClassMemberSymbol&>(
        compilation.memberModel()->members()[target.operatorOrdinal]);
    operatorMember.parameters[0].passKind = nativesharp::frontend::ParameterPassKind::In;
    operatorMember.parameters[1].passKind = nativesharp::frontend::ParameterPassKind::In;
    DeepUserBinaryOperatorSyntax shallow = makeDeepUserBinaryOperatorSyntax(1U, target);
    nativesharp::frontend::ProjectImplicitConversion conversion;
    conversion.kind = nativesharp::frontend::ProjectConversionKind::Numeric;
    conversion.sourceTypeKey = target.nodeType;
    conversion.targetTypeKey = target.nodeType;
    shallow.calls.front().selectedConversions = {conversion, conversion};
    std::vector<nativesharp::frontend::MethodBorrowBinding> borrows(2U);
    for (std::size_t index = 0; index < borrows.size(); ++index)
    {
        borrows[index].kind = nativesharp::frontend::MethodBorrowKind::
            FreshOwnedTemporaryToBorrowedParameter;
        borrows[index].syntax = shallow.nodes[index].get();
        borrows[index].methodMemberOrdinal = target.callerOrdinal;
        borrows[index].targetMemberOrdinal = target.operatorOrdinal;
        borrows[index].targetParameterOrdinal = index;
        borrows[index].typeKey = target.nodeType;
    }
    std::string error;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerUserBinaryOperatorExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(), target.callerOrdinal,
            shallow.root, shallow.nodes, shallow.calls, borrows, type, error);
    tests.expect(lowered != nullptr && error.empty() &&
        userBinaryPreparationMatches(lowered.get(), target),
        "binary operands apply In address context, conversion, and borrow immediately in order");
    destroyHirTreeIteratively(lowered);
}

void testDeepUserBinaryOperatorLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    UserBinaryOperatorTarget target;
    std::string error;
    const bool prepared = prepareUserBinaryOperatorFixture(root, compilation, target, error);
    tests.expect(prepared, "the user-binary fixture builds a trait-backed two-in-parameter operator (" +
        error + ")");
    if (!prepared) return;

    DeepUserBinaryOperatorSyntax syntax =
        makeDeepUserBinaryOperatorSyntax(nestingDepth, target);
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.nodeType;
    const std::vector<nativesharp::frontend::MethodBorrowBinding> noBorrows;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerUserBinaryOperatorExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(), target.callerOrdinal,
            syntax.root, syntax.nodes, syntax.calls, noBorrows, type, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level binary user-operator chain lowers without stack recursion (" +
            error + ")");
    tests.expect(userBinaryOperatorChainMatches(lowered.get(), nestingDepth, target),
        "iterative binary user-operator lowering preserves passKind, operands, order, and syntax");
    destroyHirTreeIteratively(lowered);

    testUserBinaryArgumentPreparation(tests, compilation, target, type);
}

struct EventSubscriptionTargets final
{
    nativesharp::frontend::MethodEventBinding backingAdd;
    nativesharp::frontend::MethodEventBinding backingRemove;
    nativesharp::frontend::MethodEventBinding customAdd;
    nativesharp::frontend::MethodEventBinding customRemove;
    nativesharp::frontend::MethodEventBinding structAdd;
    std::size_t callerOrdinal = 0U;
};

bool prepareEventSubscriptionFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    EventSubscriptionTargets& output,
    std::string& error)
{
    const auto directory = root / "DeepEventSubscription";
    writeProject(directory, "DeepEventSubscription");
    writeFile(directory / "Main.ns", R"ns(
namespace Deep {
public delegate void Handler();
public class Publisher {
    public event Handler Backing;
    public event Handler Custom { add { } remove { } }
    public void Touch(Handler h) {
        Backing += h; Backing -= h; Custom += h; Custom -= h;
    }
}
public struct StructPublisher {
    public event Handler Backing;
    public void Touch(Handler h) { Backing += h; }
}
}
)ns");
    if (!compilation.compileProjectOopHir(
            directory / "DeepEventSubscription.nsproj", error) ||
        compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.methodBodyModel() == nullptr)
        return false;
    bool backingAdd = false;
    bool backingRemove = false;
    bool customAdd = false;
    bool customRemove = false;
    bool structAdd = false;
    for (const auto& binding : compilation.methodBodyModel()->eventBindings())
    {
        const bool isStruct =
            binding.receiverTypeKey.find("StructPublisher") != std::string::npos;
        const bool isCustom = binding.customAccessorMemberOrdinal.has_value();
        if (isStruct && binding.kind == nativesharp::frontend::MethodEventOperationKind::Add)
        {
            output.structAdd = binding;
            structAdd = true;
        }
        else if (!isCustom && binding.kind == nativesharp::frontend::MethodEventOperationKind::Add)
        {
            output.backingAdd = binding;
            output.callerOrdinal = binding.methodMemberOrdinal;
            backingAdd = true;
        }
        else if (!isCustom)
        {
            output.backingRemove = binding;
            backingRemove = true;
        }
        else if (binding.kind == nativesharp::frontend::MethodEventOperationKind::Add)
        {
            output.customAdd = binding;
            customAdd = true;
        }
        else
        {
            output.customRemove = binding;
            customRemove = true;
        }
    }
    return backingAdd && backingRemove && customAdd && customRemove && structAdd;
}

struct DeepEventSubscriptionSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodEventBinding> bindings;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepEventSubscriptionSyntax makeDeepEventSubscriptionSyntax(
    std::size_t depth,
    const EventSubscriptionTargets& targets)
{
    using EventKind = nativesharp::frontend::MethodEventOperationKind;
    DeepEventSubscriptionSyntax fixture;
    fixture.nodes.reserve(depth * 2U + 1U);
    fixture.bindings.reserve(depth);
    const auto makeNull = [&]()
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::NullLiteralExpression,
            nativesharp::SourceRange{}));
        return fixture.nodes.back().get();
    };
    auto* current = makeNull();
    for (std::size_t index = 0; index < depth; ++index)
    {
        const auto* source = index % 4U == 0U ? &targets.backingAdd
            : index % 4U == 1U ? &targets.customRemove
            : index % 4U == 2U ? &targets.customAdd
                               : &targets.backingRemove;
        fixture.bindings.push_back(*source);
        auto& binding = fixture.bindings.back();
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::AssignmentExpression,
            nativesharp::SourceRange{}));
        auto* assignment = fixture.nodes.back().get();
        assignment->setTokenKind(binding.kind == EventKind::Add
            ? nativesharp::lexer::TokenKind::PlusEqual
            : nativesharp::lexer::TokenKind::MinusEqual);
        binding.syntax = assignment;
        binding.staticEvent = false;
        if (index % 2U == 0U)
        {
            binding.receiverSyntax = nullptr;
            assignment->setSecond(current);
        }
        else
        {
            binding.receiverSyntax = current;
            assignment->setSecond(makeNull());
        }
        current = assignment;
    }
    fixture.root = current;
    return fixture;
}

bool eventSubscriptionChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    const DeepEventSubscriptionSyntax& fixture)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    for (std::size_t remaining = fixture.bindings.size(); remaining > 0U; --remaining)
    {
        const auto& binding = fixture.bindings[remaining - 1U];
        const bool custom = binding.customAccessorMemberOrdinal.has_value();
        const HirKind expected = custom ? HirKind::InstanceCall
            : binding.kind == nativesharp::frontend::MethodEventOperationKind::Add
                ? HirKind::EventAdd : HirKind::EventRemove;
        if (node == nullptr || node->kind != expected || node->syntax != binding.syntax ||
            node->typeKey != "builtin:void" || node->children.size() != 2U)
            return false;
        const nativesharp::frontend::ProjectOopHirNode* receiver = nullptr;
        if (custom)
        {
            if (node->memberOrdinal != binding.customAccessorMemberOrdinal ||
                node->argumentParameterOrdinals != std::vector<std::size_t>{0U} ||
                node->argumentTypeKeys != std::vector<std::string>{binding.delegateTypeKey})
                return false;
            receiver = node->children[0].get();
        }
        else
        {
            const auto* address = node->children[0].get();
            if (node->memberOrdinal != binding.backingMemberOrdinal ||
                node->elementTypeKey != binding.delegateTypeKey || address == nullptr ||
                address->kind != HirKind::FieldAddress ||
                address->memberOrdinal != binding.backingMemberOrdinal ||
                address->children.size() != 1U)
                return false;
            receiver = address->children.front().get();
        }
        const bool explicitReceiver = binding.receiverSyntax != nullptr;
        const auto* other = explicitReceiver ? node->children[1].get() : receiver;
        if (other == nullptr || other->kind !=
                (explicitReceiver ? HirKind::NullConstant : HirKind::ThisValue))
            return false;
        node = explicitReceiver ? receiver : node->children[1].get();
    }
    return node != nullptr && node->kind == HirKind::NullConstant;
}

struct SingleEventSubscriptionSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodEventBinding> bindings;
    const nativesharp::ast::AstNode* root = nullptr;
};

SingleEventSubscriptionSyntax makeSingleEventSubscriptionSyntax(
    const nativesharp::frontend::MethodEventBinding& source,
    bool staticEvent,
    bool explicitReceiver)
{
    SingleEventSubscriptionSyntax fixture;
    const auto makeNull = [&]()
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::NullLiteralExpression,
            nativesharp::SourceRange{}));
        return fixture.nodes.back().get();
    };
    auto* receiver = explicitReceiver ? makeNull() : nullptr;
    auto* handler = makeNull();
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::AssignmentExpression,
        nativesharp::SourceRange{}));
    auto* assignment = fixture.nodes.back().get();
    assignment->setTokenKind(source.kind ==
            nativesharp::frontend::MethodEventOperationKind::Add
        ? nativesharp::lexer::TokenKind::PlusEqual
        : nativesharp::lexer::TokenKind::MinusEqual);
    assignment->setSecond(handler);
    fixture.bindings.push_back(source);
    fixture.bindings.front().syntax = assignment;
    fixture.bindings.front().receiverSyntax = receiver;
    fixture.bindings.front().staticEvent = staticEvent;
    fixture.root = assignment;
    return fixture;
}

std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowerSingleEventSubscription(
    ProjectCompilation& compilation,
    std::size_t callerOrdinal,
    SingleEventSubscriptionSyntax& fixture,
    bool addressContext,
    const nativesharp::frontend::MethodLambdaBinding* currentLambda,
    std::string& error)
{
    ProjectOopHirModel model;
    return nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerEventSubscriptionExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(), callerOrdinal,
            fixture.root, fixture.bindings, addressContext, currentLambda, error);
}

void testEventSubscriptionContracts(
    TestContext& tests,
    ProjectCompilation& compilation,
    const EventSubscriptionTargets& targets)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    std::string error;
    auto staticFixture = makeSingleEventSubscriptionSyntax(
        targets.backingAdd, true, false);
    auto staticNode = lowerSingleEventSubscription(
        compilation, targets.callerOrdinal, staticFixture, false, nullptr, error);
    tests.expect(staticNode != nullptr && error.empty() &&
            staticNode->kind == HirKind::EventAdd && staticNode->children.size() == 2U &&
            staticNode->children[0]->kind == HirKind::StaticFieldAddress,
        "iterative event lowering preserves static backing address before handler");
    destroyHirTreeIteratively(staticNode);

    error.clear();
    auto structFixture = makeSingleEventSubscriptionSyntax(
        targets.structAdd, false, true);
    auto structNode = lowerSingleEventSubscription(
        compilation, targets.callerOrdinal, structFixture, false, nullptr, error);
    const auto* fieldAddress = structNode != nullptr && !structNode->children.empty()
        ? structNode->children.front().get() : nullptr;
    const auto* temporary = fieldAddress != nullptr && !fieldAddress->children.empty()
        ? fieldAddress->children.front().get() : nullptr;
    tests.expect(structNode != nullptr && error.empty() &&
            structNode->kind == HirKind::EventAdd && fieldAddress != nullptr &&
            fieldAddress->kind == HirKind::FieldAddress && temporary != nullptr &&
            temporary->kind == HirKind::TemporaryAddress,
        "iterative event lowering materializes an explicit struct temporary exactly once");
    destroyHirTreeIteratively(structNode);

    error.clear();
    auto convertedFixture = makeSingleEventSubscriptionSyntax(
        targets.customAdd, false, false);
    nativesharp::frontend::ProjectImplicitConversion conversion;
    conversion.kind = nativesharp::frontend::ProjectConversionKind::Numeric;
    conversion.sourceTypeKey = "builtin:null";
    conversion.targetTypeKey = targets.customAdd.delegateTypeKey;
    convertedFixture.bindings.front().argumentConversions = {conversion};
    auto convertedNode = lowerSingleEventSubscription(
        compilation, targets.callerOrdinal, convertedFixture, false, nullptr, error);
    tests.expect(convertedNode != nullptr && error.empty() &&
            convertedNode->kind == HirKind::InstanceCall &&
            convertedNode->children.size() == 2U &&
            convertedNode->children[1]->kind == HirKind::Conversion,
        "iterative custom event lowering preserves its selected handler conversion");
    destroyHirTreeIteratively(convertedNode);
}

void testEventSubscriptionDiagnostics(
    TestContext& tests,
    ProjectCompilation& compilation,
    const EventSubscriptionTargets& targets)
{
    std::string error;
    auto addressFixture = makeSingleEventSubscriptionSyntax(
        targets.customAdd, false, false);
    auto addressNode = lowerSingleEventSubscription(
        compilation, targets.callerOrdinal, addressFixture, true, nullptr, error);
    const bool addressExact = addressNode == nullptr &&
        error == "Stage 42 event subscription expression is not addressable";

    error.clear();
    auto backingFixture = makeSingleEventSubscriptionSyntax(
        targets.backingAdd, false, false);
    backingFixture.bindings.front().backingMemberOrdinal =
        compilation.memberModel()->members().size();
    auto backingNode = lowerSingleEventSubscription(
        compilation, targets.callerOrdinal, backingFixture, false, nullptr, error);
    const bool backingExact = backingNode == nullptr &&
        error == "Stage 42 event binding has invalid backing member";

    error.clear();
    auto captureFixture = makeSingleEventSubscriptionSyntax(
        targets.customAdd, false, false);
    nativesharp::frontend::MethodLambdaBinding lambda;
    auto captureNode = lowerSingleEventSubscription(
        compilation, targets.callerOrdinal, captureFixture, false, &lambda, error);
    const bool captureExact = captureNode == nullptr &&
        error == "Stage 43 implicit instance receiver has no captured this";
    tests.expect(addressExact && backingExact && captureExact,
        "iterative event lowering preserves address, backing, and captured-this diagnostics byte-exactly");
}

void testDeepEventSubscriptionLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768U;
    ProjectCompilation compilation;
    EventSubscriptionTargets targets;
    std::string error;
    const bool prepared = prepareEventSubscriptionFixture(
        root, compilation, targets, error);
    tests.expect(prepared,
        "the event-subscription fixture exposes backing/custom and class/struct bindings (" +
            error + ")");
    if (!prepared) return;
    DeepEventSubscriptionSyntax syntax =
        makeDeepEventSubscriptionSyntax(nestingDepth, targets);
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerEventSubscriptionExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(),
            targets.callerOrdinal, syntax.root, syntax.bindings, false, nullptr, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level event receiver/handler chain lowers without stack recursion (" +
            error + ")");
    tests.expect(eventSubscriptionChainMatches(lowered.get(), syntax),
        "iterative event lowering preserves receiver-before-handler order and backing/custom add/remove shape");
    destroyHirTreeIteratively(lowered);
    testEventSubscriptionContracts(tests, compilation, targets);
    testEventSubscriptionDiagnostics(tests, compilation, targets);
}

struct PropertyIndexerSetterTargets final
{
    nativesharp::frontend::MethodCallBinding propertyGetter;
    nativesharp::frontend::MethodCallBinding propertySetter;
    nativesharp::frontend::MethodCallBinding indexerGetter;
    nativesharp::frontend::MethodCallBinding indexerSetter;
    std::size_t callerOrdinal = 0U;
    std::string receiverType;
    std::string valueType;
};

bool preparePropertyIndexerSetterFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    PropertyIndexerSetterTargets& output,
    std::string& error)
{
    const auto directory = root / "DeepPropertyIndexerSetter";
    writeProject(directory, "DeepPropertyIndexerSetter");
    writeFile(directory / "Main.ns", R"ns(
namespace Deep {
public class Box {
    private int slot;
    public int Value { get { return slot; } set { slot = value; } }
    public int this[int row, int column] {
        get { return slot + row + column; }
        set { slot = value - row - column; }
    }
    public int Run(int x) {
        Value = x; Value += x; this[1, 2] = x; this[3, 4] += x; return x;
    }
}
}
)ns");
    if (!compilation.compileProjectOopHir(
            directory / "DeepPropertyIndexerSetter.nsproj", error) ||
        compilation.memberModel() == nullptr || compilation.methodBodyModel() == nullptr)
        return false;
    bool found[4] = {};
    const auto& members = compilation.memberModel()->members();
    for (const auto& call : compilation.methodBodyModel()->calls())
    {
        if (!call.selectedMemberOrdinal.has_value() ||
            *call.selectedMemberOrdinal >= members.size()) continue;
        const auto& member = members[*call.selectedMemberOrdinal];
        const std::size_t slot = member.isIndexerAccessor ? (member.isSetter ? 3U : 2U)
            : member.isPropertyAccessor ? (member.isSetter ? 1U : 0U) : 4U;
        if (slot >= 4U || found[slot]) continue;
        auto* target = slot == 0U ? &output.propertyGetter
            : slot == 1U ? &output.propertySetter
            : slot == 2U ? &output.indexerGetter : &output.indexerSetter;
        *target = call;
        target->resultTypeKey = member.isSetter
            ? member.parameters.back().typeKey : member.declaredTypeKey;
        found[slot] = true;
        output.callerOrdinal = call.callerMethodMemberOrdinal;
        output.receiverType = call.receiverTypeKey;
        if (member.isGetter) output.valueType = member.declaredTypeKey;
    }
    return found[0] && found[1] && found[2] && found[3] &&
        !output.receiverType.empty() && !output.valueType.empty();
}

struct DeepPropertyIndexerSetterSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

nativesharp::ast::AstNode* appendSetterInteger(
    DeepPropertyIndexerSetterSyntax& fixture,
    std::uint64_t value)
{
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    fixture.nodes.back()->setIntegerValue(value);
    return fixture.nodes.back().get();
}

DeepPropertyIndexerSetterSyntax makeDeepPropertyIndexerSetterSyntax(
    std::size_t depth,
    const PropertyIndexerSetterTargets& targets)
{
    DeepPropertyIndexerSetterSyntax fixture;
    fixture.nodes.reserve(depth * 5U + 1U);
    fixture.calls.reserve(depth);
    auto* current = appendSetterInteger(fixture, 7U);
    for (std::size_t index = 0; index < depth; ++index)
    {
        const bool indexer = index % 2U != 0U;
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            indexer ? nativesharp::ast::AstNodeKind::IndexExpression
                    : nativesharp::ast::AstNodeKind::IdentifierExpression,
            nativesharp::SourceRange{}));
        auto* destination = fixture.nodes.back().get();
        if (indexer)
        {
            fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::ThisExpression,
                nativesharp::SourceRange{}));
            destination->setFirst(fixture.nodes.back().get());
            destination->addChild(appendSetterInteger(fixture, 1U));
            destination->addChild(appendSetterInteger(fixture, 2U));
        }
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::AssignmentExpression,
            nativesharp::SourceRange{}));
        auto* assignment = fixture.nodes.back().get();
        assignment->setTokenKind(nativesharp::lexer::TokenKind::Equal);
        assignment->setFirst(destination);
        assignment->setSecond(current);
        fixture.calls.push_back(indexer ? targets.indexerSetter : targets.propertySetter);
        auto& call = fixture.calls.back();
        call.syntax = assignment;
        call.receiverSyntax = nullptr;
        current = assignment;
    }
    fixture.root = current;
    return fixture;
}

bool propertyIndexerSetterChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    const PropertyIndexerSetterTargets& targets)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    for (std::size_t remaining = expectedDepth; remaining > 0U; --remaining)
    {
        const bool indexer = (remaining - 1U) % 2U != 0U;
        const auto& binding = indexer ? targets.indexerSetter : targets.propertySetter;
        const std::size_t expectedChildren = indexer ? 4U : 2U;
        if (node == nullptr || node->kind != HirKind::InstanceCall ||
            node->memberOrdinal != binding.selectedMemberOrdinal ||
            node->children.size() != expectedChildren || !node->returnsAssignedValue ||
            node->children.front()->kind != HirKind::ThisValue)
            return false;
        if (indexer && (node->children[1]->integerValue != 1U ||
                node->children[2]->integerValue != 2U))
            return false;
        node = node->children.back().get();
    }
    return node != nullptr && node->kind == HirKind::IntegerConstant &&
        node->integerValue == 7U;
}

DeepPropertyIndexerSetterSyntax makeCompoundSetterSyntax(
    const PropertyIndexerSetterTargets& targets,
    bool indexer,
    nativesharp::lexer::TokenKind operation)
{
    DeepPropertyIndexerSetterSyntax fixture;
    fixture.nodes.reserve(6U);
    fixture.calls.reserve(2U);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* receiver = fixture.nodes.back().get();
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        indexer ? nativesharp::ast::AstNodeKind::IndexExpression
                : nativesharp::ast::AstNodeKind::MemberExpression,
        nativesharp::SourceRange{}));
    auto* destination = fixture.nodes.back().get();
    destination->setFirst(receiver);
    if (indexer)
    {
        destination->addChild(appendSetterInteger(fixture, 3U));
        destination->addChild(appendSetterInteger(fixture, 4U));
    }
    auto getter = indexer ? targets.indexerGetter : targets.propertyGetter;
    getter.syntax = destination;
    getter.receiverSyntax = receiver;
    fixture.calls.push_back(std::move(getter));
    auto* right = appendSetterInteger(fixture, 5U);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::AssignmentExpression,
        nativesharp::SourceRange{}));
    auto* assignment = fixture.nodes.back().get();
    assignment->setTokenKind(operation);
    assignment->setFirst(destination);
    assignment->setSecond(right);
    auto setter = indexer ? targets.indexerSetter : targets.propertySetter;
    setter.syntax = assignment;
    setter.receiverSyntax = receiver;
    fixture.calls.push_back(std::move(setter));
    fixture.root = assignment;
    return fixture;
}

bool compoundSetterMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    const PropertyIndexerSetterTargets& targets,
    bool indexer)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    const auto& setter = indexer ? targets.indexerSetter : targets.propertySetter;
    const auto& getter = indexer ? targets.indexerGetter : targets.propertyGetter;
    const std::size_t setterChildren = indexer ? 4U : 2U;
    if (node == nullptr || node->kind != HirKind::InstanceCall ||
        node->memberOrdinal != setter.selectedMemberOrdinal ||
        node->children.size() != setterChildren || !node->returnsAssignedValue)
        return false;
    const auto* binary = node->children.back().get();
    if (binary->kind != HirKind::Binary ||
        binary->operation != nativesharp::lexer::TokenKind::Plus ||
        binary->children.size() != 2U ||
        binary->children[1]->kind != HirKind::IntegerConstant)
        return false;
    const auto* getterCall = binary->children.front().get();
    return getterCall->kind == HirKind::InstanceCall &&
        getterCall->memberOrdinal == getter.selectedMemberOrdinal &&
        getterCall->children.size() == (indexer ? 3U : 1U) &&
        getterCall->children.front()->kind == HirKind::ThisValue;
}

std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowerSetterFixture(
    ProjectCompilation& compilation,
    const PropertyIndexerSetterTargets& targets,
    DeepPropertyIndexerSetterSyntax& fixture,
    bool addressContext,
    std::string& error)
{
    nativesharp::frontend::MethodExpressionTypeBinding valueType;
    valueType.typeKey = targets.valueType;
    nativesharp::frontend::MethodExpressionTypeBinding receiverType;
    receiverType.typeKey = targets.receiverType;
    ProjectOopHirModel model;
    return nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerPropertyIndexerSetterExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(),
            targets.callerOrdinal, fixture.root, fixture.nodes, fixture.calls,
            valueType, receiverType, addressContext, nullptr, error);
}

void testDeepPropertyIndexerSetterLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768U;
    ProjectCompilation compilation;
    PropertyIndexerSetterTargets targets;
    std::string error;
    const bool prepared = preparePropertyIndexerSetterFixture(
        root, compilation, targets, error);
    tests.expect(prepared,
        "the property/indexer setter fixture exposes four real accessor bindings (" +
            error + ")");
    if (!prepared) return;
    auto deep = makeDeepPropertyIndexerSetterSyntax(nestingDepth, targets);
    auto lowered = lowerSetterFixture(compilation, targets, deep, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level property/indexer setter chain lowers without stack recursion (" +
            error + ")");
    tests.expect(propertyIndexerSetterChainMatches(lowered.get(), nestingDepth, targets),
        "iterative setter lowering preserves receiver, index, value, ordinal, and assignment result");
    destroyHirTreeIteratively(lowered);
    for (const bool indexer : {false, true})
    {
        error.clear();
        auto compound = makeCompoundSetterSyntax(
            targets, indexer, nativesharp::lexer::TokenKind::PlusEqual);
        auto compoundNode = lowerSetterFixture(
            compilation, targets, compound, false, error);
        tests.expect(compoundNode != nullptr && error.empty() &&
                compoundSetterMatches(compoundNode.get(), targets, indexer),
            (indexer ? "iterative compound indexer lowering preserves getter/index/RHS order ("
                     : "iterative compound property lowering preserves getter/RHS order (") +
                error + ")");
        destroyHirTreeIteratively(compoundNode);
    }
    error.clear();
    auto address = makeCompoundSetterSyntax(
        targets, false, nativesharp::lexer::TokenKind::PlusEqual);
    auto addressNode = lowerSetterFixture(compilation, targets, address, true, error);
    tests.expect(addressNode == nullptr && error ==
            "the result of a compound assignment is a value, not addressable storage",
        "iterative compound setter lowering preserves the addressability diagnostic exactly");
}

void testDeepConversionExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::CastExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level conversion chain lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Conversion)
    {
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative conversion lowering preserves every cast and its literal leaf");
    destroyHirTreeIteratively(lowered);

    const auto checkRuntime = [&](
        nativesharp::ast::AstNodeKind syntaxKind,
        nativesharp::frontend::MethodRuntimeTypeOperationKind operation,
        nativesharp::frontend::ProjectOopHirNodeKind expectedKind,
        std::string resultType)
    {
        std::vector<std::unique_ptr<nativesharp::ast::AstNode>> runtimeSyntax;
        runtimeSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        runtimeSyntax.back()->setIntegerValue(9U);
        auto* runtimeLiteral = runtimeSyntax.back().get();
        runtimeSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            syntaxKind,
            nativesharp::SourceRange{}));
        runtimeSyntax.back()->setFirst(runtimeLiteral);
        auto* runtimeExpression = runtimeSyntax.back().get();

        nativesharp::frontend::MethodExpressionTypeBinding runtimeType;
        runtimeType.typeKey = std::move(resultType);
        nativesharp::frontend::MethodRuntimeTypeBinding runtimeBinding;
        runtimeBinding.kind = operation;
        runtimeBinding.syntax = runtimeExpression;
        runtimeBinding.targetTypeOrdinal = 37U;
        runtimeBinding.targetIsInterface = true;
        ProjectOopHirModel runtimeModel;
        nativesharp::frontend::ProjectOopHirModelTestAccess::setRuntimeTypeBinding(
            runtimeModel, runtimeExpression, runtimeBinding);
        std::string runtimeError;
        auto runtimeLowered =
            nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
                runtimeModel,
                runtimeExpression,
                runtimeSyntax,
                runtimeType,
                runtimeError);
        return runtimeLowered != nullptr && runtimeError.empty() &&
            runtimeLowered->kind == expectedKind &&
            runtimeLowered->targetTypeOrdinal == 37U &&
            runtimeLowered->targetIsInterface &&
            runtimeLowered->children.size() == 1U &&
            runtimeLowered->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant;
    };
    tests.expect(checkRuntime(
            nativesharp::ast::AstNodeKind::TypeTestExpression,
            nativesharp::frontend::MethodRuntimeTypeOperationKind::TypeTest,
            nativesharp::frontend::ProjectOopHirNodeKind::RuntimeTypeTest,
            "builtin:bool"),
        "iterative type-test lowering preserves runtime target metadata");
    tests.expect(checkRuntime(
            nativesharp::ast::AstNodeKind::SafeCastExpression,
            nativesharp::frontend::MethodRuntimeTypeOperationKind::SafeCast,
            nativesharp::frontend::ProjectOopHirNodeKind::SafeReferenceCast,
            "type:Target"),
        "iterative safe-cast lowering preserves runtime target metadata");
    tests.expect(checkRuntime(
            nativesharp::ast::AstNodeKind::CastExpression,
            nativesharp::frontend::MethodRuntimeTypeOperationKind::CheckedCast,
            nativesharp::frontend::ProjectOopHirNodeKind::CheckedReferenceCast,
            "type:Target"),
        "iterative checked-cast lowering preserves runtime target metadata");

    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> missingBindingSyntax;
    missingBindingSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::TypeTestExpression,
        nativesharp::SourceRange{}));
    nativesharp::frontend::MethodExpressionTypeBinding missingBindingType;
    missingBindingType.typeKey = "builtin:bool";
    ProjectOopHirModel missingBindingModel;
    std::string missingBindingError;
    auto missingBinding =
        nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
            missingBindingModel,
            missingBindingSyntax.front().get(),
            missingBindingSyntax,
            missingBindingType,
            missingBindingError);
    tests.expect(missingBinding == nullptr &&
            missingBindingError ==
                "Stage 39 runtime type operation lost its semantic binding",
        "missing runtime binding fails before visiting the conversion operand");
}

void testDeepClassFieldExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    const std::string nodeType = "type:Deep::Node";
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression,
        nativesharp::SourceRange{}));
    nativesharp::ast::AstNode* current = syntax.back().get();
    std::vector<nativesharp::frontend::MethodNameBinding> names;
    names.reserve(nestingDepth);
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MemberExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(current);
        names.emplace_back();
        names.back().kind = nativesharp::frontend::MethodNameBindingKind::ExplicitObjectField;
        names.back().syntax = syntax.back().get();
        names.back().typeKey = nodeType;
        names.back().candidateMemberOrdinals.push_back(0U);
        names.back().receiverSyntax = current;
        names.back().receiverTypeKey = nodeType;
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = nodeType;
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerFieldExpression(
        model, current, syntax, names, type, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level class-field chain lowers without process-stack recursion (" + error + ")");

    std::size_t observedDepth = 0;
    bool fieldsPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::FieldValue)
    {
        fieldsPreserved = fieldsPreserved && cursor->typeKey == nodeType &&
            cursor->memberOrdinal == 0U && !cursor->volatileAccess &&
            !cursor->baseReceiver && cursor->children.size() == 1U;
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && fieldsPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::ThisValue,
        "iterative class-field lowering preserves every field and its this receiver");
    destroyHirTreeIteratively(lowered);

    ProjectOopHirModel addressModel;
    std::string addressError;
    auto address = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerFieldExpression(
        addressModel, current, syntax, names, type, true, addressError);
    tests.expect(address != nullptr && addressError.empty() &&
            address->kind == nativesharp::frontend::ProjectOopHirNodeKind::FieldAddress &&
            address->children.size() == 1U &&
            address->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::FieldValue,
        "address context changes only the outer class field and preserves value receivers");
    destroyHirTreeIteratively(address);
}

bool preparePropertyGetterFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    std::size_t& getterOrdinal,
    std::size_t& callerOrdinal,
    std::string& nodeType,
    std::string& error)
{
    const auto directory = root / "DeepPropertyGetter";
    writeProject(directory, "DeepPropertyGetter");
    writeFile(
        directory / "Main.ns",
        "namespace Deep; public class Node { "
        "public Node Next { get { return this; } } "
        "public Node Probe(Node node) { return node.Next; } }");
    if (!compilation.compileProjectOopHir(directory / "DeepPropertyGetter.nsproj", error) ||
        compilation.memberModel() == nullptr)
    {
        return false;
    }
    bool foundGetter = false;
    bool foundCaller = false;
    const auto members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const auto& member = members[ordinal];
        if (member.isPropertyAccessor && !member.isIndexerAccessor && member.isGetter &&
            member.associatedName == "Next")
        {
            getterOrdinal = ordinal;
            nodeType = member.declaredTypeKey;
            foundGetter = true;
        }
        if (!member.isPropertyAccessor && member.name == "Probe")
        {
            callerOrdinal = ordinal;
            foundCaller = true;
        }
    }
    return foundGetter && foundCaller && !nodeType.empty();
}

struct DeepPropertyGetterSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepPropertyGetterSyntax makeDeepPropertyGetterSyntax(
    std::size_t depth,
    std::size_t getterOrdinal,
    const std::string& nodeType)
{
    DeepPropertyGetterSyntax fixture;
    fixture.nodes.reserve(depth + 1U);
    fixture.calls.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MemberExpression, nativesharp::SourceRange{}));
        fixture.nodes.back()->setFirst(current);
        fixture.calls.emplace_back();
        auto& call = fixture.calls.back();
        call.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        call.syntax = fixture.nodes.back().get();
        call.receiverSyntax = current;
        call.receiverTypeKey = nodeType;
        call.selectedMemberOrdinal = getterOrdinal;
        call.resultTypeKey = nodeType;
        current = fixture.nodes.back().get();
    }
    fixture.root = current;
    return fixture;
}

bool propertyGetterChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    std::size_t getterOrdinal,
    const std::string& nodeType)
{
    std::size_t observedDepth = 0;
    while (node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::InstanceCall)
    {
        if (node->memberOrdinal != getterOrdinal || node->typeKey != nodeType ||
            node->children.size() != 1U || node->interfaceDispatch || node->nullConditionalCall)
        {
            return false;
        }
        ++observedDepth;
        node = node->children.front().get();
    }
    return observedDepth == expectedDepth && node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::ThisValue;
}

void testDeepPropertyGetterExpressionLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    std::size_t getterOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string nodeType;
    std::string error;
    const bool prepared = preparePropertyGetterFixture(
        root, compilation, getterOrdinal, callerOrdinal, nodeType, error);
    tests.expect(prepared, "the property-getter depth fixture builds semantic accessors (" +
        error + ")");
    if (!prepared) return;

    DeepPropertyGetterSyntax syntax =
        makeDeepPropertyGetterSyntax(nestingDepth, getterOrdinal, nodeType);
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = nodeType;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerPropertyGetterExpression(
            model,
            *compilation.memberModel(),
            callerOrdinal,
            syntax.root,
            syntax.nodes,
            syntax.calls,
            type,
            error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level property-getter chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(propertyGetterChainMatches(
            lowered.get(), nestingDepth, getterOrdinal, nodeType),
        "iterative property-getter lowering preserves every call and receiver in source order");
    destroyHirTreeIteratively(lowered);
}

struct ExtensionPropertyTarget final
{
    std::size_t functionOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string nodeType;
    std::string parameterType;
    std::string mangledName;
};

bool prepareExtensionPropertyFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    ExtensionPropertyTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepExtensionProperty";
    writeProject(directory, "DeepExtensionProperty");
    writeFile(
        directory / "Main.ns",
        "namespace Deep { public class Node {} extension Node { "
        "public Node Next { get { return this; } } } "
        "public class Probe { public Node Run(Node node) { return node.Next; } } }");
    if (!compilation.compileProjectOopHir(directory / "DeepExtensionProperty.nsproj", error) ||
        compilation.memberModel() == nullptr || compilation.namespaceFunctionModel() == nullptr)
    {
        return false;
    }
    bool foundFunction = false;
    const auto& functions = compilation.namespaceFunctionModel()->functions();
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const auto& function = functions[ordinal];
        if (!function.isExtension || function.name != "Next" || function.parameters.size() != 1U)
            continue;
        output.functionOrdinal = ordinal;
        output.nodeType = function.declaredTypeKey;
        output.parameterType = function.parameters.front().typeKey;
        output.mangledName = function.mangledName;
        foundFunction = true;
    }
    bool foundCaller = false;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        if (members[ordinal].name != "Run") continue;
        output.callerOrdinal = ordinal;
        foundCaller = true;
    }
    return foundFunction && foundCaller && !output.nodeType.empty() &&
        !output.parameterType.empty() && !output.mangledName.empty();
}

struct DeepExtensionPropertySyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::NamespaceFunctionCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepExtensionPropertySyntax makeDeepExtensionPropertySyntax(
    std::size_t depth,
    const ExtensionPropertyTarget& target)
{
    DeepExtensionPropertySyntax fixture;
    fixture.nodes.reserve(depth + 1U);
    fixture.calls.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MemberExpression, nativesharp::SourceRange{}));
        fixture.nodes.back()->setFirst(current);
        fixture.calls.emplace_back();
        auto& call = fixture.calls.back();
        call.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        call.syntax = fixture.nodes.back().get();
        call.writtenName = "Next";
        call.forwardedReceiver = true;
        call.receiverSyntax = current;
        call.selectedFunctionOrdinal = target.functionOrdinal;
        call.selectedArgumentParameterOrdinals.push_back(0U);
        call.resultTypeKey = target.nodeType;
        current = fixture.nodes.back().get();
    }
    fixture.root = current;
    return fixture;
}

bool extensionPropertyChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    const ExtensionPropertyTarget& target)
{
    std::size_t observedDepth = 0;
    while (node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::StaticCall)
    {
        if (node->namespaceFunctionOrdinal != target.functionOrdinal ||
            node->stringValue != target.mangledName || node->typeKey != target.nodeType ||
            node->children.size() != 1U || node->argumentCount != 0U || node->referenceResult ||
            node->argumentParameterOrdinals.size() != 1U ||
            node->argumentParameterOrdinals.front() != 0U ||
            node->argumentTypeKeys.size() != 1U ||
            node->argumentTypeKeys.front() != target.parameterType)
        {
            return false;
        }
        ++observedDepth;
        node = node->children.front().get();
    }
    return observedDepth == expectedDepth && node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::ThisValue;
}

void testDeepExtensionPropertyGetterLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    ExtensionPropertyTarget target;
    std::string error;
    const bool prepared = prepareExtensionPropertyFixture(root, compilation, target, error);
    tests.expect(prepared, "the extension-property fixture builds its real namespace getter (" +
        error + ")");
    if (!prepared) return;

    DeepExtensionPropertySyntax syntax =
        makeDeepExtensionPropertySyntax(nestingDepth, target);
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.nodeType;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerExtensionPropertyGetterExpression(
            model,
            *compilation.memberModel(),
            *compilation.namespaceFunctionModel(),
            target.callerOrdinal,
            syntax.root,
            syntax.nodes,
            syntax.calls,
            type,
            error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level extension-property chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(extensionPropertyChainMatches(lowered.get(), nestingDepth, target),
        "iterative extension-property lowering preserves every forwarded receiver contract");
    destroyHirTreeIteratively(lowered);
}

struct ExtensionPropertySetterTarget final
{
    nativesharp::frontend::NamespaceFunctionCallBinding binding;
    std::size_t callerOrdinal = 0;
    std::string receiverType;
    std::string valueType;
    std::string mangledName;
};

bool prepareExtensionPropertySetterFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    ExtensionPropertySetterTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepExtensionPropertySetter";
    writeProject(directory, "DeepExtensionPropertySetter");
    writeFile(directory / "Main.ns",
        "namespace Deep { extension int { public int Doubled { "
        "get { return this * 2; } set { int observed = value; } } } "
        "public class Probe { public int Run(int value) { value.Doubled = value; return value; } } }");
    if (!compilation.compileProjectOopHir(directory / "DeepExtensionPropertySetter.nsproj", error) ||
        compilation.memberModel() == nullptr || compilation.namespaceFunctionModel() == nullptr)
        return false;
    const auto& functions = compilation.namespaceFunctionModel()->functions();
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const auto& function = functions[ordinal];
        if (!function.isExtension || function.parameters.size() != 2U) continue;
        output.binding.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        output.binding.forwardedReceiver = true;
        output.binding.selectedFunctionOrdinal = ordinal;
        output.receiverType = function.parameters[0].typeKey;
        output.valueType = function.parameters[1].typeKey;
        output.binding.resultTypeKey = output.valueType;
        output.mangledName = function.mangledName;
    }
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
        if (members[ordinal].name == "Run") output.callerOrdinal = ordinal;
    return output.binding.selectedFunctionOrdinal.has_value() &&
        !output.receiverType.empty() && !output.valueType.empty() && !output.mangledName.empty();
}

struct DeepExtensionPropertySetterSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::NamespaceFunctionCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepExtensionPropertySetterSyntax makeDeepExtensionPropertySetterSyntax(
    std::size_t depth,
    const ExtensionPropertySetterTarget& target)
{
    using AstKind = nativesharp::ast::AstNodeKind;
    DeepExtensionPropertySetterSyntax fixture;
    fixture.nodes.reserve(depth * 3U + 1U);
    fixture.calls.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(AstKind::IntegerLiteralExpression, nativesharp::SourceRange{}));
    fixture.nodes.back()->setIntegerValue(7U);
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(AstKind::IntegerLiteralExpression, nativesharp::SourceRange{}));
        fixture.nodes.back()->setIntegerValue(index + 11U);
        auto* receiver = fixture.nodes.back().get();
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(AstKind::MemberExpression, nativesharp::SourceRange{}));
        fixture.nodes.back()->setFirst(receiver);
        auto* destination = fixture.nodes.back().get();
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(AstKind::AssignmentExpression, nativesharp::SourceRange{}));
        auto* assignment = fixture.nodes.back().get();
        assignment->setTokenKind(nativesharp::lexer::TokenKind::Equal);
        assignment->setFirst(destination);
        assignment->setSecond(current);
        fixture.calls.push_back(target.binding);
        fixture.calls.back().syntax = assignment;
        fixture.calls.back().receiverSyntax = receiver;
        current = assignment;
    }
    fixture.root = current;
    return fixture;
}

bool extensionPropertySetterChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t depth,
    const ExtensionPropertySetterTarget& target)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    std::size_t observed = 0;
    while (node != nullptr && node->kind == HirKind::StaticCall)
    {
        if (node->namespaceFunctionOrdinal != *target.binding.selectedFunctionOrdinal ||
            node->stringValue != target.mangledName || node->typeKey != target.valueType ||
            !node->returnsAssignedValue || node->children.size() != 2U ||
            node->children[0]->kind != HirKind::IntegerConstant ||
            node->argumentParameterOrdinals != std::vector<std::size_t>{0U, 1U} ||
            node->argumentTypeKeys != std::vector<std::string>{target.receiverType, target.valueType})
            return false;
        ++observed;
        node = node->children[1].get();
    }
    return observed == depth && node != nullptr && node->kind == HirKind::IntegerConstant;
}

std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowerExtensionSetterFixture(
    ProjectCompilation& compilation,
    const ExtensionPropertySetterTarget& target,
    DeepExtensionPropertySetterSyntax& syntax,
    bool addressContext,
    std::string& error)
{
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.valueType;
    ProjectOopHirModel model;
    return nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerExtensionPropertySetterExpression(
            model, *compilation.memberModel(), *compilation.namespaceFunctionModel(),
            target.callerOrdinal, syntax.root, syntax.nodes, syntax.calls, type,
            addressContext, error);
}

void testExtensionPropertySetterDiagnostics(
    TestContext& tests,
    ProjectCompilation& compilation,
    const ExtensionPropertySetterTarget& target)
{
    auto syntax = makeDeepExtensionPropertySetterSyntax(1U, target);
    std::string error;
    auto lowered = lowerExtensionSetterFixture(compilation, target, syntax, true, error);
    tests.expect(lowered == nullptr && error ==
        "the result of an extension-property assignment is a value, not addressable storage",
        "extension-property setter preserves its address-context diagnostic");
    syntax.calls.front().receiverSyntax = nullptr;
    error.clear();
    lowered = lowerExtensionSetterFixture(compilation, target, syntax, false, error);
    tests.expect(lowered == nullptr && error == "extension setter lost its synthetic receiver contract",
        "extension-property setter preserves its missing receiver diagnostic");
    auto& function = const_cast<nativesharp::frontend::NamespaceFunctionSymbol&>(
        compilation.namespaceFunctionModel()->functions()[*target.binding.selectedFunctionOrdinal]);
    auto parameters = function.parameters;
    function.parameters.resize(1U);
    syntax.calls.front().receiverSyntax = syntax.nodes[1].get();
    error.clear();
    lowered = lowerExtensionSetterFixture(compilation, target, syntax, false, error);
    tests.expect(lowered == nullptr && error == "extension setter has no value parameter",
        "extension-property setter validates its value parameter after lowering operands");
    function.parameters = std::move(parameters);
}

void testExtensionPropertySetterShallowContract(
    TestContext& tests,
    ProjectCompilation& compilation,
    const ExtensionPropertySetterTarget& target)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    auto syntax = makeDeepExtensionPropertySetterSyntax(1U, target);
    nativesharp::frontend::ProjectImplicitConversion conversion;
    conversion.kind = nativesharp::frontend::ProjectConversionKind::Numeric;
    conversion.sourceTypeKey = target.valueType;
    conversion.targetTypeKey = target.valueType;
    syntax.calls.front().selectedConversions = {conversion, conversion};
    std::string error;
    auto lowered = lowerExtensionSetterFixture(compilation, target, syntax, false, error);
    tests.expect(lowered != nullptr && error.empty() && lowered->children.size() == 2U &&
        lowered->children[0]->kind == HirKind::Conversion &&
        lowered->children[1]->kind == HirKind::Conversion && lowered->returnsAssignedValue,
        "extension-property setter preserves receiver/value conversion order and assigned value");
    destroyHirTreeIteratively(lowered);
    syntax = makeDeepExtensionPropertySetterSyntax(1U, target);
    syntax.calls.front().forwardedReceiver = false;
    syntax.calls.front().receiverSyntax = nullptr;
    error.clear();
    lowered = lowerExtensionSetterFixture(compilation, target, syntax, false, error);
    tests.expect(lowered != nullptr && error.empty() && lowered->children.size() == 1U &&
        lowered->argumentParameterOrdinals == std::vector<std::size_t>{0U},
        "extension-property setter without receiver preserves value parameter ordinal zero");
    destroyHirTreeIteratively(lowered);
}

void testDeepExtensionPropertySetterLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    ExtensionPropertySetterTarget target;
    std::string error;
    const bool prepared = prepareExtensionPropertySetterFixture(root, compilation, target, error);
    tests.expect(prepared, "the extension-property setter fixture builds its namespace target (" + error + ")");
    if (!prepared) return;
    auto syntax = makeDeepExtensionPropertySetterSyntax(nestingDepth, target);
    auto lowered = lowerExtensionSetterFixture(compilation, target, syntax, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level extension-property setter chain lowers without process-stack recursion (" + error + ")");
    tests.expect(extensionPropertySetterChainMatches(lowered.get(), nestingDepth, target),
        "iterative extension-property setters preserve receiver-before-value shape and metadata");
    destroyHirTreeIteratively(lowered);
    testExtensionPropertySetterShallowContract(tests, compilation, target);
    testExtensionPropertySetterDiagnostics(tests, compilation, target);
}

struct NullConditionalAssignmentTarget final
{
    std::size_t fieldOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string typeKey;
};

bool prepareNullConditionalAssignmentFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    NullConditionalAssignmentTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepNullConditionalAssignment";
    writeProject(directory, "DeepNullConditionalAssignment");
    writeFile(directory / "Main.ns",
        "namespace Deep { public class Box { public volatile int Value; } "
        "public class Probe { public int Run() { return 0; } } }");
    if (!compilation.compileProjectOopHir(directory / "DeepNullConditionalAssignment.nsproj", error) ||
        compilation.memberModel() == nullptr)
        return false;
    bool foundField = false;
    bool foundCaller = false;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        if (members[ordinal].name == "Value" && members[ordinal].isVolatile)
        {
            output.fieldOrdinal = ordinal;
            output.typeKey = members[ordinal].declaredTypeKey;
            foundField = true;
        }
        if (members[ordinal].name == "Run")
        {
            output.callerOrdinal = ordinal;
            foundCaller = true;
        }
    }
    return foundField && foundCaller && !output.typeKey.empty();
}

struct DeepNullConditionalAssignmentSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodNameBinding> names;
    std::vector<nativesharp::frontend::MethodCoreBinding> cores;
    std::vector<std::size_t> deepChildOrdinals;
    const nativesharp::ast::AstNode* root = nullptr;
};

nativesharp::ast::AstNode* addNullConditionalInteger(
    DeepNullConditionalAssignmentSyntax& fixture,
    std::uint64_t value)
{
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    fixture.nodes.back()->setIntegerValue(value);
    return fixture.nodes.back().get();
}

nativesharp::ast::AstNode* addNullConditionalFieldLayer(
    DeepNullConditionalAssignmentSyntax& fixture,
    nativesharp::ast::AstNode* current,
    std::size_t index,
    const NullConditionalAssignmentTarget& target)
{
    using AstKind = nativesharp::ast::AstNodeKind;
    const bool deepReceiver = index % 5U == 1U;
    auto* receiver = deepReceiver ? current : addNullConditionalInteger(fixture, index + 11U);
    auto* value = deepReceiver ? addNullConditionalInteger(fixture, index + 21U) : current;
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        AstKind::NullConditionalMemberExpression, nativesharp::SourceRange{}));
    auto* destination = fixture.nodes.back().get();
    destination->setFirst(receiver);
    fixture.names.emplace_back();
    fixture.names.back().kind = nativesharp::frontend::MethodNameBindingKind::ExplicitObjectField;
    fixture.names.back().syntax = destination;
    fixture.names.back().candidateMemberOrdinals.push_back(target.fieldOrdinal);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        AstKind::AssignmentExpression, nativesharp::SourceRange{}));
    auto* assignment = fixture.nodes.back().get();
    assignment->setTokenKind(index % 10U == 0U
        ? nativesharp::lexer::TokenKind::PlusEqual : nativesharp::lexer::TokenKind::Equal);
    assignment->setFirst(destination);
    assignment->setSecond(value);
    fixture.deepChildOrdinals.push_back(deepReceiver ? 0U : 1U);
    return assignment;
}

nativesharp::ast::AstNode* addNullConditionalArrayLayer(
    DeepNullConditionalAssignmentSyntax& fixture,
    nativesharp::ast::AstNode* current,
    std::size_t index,
    const NullConditionalAssignmentTarget& target)
{
    using AstKind = nativesharp::ast::AstNodeKind;
    const std::size_t deepChild = index % 5U == 3U ? 0U : index % 5U == 4U ? 1U : 2U;
    auto* receiver = deepChild == 0U ? current : addNullConditionalInteger(fixture, index + 31U);
    auto* elementIndex = deepChild == 1U ? current : addNullConditionalInteger(fixture, index + 41U);
    auto* value = deepChild == 2U ? current : addNullConditionalInteger(fixture, index + 51U);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        AstKind::NullConditionalIndexExpression, nativesharp::SourceRange{}));
    auto* destination = fixture.nodes.back().get();
    destination->setFirst(receiver);
    if (index % 2U == 0U) destination->addChild(elementIndex);
    else destination->setSecond(elementIndex);
    fixture.cores.emplace_back();
    fixture.cores.back().kind = nativesharp::frontend::MethodCoreOperationKind::NullConditionalArrayElement;
    fixture.cores.back().syntax = destination;
    fixture.cores.back().elementTypeKey = target.typeKey;
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        AstKind::AssignmentExpression, nativesharp::SourceRange{}));
    auto* assignment = fixture.nodes.back().get();
    assignment->setTokenKind(nativesharp::lexer::TokenKind::Equal);
    assignment->setFirst(destination);
    assignment->setSecond(value);
    fixture.deepChildOrdinals.push_back(deepChild);
    return assignment;
}

DeepNullConditionalAssignmentSyntax makeDeepNullConditionalAssignmentSyntax(
    std::size_t depth,
    const NullConditionalAssignmentTarget& target)
{
    DeepNullConditionalAssignmentSyntax fixture;
    fixture.nodes.reserve(depth * 5U + 1U);
    fixture.names.reserve(depth);
    fixture.cores.reserve(depth);
    fixture.deepChildOrdinals.reserve(depth);
    auto* current = addNullConditionalInteger(fixture, 7U);
    for (std::size_t index = 0; index < depth; ++index)
    {
        current = index % 5U < 2U
            ? addNullConditionalFieldLayer(fixture, current, index, target)
            : addNullConditionalArrayLayer(fixture, current, index, target);
    }
    fixture.root = current;
    return fixture;
}

std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowerNullConditionalFixture(
    ProjectCompilation& compilation,
    const NullConditionalAssignmentTarget& target,
    DeepNullConditionalAssignmentSyntax& syntax,
    bool addressContext,
    std::string& error)
{
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.typeKey;
    type.ownsStorageDestination = true;
    ProjectOopHirModel model;
    return nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerNullConditionalAssignmentExpression(
            model, *compilation.memberModel(), target.callerOrdinal, syntax.root,
            syntax.nodes, syntax.names, syntax.cores, type, addressContext, error);
}

bool nullConditionalAssignmentChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    const DeepNullConditionalAssignmentSyntax& syntax,
    const NullConditionalAssignmentTarget& target)
{
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    for (std::size_t remaining = syntax.deepChildOrdinals.size(); remaining > 0U; --remaining)
    {
        const std::size_t sourceIndex = remaining - 1U;
        const bool field = sourceIndex % 5U < 2U;
        const std::size_t childCount = field ? 2U : 3U;
        if (node == nullptr || node->kind != (field
                ? HirKind::NullConditionalFieldStore : HirKind::NullConditionalArrayStore) ||
            node->typeKey != target.typeKey || !node->booleanValue ||
            node->children.size() != childCount)
            return false;
        if (field && (node->memberOrdinal != target.fieldOrdinal || !node->volatileAccess ||
            node->operation != (sourceIndex % 10U == 0U
                ? nativesharp::lexer::TokenKind::PlusEqual : nativesharp::lexer::TokenKind::Equal)))
            return false;
        if (!field && node->elementTypeKey != target.typeKey) return false;
        node = node->children[syntax.deepChildOrdinals[sourceIndex]].get();
    }
    return node != nullptr && node->kind == HirKind::IntegerConstant;
}

void testNullConditionalAssignmentDiagnostics(
    TestContext& tests,
    ProjectCompilation& compilation,
    const NullConditionalAssignmentTarget& target)
{
    auto fieldSyntax = makeDeepNullConditionalAssignmentSyntax(1U, target);
    fieldSyntax.names.front().candidateMemberOrdinals.clear();
    std::string error;
    auto lowered = lowerNullConditionalFixture(compilation, target, fieldSyntax, false, error);
    tests.expect(lowered == nullptr && error ==
        "T10-F05 null-conditional assignment has no unique instance-field binding",
        "null-conditional field assignment preserves its invalid binding diagnostic");
    auto arraySyntax = makeDeepNullConditionalAssignmentSyntax(3U, target);
    arraySyntax.cores.back().kind = nativesharp::frontend::MethodCoreOperationKind::ArrayAllocation;
    error.clear();
    lowered = lowerNullConditionalFixture(compilation, target, arraySyntax, false, error);
    tests.expect(lowered == nullptr && error ==
        "T10-F05 null-conditional index store has no native-array element binding",
        "null-conditional array assignment preserves its invalid core diagnostic");
}

void testDeepNullConditionalAssignmentLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    NullConditionalAssignmentTarget target;
    std::string error;
    const bool prepared = prepareNullConditionalAssignmentFixture(root, compilation, target, error);
    tests.expect(prepared, "the null-conditional assignment fixture builds its volatile field (" + error + ")");
    if (!prepared) return;
    auto syntax = makeDeepNullConditionalAssignmentSyntax(nestingDepth, target);
    auto lowered = lowerNullConditionalFixture(compilation, target, syntax, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level mixed null-conditional store chain lowers without process-stack recursion (" + error + ")");
    tests.expect(nullConditionalAssignmentChainMatches(lowered.get(), syntax, target),
        "iterative null-conditional stores preserve field/array shape and every operand edge");
    destroyHirTreeIteratively(lowered);
    auto addressSyntax = makeDeepNullConditionalAssignmentSyntax(1U, target);
    error.clear();
    lowered = lowerNullConditionalFixture(compilation, target, addressSyntax, true, error);
    tests.expect(lowered != nullptr && error.empty(),
        "null-conditional assignment preserves historical address-context acceptance");
    destroyHirTreeIteratively(lowered);
    testNullConditionalAssignmentDiagnostics(tests, compilation, target);
}

struct UserIndexerTarget final
{
    std::size_t getterOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string nodeType;
    std::string indexType;
};

bool prepareUserIndexerFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    UserIndexerTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepUserIndexer";
    writeProject(directory, "DeepUserIndexer");
    writeFile(
        directory / "Main.ns",
        "namespace Deep { public class Node { "
        "public Node this[int row, int column] { get { return this; } } } "
        "public class Probe { public Node Run(Node node) { return node[1, 2]; } } }");
    if (!compilation.compileProjectOopHir(directory / "DeepUserIndexer.nsproj", error) ||
        compilation.memberModel() == nullptr)
    {
        return false;
    }
    bool foundGetter = false;
    bool foundCaller = false;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const auto& member = members[ordinal];
        if (member.isIndexerAccessor && member.isGetter && member.parameters.size() == 2U)
        {
            output.getterOrdinal = ordinal;
            output.nodeType = member.declaredTypeKey;
            output.indexType = member.parameters.front().typeKey;
            foundGetter = true;
        }
        if (!member.isPropertyAccessor && !member.isIndexerAccessor && member.name == "Run")
        {
            output.callerOrdinal = ordinal;
            foundCaller = true;
        }
    }
    return foundGetter && foundCaller && !output.nodeType.empty() &&
        output.indexType == "builtin:int";
}

struct DeepUserIndexerSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCallBinding> calls;
    std::vector<nativesharp::frontend::MethodCoreBinding> cores;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepUserIndexerSyntax makeDeepUserIndexerSyntax(
    std::size_t depth,
    const UserIndexerTarget& target)
{
    DeepUserIndexerSyntax fixture;
    fixture.nodes.reserve(depth * 3U + 1U);
    fixture.calls.reserve(depth);
    fixture.cores.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        fixture.nodes.back()->setIntegerValue(1U);
        auto* row = fixture.nodes.back().get();
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        fixture.nodes.back()->setIntegerValue(2U);
        auto* column = fixture.nodes.back().get();
        const bool nullConditional = index % 2U != 0U;
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nullConditional
                ? nativesharp::ast::AstNodeKind::NullConditionalIndexExpression
                : nativesharp::ast::AstNodeKind::IndexExpression,
            nativesharp::SourceRange{}));
        auto* indexer = fixture.nodes.back().get();
        indexer->setFirst(current);
        indexer->addChild(row);
        indexer->addChild(column);
        fixture.calls.emplace_back();
        auto& call = fixture.calls.back();
        call.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        call.syntax = indexer;
        call.receiverSyntax = current;
        call.receiverTypeKey = target.nodeType;
        call.selectedMemberOrdinal = target.getterOrdinal;
        call.resultTypeKey = target.nodeType;
        if (nullConditional)
        {
            fixture.cores.emplace_back();
            auto& core = fixture.cores.back();
            core.kind = nativesharp::frontend::MethodCoreOperationKind::NullConditionalUserIndex;
            core.syntax = indexer;
            core.resultTypeKey = target.nodeType;
        }
        current = indexer;
    }
    fixture.root = current;
    return fixture;
}

bool userIndexerChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    const UserIndexerTarget& target)
{
    std::size_t observedDepth = 0;
    while (node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::InstanceCall)
    {
        const bool expectedNullConditional =
            (expectedDepth - observedDepth - 1U) % 2U != 0U;
        if (node->memberOrdinal != target.getterOrdinal || node->typeKey != target.nodeType ||
            node->children.size() != 3U || node->interfaceDispatch || node->baseReceiver ||
            node->nullConditionalCall != expectedNullConditional || node->referenceResult ||
            node->argumentCount != 0U)
        {
            return false;
        }
        const auto& row = node->children[1];
        const auto& column = node->children[2];
        if (row->kind != nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant ||
            column->kind != nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant ||
            row->typeKey != target.indexType || column->typeKey != target.indexType ||
            row->integerValue != 1U || column->integerValue != 2U)
        {
            return false;
        }
        ++observedDepth;
        node = node->children.front().get();
    }
    return observedDepth == expectedDepth && node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::ThisValue;
}

void testDeepUserIndexerGetterLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    UserIndexerTarget target;
    std::string error;
    const bool prepared = prepareUserIndexerFixture(root, compilation, target, error);
    tests.expect(prepared, "the user-indexer fixture builds its real two-argument getter (" +
        error + ")");
    if (!prepared) return;

    DeepUserIndexerSyntax syntax = makeDeepUserIndexerSyntax(nestingDepth, target);
    nativesharp::frontend::MethodExpressionTypeBinding nodeType;
    nodeType.typeKey = target.nodeType;
    nativesharp::frontend::MethodExpressionTypeBinding indexType;
    indexType.typeKey = target.indexType;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerUserIndexerGetterExpression(
            model,
            *compilation.memberModel(),
            target.callerOrdinal,
            syntax.root,
            syntax.nodes,
            syntax.calls,
            syntax.cores,
            nodeType,
            indexType,
            error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level user-indexer chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(userIndexerChainMatches(lowered.get(), nestingDepth, target),
        "iterative user-indexer lowering preserves receiver/index order and null-call metadata");
    destroyHirTreeIteratively(lowered);
}

struct InterpolationHandlerTarget final
{
    std::size_t appendLiteralOrdinal = 0;
    std::size_t appendFormattedOrdinal = 0;
    std::size_t finishOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string typeKey;
};

bool prepareInterpolationHandlerFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    InterpolationHandlerTarget& output,
    std::string& error)
{
    const auto directory = root / "DeepInterpolationHandler";
    writeProject(directory, "DeepInterpolationHandler");
    writeFile(directory / "Main.ns", R"ns(
struct Builder {
    public void AppendLiteral(string value) { }
    public void AppendFormatted(int value) { }
    public void AppendFormatted(Builder value) { }
    public Builder Finish() { return this; }
    public Builder Wrap() { Builder result = $"pre{2:X4}post"; return result; }
}
)ns");
    if (!compilation.compileProjectOopHir(directory / "DeepInterpolationHandler.nsproj", error) ||
        compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr)
    {
        return false;
    }
    const auto types = compilation.nameLookup()->namedTypes();
    const auto type = std::find_if(types.begin(), types.end(), [](const auto& candidate)
    {
        return candidate.identity.name() == "Builder";
    });
    if (type == types.end()) return false;
    const std::size_t typeOrdinal = static_cast<std::size_t>(type - types.begin());
    output.typeKey = "type:" + type->identity.key();
    bool literal = false;
    bool formatted = false;
    bool finish = false;
    bool caller = false;
    const auto& members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const auto& member = members[ordinal];
        if (member.declaringTypeOrdinal != typeOrdinal) continue;
        if (member.name == "AppendLiteral" && member.parameters.size() == 1U &&
            member.parameters.front().typeKey == "builtin:string")
        {
            output.appendLiteralOrdinal = ordinal;
            literal = true;
        }
        if (member.name == "AppendFormatted" && member.parameters.size() == 1U &&
            member.parameters.front().typeKey == output.typeKey)
        {
            output.appendFormattedOrdinal = ordinal;
            formatted = true;
        }
        if (member.name == "Finish" && member.parameters.empty())
        {
            output.finishOrdinal = ordinal;
            finish = true;
        }
        if (member.name == "Wrap" && member.parameters.empty())
        {
            output.callerOrdinal = ordinal;
            caller = true;
        }
    }
    return literal && formatted && finish && caller;
}

struct DeepInterpolationHandlerSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCoreBinding> cores;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepInterpolationHandlerSyntax makeDeepInterpolationHandlerSyntax(
    std::size_t depth,
    const InterpolationHandlerTarget& target)
{
    DeepInterpolationHandlerSyntax fixture;
    fixture.nodes.reserve(depth * 2U + 1U);
    fixture.cores.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
        auto* sideHole = fixture.nodes.back().get();
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::InterpolatedStringExpression,
            nativesharp::SourceRange{}));
        auto* interpolation = fixture.nodes.back().get();
        interpolation->setText("pre{value:X4}mid{side}post");
        interpolation->addChild(current);
        interpolation->addChild(sideHole);
        fixture.cores.emplace_back();
        auto& core = fixture.cores.back();
        core.kind = nativesharp::frontend::MethodCoreOperationKind::InterpolationHandler;
        core.syntax = interpolation;
        core.resultTypeKey = target.typeKey;
        core.builderMemberOrdinals = {
            target.appendLiteralOrdinal, target.appendFormattedOrdinal, target.finishOrdinal};
        current = interpolation;
    }
    fixture.root = current;
    return fixture;
}

const nativesharp::frontend::ProjectOopHirNode* interpolationArgument(
    const nativesharp::frontend::ProjectOopHirNode* statement,
    std::size_t memberOrdinal,
    std::size_t localOrdinal,
    std::string_view handlerType)
{
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    if (statement == nullptr || statement->kind != Kind::ExpressionStatement ||
        statement->children.size() != 1U) return nullptr;
    const auto* call = statement->children.front().get();
    if (call->kind != Kind::InstanceCall || call->typeKey != "builtin:void" ||
        call->memberOrdinal != memberOrdinal || call->children.size() != 2U) return nullptr;
    const auto* receiver = call->children.front().get();
    if (receiver->kind != Kind::LocalAddress || receiver->localOrdinal != localOrdinal ||
        receiver->typeKey != std::string(handlerType) + "*") return nullptr;
    return call->children[1].get();
}

bool loadedBuilderThis(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::string_view typeKey)
{
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    return node != nullptr && node->kind == Kind::PointerLoad && node->typeKey == typeKey &&
        node->children.size() == 1U && node->children.front()->kind == Kind::ThisValue &&
        node->children.front()->typeKey == typeKey;
}

bool interpolationHandlerChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    const InterpolationHandlerTarget& target)
{
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    for (std::size_t depth = 0; depth < expectedDepth; ++depth)
    {
        if (node == nullptr || node->kind != Kind::BlockExpression ||
            node->typeKey != target.typeKey || node->children.size() != 7U) return false;
        const auto* declaration = node->children[0].get();
        if (declaration->kind != Kind::LocalDeclaration || declaration->localOrdinal != depth ||
            declaration->stringValue != "handler" || declaration->children.size() != 1U ||
            declaration->children.front()->kind != Kind::DefaultValue) return false;
        const auto* prefix = interpolationArgument(
            node->children[1].get(), target.appendLiteralOrdinal, depth, target.typeKey);
        const auto* nested = interpolationArgument(
            node->children[2].get(), target.appendFormattedOrdinal, depth, target.typeKey);
        const auto* middle = interpolationArgument(
            node->children[3].get(), target.appendLiteralOrdinal, depth, target.typeKey);
        const auto* side = interpolationArgument(
            node->children[4].get(), target.appendFormattedOrdinal, depth, target.typeKey);
        const auto* suffix = interpolationArgument(
            node->children[5].get(), target.appendLiteralOrdinal, depth, target.typeKey);
        if (prefix == nullptr || prefix->kind != Kind::StringLiteral || prefix->stringValue != "pre" ||
            middle == nullptr || middle->kind != Kind::StringLiteral || middle->stringValue != "mid" ||
            suffix == nullptr || suffix->kind != Kind::StringLiteral || suffix->stringValue != "post" ||
            !loadedBuilderThis(side, target.typeKey)) return false;
        const auto* finish = node->children[6].get();
        if (finish->kind != Kind::InstanceCall || finish->memberOrdinal != target.finishOrdinal ||
            finish->typeKey != target.typeKey || finish->children.size() != 1U ||
            finish->children.front()->kind != Kind::LocalAddress ||
            finish->children.front()->localOrdinal != depth) return false;
        node = nested;
    }
    return loadedBuilderThis(node, target.typeKey);
}

void testInterpolationHandlerDiagnostics(
    TestContext& tests,
    const ProjectCompilation& compilation,
    const InterpolationHandlerTarget& target)
{
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.typeKey;
    DeepInterpolationHandlerSyntax syntax = makeDeepInterpolationHandlerSyntax(1U, target);
    syntax.nodes.back()->setText("pre{one}mid{two}post{missing}");
    std::string error;
    ProjectOopHirModel missingModel;
    auto missing = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerInterpolationHandlerExpression(
            missingModel, *compilation.nameLookup(), *compilation.memberModel(),
            target.callerOrdinal, syntax.root, syntax.nodes, syntax.cores, type, false, error);
    tests.expect(missing == nullptr &&
            error == "interpolation handler hole has no expression",
        "iterative interpolation preserves the exact missing-hole diagnostic");

    error.clear();
    ProjectOopHirModel addressModel;
    auto address = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerInterpolationHandlerExpression(
            addressModel, *compilation.nameLookup(), *compilation.memberModel(),
            target.callerOrdinal, syntax.root, syntax.nodes, syntax.cores, type, true, error);
    tests.expect(address == nullptr &&
            error == "an interpolation handler result is a value, not addressable storage",
        "iterative interpolation preserves the exact address-context diagnostic");

    syntax.nodes.back()->setText("pre{unterminated");
    error.clear();
    ProjectOopHirModel parseModel;
    auto malformed = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerInterpolationHandlerExpression(
            parseModel, *compilation.nameLookup(), *compilation.memberModel(),
            target.callerOrdinal, syntax.root, syntax.nodes, syntax.cores, type, false, error);
    tests.expect(malformed == nullptr && error == "unterminated interpolation hole",
        "iterative interpolation preserves the interpolation-parser diagnostic");
}

void testDeepInterpolationHandlerLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    InterpolationHandlerTarget target;
    std::string error;
    const bool prepared = prepareInterpolationHandlerFixture(root, compilation, target, error);
    tests.expect(prepared, "the interpolation fixture builds real builder members (" + error + ")");
    if (!prepared) return;

    DeepInterpolationHandlerSyntax syntax =
        makeDeepInterpolationHandlerSyntax(nestingDepth, target);
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = target.typeKey;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerInterpolationHandlerExpression(
            model, *compilation.nameLookup(), *compilation.memberModel(), target.callerOrdinal,
            syntax.root, syntax.nodes, syntax.cores, type, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level interpolation-hole chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(interpolationHandlerChainMatches(lowered.get(), nestingDepth, target),
        "iterative interpolation preserves literals, hole order, format-neutral builder calls, "
        "synthetic ordinals, and Finish");
    destroyHirTreeIteratively(lowered);
    testInterpolationHandlerDiagnostics(tests, compilation, target);
}

void testDeepOverflowPolicyExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::OverflowPolicyExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setAuxiliaryText(index % 2U == 0U ? "checked" : "unchecked");
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level overflow-policy expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    bool policiesPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::OverflowPolicy)
    {
        const std::size_t sourceIndex = nestingDepth - observedDepth - 1U;
        policiesPreserved = policiesPreserved && cursor->stringValue ==
            (sourceIndex % 2U == 0U ? "checked" : "unchecked");
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && policiesPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative overflow-policy lowering preserves every policy and its literal leaf");
    destroyHirTreeIteratively(lowered);
}

void testDeepBuiltinBinaryExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 2U + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setIntegerValue(index + 2U);
        nativesharp::ast::AstNode* right = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BinaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Plus);
        syntax.back()->setFirst(current);
        syntax.back()->setSecond(right);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level builtin binary expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    bool rightOperandsPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Binary)
    {
        rightOperandsPreserved = rightOperandsPreserved && cursor->children.size() == 2U &&
            cursor->children[1]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant &&
            cursor->operation == nativesharp::lexer::TokenKind::Plus;
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && rightOperandsPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative builtin binary lowering preserves postorder, both operands, and every operator");
    destroyHirTreeIteratively(lowered);
}

void testDeepConditionalExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 3U + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(0U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
        nativesharp::ast::AstNode* condition = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setIntegerValue(index + 1U);
        nativesharp::ast::AstNode* whenTrue = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ConditionalExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(condition);
        syntax.back()->setSecond(whenTrue);
        syntax.back()->setThird(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level conditional expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    bool branchesPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Conditional)
    {
        branchesPreserved = branchesPreserved && cursor->children.size() == 3U &&
            cursor->children[0]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::BooleanConstant &&
            cursor->children[1]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant;
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children[2].get();
    }
    tests.expect(observedDepth == nestingDepth && branchesPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative conditional lowering preserves condition, both branches, and common type");
    destroyHirTreeIteratively(lowered);
}

using MatchTypedSyntax = std::pair<const nativesharp::ast::AstNode*,
    const nativesharp::frontend::MethodExpressionTypeBinding*>;

nativesharp::ast::AstNode* appendMatchBooleanSyntax(
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>>& syntax,
    std::vector<MatchTypedSyntax>& typedSyntax,
    const nativesharp::frontend::MethodExpressionTypeBinding& booleanType)
{
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
    typedSyntax.emplace_back(syntax.back().get(), &booleanType);
    return syntax.back().get();
}

nativesharp::ast::AstNode* buildDeepMatchSyntax(
    std::size_t nestingDepth,
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>>& syntax,
    std::vector<const nativesharp::ast::AstNode*>& matches,
    std::vector<MatchTypedSyntax>& typedSyntax,
    std::vector<nativesharp::frontend::MethodMatchExpressionBinding>& bindings,
    const nativesharp::frontend::MethodExpressionTypeBinding& booleanType)
{
    auto* current = appendMatchBooleanSyntax(syntax, typedSyntax, booleanType);
    for (std::size_t depth = 0; depth < nestingDepth; ++depth)
    {
        std::array<nativesharp::ast::AstNode*, 4> edges{};
        const std::size_t recursiveEdge = depth % edges.size();
        for (std::size_t edge = 0; edge < edges.size(); ++edge)
        {
            edges[edge] = edge == recursiveEdge ? current
                : appendMatchBooleanSyntax(syntax, typedSyntax, booleanType);
        }
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MatchArmExpression, nativesharp::SourceRange{}));
        auto* arm = syntax.back().get();
        arm->setFirst(edges[1]);
        arm->setSecond(edges[2]);
        arm->setThird(edges[3]);
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MatchExpression, nativesharp::SourceRange{}));
        current = syntax.back().get();
        current->setFirst(edges[0]);
        current->addChild(arm);
        typedSyntax.emplace_back(current, &booleanType);
        matches.push_back(current);
        nativesharp::frontend::MethodMatchExpressionBinding binding;
        binding.syntax = current;
        binding.governingTypeKey = "builtin:bool";
        binding.resultTypeKey = "builtin:bool";
        binding.armConversions.push_back(nativesharp::frontend::ProjectImplicitConversion{
            nativesharp::frontend::ProjectConversionKind::Identity,
            0,
            "builtin:bool",
            "builtin:bool",
            std::nullopt});
        bindings.push_back(std::move(binding));
    }
    return current;
}

std::size_t observeDeepMatchDepth(
    nativesharp::frontend::ProjectOopHirNode* cursor,
    std::size_t nestingDepth)
{
    std::size_t observedDepth = 0;
    for (std::size_t remaining = nestingDepth; remaining > 0U && cursor != nullptr; --remaining)
    {
        const std::size_t recursiveEdge = (remaining - 1U) % 4U;
        const bool valid = cursor->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::MatchExpression &&
            cursor->children.size() == 2U && cursor->children[1]->children.size() == 3U;
        if (!valid) break;
        auto* arm = cursor->children[1].get();
        cursor = recursiveEdge == 0U ? cursor->children[0].get()
            : arm->children[recursiveEdge - 1U].get();
        ++observedDepth;
    }
    return cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::BooleanConstant
        ? observedDepth : 0U;
}

void testDeepMatchExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    std::vector<const nativesharp::ast::AstNode*> matches;
    std::vector<MatchTypedSyntax> typedSyntax;
    std::vector<nativesharp::frontend::MethodMatchExpressionBinding> bindings;
    syntax.reserve(nestingDepth * 5U + 1U);
    matches.reserve(nestingDepth);
    typedSyntax.reserve(nestingDepth * 4U + 1U);
    bindings.reserve(nestingDepth);
    nativesharp::frontend::MethodExpressionTypeBinding booleanType;
    booleanType.typeKey = "builtin:bool";
    auto* current = buildDeepMatchSyntax(
        nestingDepth, syntax, matches, typedSyntax, bindings, booleanType);
    nativesharp::frontend::ProjectMethodBodyModel methodBodyModel;
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerMatchExpression(
        model, methodBodyModel, current, matches, std::move(bindings), typedSyntax, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level match expression lowers without process-stack recursion (" + error + ")");
    tests.expect(observeDeepMatchDepth(lowered.get(), nestingDepth) == nestingDepth,
        "iterative match lowering preserves governing, condition, guard, and value continuations");
    destroyHirTreeIteratively(lowered);
}

void testMatchExpressionContract(TestContext& tests)
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    const auto makeNode = [&](nativesharp::ast::AstNodeKind kind)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            kind, nativesharp::SourceRange{}));
        return syntax.back().get();
    };
    auto* governing = makeNode(nativesharp::ast::AstNodeKind::IntegerLiteralExpression);
    auto* condition = makeNode(nativesharp::ast::AstNodeKind::BooleanLiteralExpression);
    condition->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
    auto* guard = makeNode(nativesharp::ast::AstNodeKind::BooleanLiteralExpression);
    guard->setTokenKind(nativesharp::lexer::TokenKind::KwFalse);
    auto* declaration = makeNode(nativesharp::ast::AstNodeKind::VariableDeclarationStatement);
    auto* value = makeNode(nativesharp::ast::AstNodeKind::IntegerLiteralExpression);
    auto* arm = makeNode(nativesharp::ast::AstNodeKind::MatchArmExpression);
    arm->setFirst(condition);
    arm->setSecond(guard);
    arm->setThird(value);
    arm->addChild(declaration);
    auto* match = makeNode(nativesharp::ast::AstNodeKind::MatchExpression);
    match->setFirst(governing);
    match->addChild(arm);
    nativesharp::frontend::MethodExpressionTypeBinding integerType;
    integerType.typeKey = "builtin:int";
    nativesharp::frontend::MethodExpressionTypeBinding longType;
    longType.typeKey = "builtin:long";
    nativesharp::frontend::MethodExpressionTypeBinding booleanType;
    booleanType.typeKey = "builtin:bool";
    const std::vector<std::pair<const nativesharp::ast::AstNode*,
        const nativesharp::frontend::MethodExpressionTypeBinding*>> typedSyntax{
        {governing, &integerType}, {condition, &booleanType}, {guard, &booleanType},
        {value, &integerType}, {match, &longType}};
    nativesharp::frontend::MethodMatchExpressionBinding binding;
    binding.syntax = match;
    binding.governingTypeKey = "builtin:int";
    binding.resultTypeKey = "builtin:long";
    binding.switchTagMemberOrdinal = 17U;
    binding.armTags = {{3, 5}};
    binding.armConversions.push_back(nativesharp::frontend::ProjectImplicitConversion{
        nativesharp::frontend::ProjectConversionKind::Numeric,
        1,
        "builtin:int",
        "builtin:long",
        std::nullopt});
    nativesharp::frontend::ProjectMethodBodyModel methodBodyModel;
    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::configureMatchCleanupLocal(
        model, methodBodyModel, declaration);
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerMatchExpression(
        model, methodBodyModel, match, {match}, {binding}, typedSyntax, false, error);
    const auto* loweredArm = lowered != nullptr && lowered->children.size() == 2U
        ? lowered->children[1].get() : nullptr;
    tests.expect(loweredArm != nullptr && error.empty() && lowered->booleanValue &&
            lowered->secondaryMemberOrdinal == 17U && loweredArm->switchLabels ==
                std::vector<std::int64_t>({3, 5}) && loweredArm->children.size() == 4U &&
            loweredArm->children[2]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::LocalDeclaration &&
            loweredArm->children[3]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Conversion &&
            loweredArm->cleanupRegionOrdinals == std::vector<std::size_t>({0U}) &&
            nativesharp::frontend::ProjectOopHirModelTestAccess::activeCleanupDepth(model) == 0U,
        "iterative match lowering preserves tags, declaration order, conversion, and cleanup scope");
    destroyHirTreeIteratively(lowered);
}

void testReferenceMatchExpressionLowering(TestContext& tests)
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    const auto makeNode = [&](nativesharp::ast::AstNodeKind kind)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            kind, nativesharp::SourceRange{}));
        return syntax.back().get();
    };
    auto* governing = makeNode(nativesharp::ast::AstNodeKind::MatchValueExpression);
    auto* condition = makeNode(nativesharp::ast::AstNodeKind::BooleanLiteralExpression);
    condition->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
    auto* value = makeNode(nativesharp::ast::AstNodeKind::MatchValueExpression);
    auto* arm = makeNode(nativesharp::ast::AstNodeKind::MatchArmExpression);
    arm->setFirst(condition);
    arm->setThird(value);
    auto* match = makeNode(nativesharp::ast::AstNodeKind::MatchExpression);
    match->setFirst(governing);
    match->addChild(arm);
    nativesharp::frontend::MethodExpressionTypeBinding integerType;
    integerType.typeKey = "builtin:int";
    nativesharp::frontend::MethodExpressionTypeBinding booleanType;
    booleanType.typeKey = "builtin:bool";
    const std::vector<std::pair<const nativesharp::ast::AstNode*,
        const nativesharp::frontend::MethodExpressionTypeBinding*>> typedSyntax{
        {governing, &integerType}, {condition, &booleanType},
        {value, &integerType}, {match, &integerType}};
    nativesharp::frontend::MethodMatchExpressionBinding binding;
    binding.syntax = match;
    binding.governingTypeKey = "builtin:int";
    binding.resultTypeKey = "builtin:int";
    binding.referenceResult = true;
    binding.governingReference = true;
    binding.armConversions.emplace_back();
    nativesharp::frontend::ProjectMethodBodyModel methodBodyModel;
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerMatchExpression(
        model, methodBodyModel, match, {match}, {binding}, typedSyntax, false, error);
    const auto* matchNode = lowered != nullptr && lowered->children.size() == 1U
        ? lowered->children.front().get() : nullptr;
    tests.expect(matchNode != nullptr && error.empty() &&
            lowered->kind == nativesharp::frontend::ProjectOopHirNodeKind::PointerLoad &&
            matchNode->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::MatchExpression &&
            matchNode->referenceResult && matchNode->governingReference &&
            matchNode->children[0]->referenceResult &&
            matchNode->children[1]->children.back()->referenceResult,
        "a reference match preserves address lowering and adds one value-context pointer load");
    destroyHirTreeIteratively(lowered);
}

void testDeepMixedExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 2U + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        if (index % 3U == 0U)
        {
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::UnaryExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Minus);
            syntax.back()->setFirst(current);
            current = syntax.back().get();
            continue;
        }
        if (index % 3U == 1U)
        {
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setIntegerValue(index + 2U);
            nativesharp::ast::AstNode* right = syntax.back().get();
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::BinaryExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Plus);
            syntax.back()->setFirst(current);
            syntax.back()->setSecond(right);
            current = syntax.back().get();
            continue;
        }

        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
        nativesharp::ast::AstNode* condition = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setIntegerValue(index + 2U);
        nativesharp::ast::AstNode* whenTrue = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ConditionalExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(condition);
        syntax.back()->setSecond(whenTrue);
        syntax.back()->setThird(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level mixed expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    auto* cursor = lowered.get();
    while (cursor != nullptr && observedDepth < nestingDepth)
    {
        const std::size_t sourceIndex = nestingDepth - observedDepth - 1U;
        if (sourceIndex % 3U == 0U)
        {
            if (cursor->kind != nativesharp::frontend::ProjectOopHirNodeKind::Unary ||
                cursor->children.size() != 1U) break;
            cursor = cursor->children[0].get();
        }
        else if (sourceIndex % 3U == 1U)
        {
            if (cursor->kind != nativesharp::frontend::ProjectOopHirNodeKind::Binary ||
                cursor->children.size() != 2U) break;
            cursor = cursor->children[0].get();
        }
        else
        {
            if (cursor->kind != nativesharp::frontend::ProjectOopHirNodeKind::Conditional ||
                cursor->children.size() != 3U) break;
            cursor = cursor->children[2].get();
        }
        ++observedDepth;
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative mixed lowering preserves every family transition and its literal leaf");
    destroyHirTreeIteratively(lowered);
}

void testDeepStaticAssertEvaluation(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 2U + 2U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        if (index % 2U == 0U)
        {
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::UnaryExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Bang);
            syntax.back()->setFirst(current);
            current = syntax.back().get();
            continue;
        }

        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
        nativesharp::ast::AstNode* condition = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwFalse);
        nativesharp::ast::AstNode* whenFalse = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ConditionalExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(condition);
        syntax.back()->setSecond(current);
        syntax.back()->setThird(whenFalse);
        current = syntax.back().get();
    }
    auto statement = std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::StaticAssertStatement,
        nativesharp::SourceRange{});
    statement->setFirst(current);

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:bool";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerStatement(
        model, statement.get(), syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level static_assert constant evaluates without process-stack recursion (" +
            error + ")");
    tests.expect(lowered != nullptr &&
            lowered->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block,
        "deep static_assert preserves the successful empty-block lowering contract");
    destroyHirTreeIteratively(lowered);
}

void testStaticAssertShortCircuitEvaluation(TestContext& tests)
{
    const auto evaluate = [&](nativesharp::lexer::TokenKind operation,
                              nativesharp::lexer::TokenKind leftToken,
                              std::string expectedError)
    {
        std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(leftToken);
        nativesharp::ast::AstNode* left = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::NullLiteralExpression,
            nativesharp::SourceRange{}));
        nativesharp::ast::AstNode* invalidIfVisited = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BinaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(operation);
        syntax.back()->setFirst(left);
        syntax.back()->setSecond(invalidIfVisited);
        nativesharp::ast::AstNode* expression = syntax.back().get();
        auto statement = std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::StaticAssertStatement,
            nativesharp::SourceRange{});
        statement->setFirst(expression);

        nativesharp::frontend::MethodExpressionTypeBinding binding;
        binding.typeKey = "builtin:bool";
        ProjectOopHirModel model;
        std::string error;
        auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerStatement(
            model, statement.get(), syntax, binding, error);
        tests.expect(error == expectedError,
            "static_assert preserves short-circuit selection without evaluating the unused operand");
        destroyHirTreeIteratively(lowered);
    };

    evaluate(
        nativesharp::lexer::TokenKind::AmpAmp,
        nativesharp::lexer::TokenKind::KwFalse,
        "static assertion failed");
    evaluate(
        nativesharp::lexer::TokenKind::PipePipe,
        nativesharp::lexer::TokenKind::KwTrue,
        "");
}

void testDeepHirDump(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 8192;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    root->typeKey = "builtin:void";
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        child->typeKey = "builtin:void";
        cursor->children.push_back(std::move(child));
        cursor = cursor->children.front().get();
    }

    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperBody(
        model, std::move(root));
    const std::string dump = nativesharp::frontend::ProjectOopHirModelDumper::dump(model);
    tests.expect(
        static_cast<std::size_t>(std::count(dump.begin(), dump.end(), '\n')) ==
            nestingDepth + 4U,
        "an 8192-level HIR tree dumps without process-stack recursion");
    tests.expect(dump.starts_with("project.oop.hir functions=1\n") &&
            dump.find("function #0 deep_hir_dump abi=static") != std::string::npos &&
            dump.find("field-initializers=0\n") != std::string::npos,
        "iterative HIR dumping preserves header, function contract, preorder, and footer");
    auto body = nativesharp::frontend::ProjectOopHirModelTestAccess::takeDumperBody(model);
    destroyHirTreeIteratively(body);
}

void testDeepHirCoordinatorWalker(TestContext& tests, std::string_view walker)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(child));
        cursor = cursor->children.front().get();
    }

    cursor->kind = nativesharp::frontend::ProjectOopHirNodeKind::OwnedTemporaryBorrow;
    cursor->localOrdinal = 100U;
    cursor->cleanupRegionOrdinal = 200U;
    cursor->hirRegionOrdinal = 5U;
    cursor->exceptionTargetRegionOrdinal = 6U;
    auto declaration = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    declaration->kind = nativesharp::frontend::ProjectOopHirNodeKind::LocalDeclaration;
    declaration->localOrdinal = 77U;
    auto* declarationPointer = declaration.get();
    auto construction = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    construction->kind = nativesharp::frontend::ProjectOopHirNodeKind::ObjectConstruction;
    auto* constructionPointer = construction.get();
    declaration->children.push_back(std::move(construction));
    cursor->children.push_back(std::move(declaration));

    if (walker == "collect")
    {
        auto secondDeclaration = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        secondDeclaration->kind =
            nativesharp::frontend::ProjectOopHirNodeKind::LocalDeclaration;
        auto* secondDeclarationPointer = secondDeclaration.get();
        root->children.push_back(std::move(secondDeclaration));
        std::vector<nativesharp::frontend::ProjectOopHirNode*> declarations;
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectLocalDeclarations(
            root.get(), declarations);
        tests.expect(declarations.size() == 2U && declarations[0] == declarationPointer &&
                declarations[1] == secondDeclarationPointer,
            "deep HIR declaration collection preserves preorder without process recursion");
    }
    else if (walker == "uses")
    {
        tests.expect(
            nativesharp::frontend::ProjectOopHirModelTestAccess::stackClassLocalUsesAreSafe(
                root.get(), declarationPointer, 77U),
            "deep HIR stack-promotion use proof completes without process recursion");

        auto assignment = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        assignment->kind = nativesharp::frontend::ProjectOopHirNodeKind::Assignment;
        auto fieldAddress = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        fieldAddress->kind = nativesharp::frontend::ProjectOopHirNodeKind::FieldAddress;
        auto localValue = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        localValue->kind = nativesharp::frontend::ProjectOopHirNodeKind::LocalValue;
        localValue->localOrdinal = 77U;
        fieldAddress->children.push_back(std::move(localValue));
        assignment->children.push_back(std::move(fieldAddress));
        tests.expect(
            nativesharp::frontend::ProjectOopHirModelTestAccess::stackClassLocalUsesAreSafe(
                assignment.get(), declarationPointer, 77U),
            "iterative stack-promotion proof preserves direct field-store safety");
        assignment->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        tests.expect(
            !nativesharp::frontend::ProjectOopHirModelTestAccess::stackClassLocalUsesAreSafe(
                assignment.get(), declarationPointer, 77U),
            "iterative stack-promotion proof still rejects an escaping field address");
        destroyHirTreeIteratively(assignment);
    }
    else if (walker == "allocation")
    {
        auto laterConstruction = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        laterConstruction->kind =
            nativesharp::frontend::ProjectOopHirNodeKind::ObjectConstruction;
        root->children.push_back(std::move(laterConstruction));
        tests.expect(
            nativesharp::frontend::ProjectOopHirModelTestAccess::firstUnpromotedObjectConstruction(
                root.get()) == constructionPointer,
            "deep HIR allocation search preserves first-preorder result");
    }
    else if (walker == "scan")
    {
        std::size_t localCount = 0;
        std::size_t cleanupCount = 0;
        std::size_t regionCount = 0;
        nativesharp::frontend::ProjectOopHirModelTestAccess::scanParallelOrdinals(
            root.get(), 90U, 190U, localCount, cleanupCount, regionCount);
        tests.expect(localCount == 11U && cleanupCount == 11U && regionCount == 6U,
            "deep HIR ordinal scan preserves synthetic maxima");
    }
    else if (walker == "apply")
    {
        nativesharp::frontend::ProjectOopHirModelTestAccess::applyParallelOrdinalOffsets(
            root.get(), 90U, 190U, 3U, 4U, 7U);
        tests.expect(cursor->localOrdinal == 103U && cursor->cleanupRegionOrdinal == 204U &&
                cursor->hirRegionOrdinal == 12U && cursor->exceptionTargetRegionOrdinal == 13U,
            "deep HIR ordinal rebasing preserves all offsets");
    }
    destroyHirTreeIteratively(root);
}

void testDeepHirModelDestruction(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    const auto makeDeepTree = []
    {
        auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        auto* cursor = root.get();
        for (std::size_t index = 0; index < nestingDepth; ++index)
        {
            auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
            child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
            cursor->children.push_back(std::move(child));
            cursor = cursor->children.front().get();
        }
        return root;
    };

    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperBody(
        model, makeDeepTree());
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperFieldInitializer(
        model, makeDeepTree());
    tests.expect(true,
        "32768-level HIR function and field roots reach model destruction without extraction");
}

void testDeepHirNodeDestruction(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(child));
        cursor = cursor->children.front().get();
    }
    root.reset();
    tests.expect(true,
        "a 32768-level standalone HIR temporary is destroyed without process-stack recursion");
}

void testDeepHirBlockLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::BlockStatement,
        nativesharp::SourceRange{}));
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BlockStatement,
            nativesharp::SourceRange{}));
        syntax.back()->addChild(current);
        current = syntax.back().get();
    }

    ProjectOopHirModel model;
    std::string error;
    std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::lowerBlock(
            model, current, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level block chain lowers without process-stack recursion (" + error + ")");
    const auto* cursor = lowered.get();
    std::size_t observedDepth = 0;
    while (cursor != nullptr && cursor->children.size() == 1U &&
        cursor->children.front() != nullptr &&
        cursor->children.front()->kind ==
            nativesharp::frontend::ProjectOopHirNodeKind::Block)
    {
        cursor = cursor->children.front().get();
        ++observedDepth;
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr && cursor->children.empty(),
        "deep block lowering preserves every block and the empty leaf");
}

struct DeepDeconstructionAssignmentFixture final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    std::vector<nativesharp::frontend::MethodDeconstructionBinding> bindings;
    std::vector<nativesharp::frontend::MethodNameBinding> names;
    std::vector<nativesharp::frontend::MethodLocalSymbol> locals;
    const nativesharp::ast::AstNode* source = nullptr;
    const nativesharp::ast::AstNode* target = nullptr;
    const nativesharp::ast::AstNode* root = nullptr;
    std::string tupleTypeKey;
};

DeepDeconstructionAssignmentFixture makeDeepDeconstructionAssignmentFixture(std::size_t depth)
{
    using AstKind = nativesharp::ast::AstNodeKind;
    DeepDeconstructionAssignmentFixture fixture;
    const std::array<std::string, 1U> elementTypes{"builtin:int"};
    fixture.tupleTypeKey = nativesharp::frontend::ProjectTypeConversion::tupleTypeKey(elementTypes);
    fixture.syntax.reserve(depth + 3U);
    fixture.bindings.reserve(depth);
    fixture.names.reserve(2U);
    fixture.locals.resize(2U);
    const auto makeNode = [&] (AstKind kind)
    {
        fixture.syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            kind, nativesharp::SourceRange{}));
        return fixture.syntax.back().get();
    };
    auto* source = makeNode(AstKind::IdentifierExpression);
    source->setText("source");
    auto* target = makeNode(AstKind::IdentifierExpression);
    target->setText("target");
    auto* pattern = makeNode(AstKind::TupleExpression);
    pattern->addChild(target);
    nativesharp::ast::AstNode* current = source;
    for (std::size_t index = 0; index < depth; ++index)
    {
        auto* assignment = makeNode(AstKind::AssignmentExpression);
        assignment->setTokenKind(nativesharp::lexer::TokenKind::Equal);
        assignment->setFirst(pattern);
        assignment->setSecond(current);
        fixture.bindings.push_back(nativesharp::frontend::MethodDeconstructionBinding{
            assignment, current, fixture.tupleTypeKey, fixture.tupleTypeKey, std::nullopt});
        current = assignment;
    }
    nativesharp::frontend::MethodNameBinding sourceName;
    sourceName.kind = nativesharp::frontend::MethodNameBindingKind::Local;
    sourceName.syntax = source;
    sourceName.name = "source";
    sourceName.typeKey = fixture.tupleTypeKey;
    sourceName.localOrdinal = 0U;
    fixture.names.push_back(std::move(sourceName));
    nativesharp::frontend::MethodNameBinding targetName;
    targetName.kind = nativesharp::frontend::MethodNameBindingKind::Local;
    targetName.syntax = target;
    targetName.name = "target";
    targetName.typeKey = "builtin:int";
    targetName.localOrdinal = 1U;
    fixture.names.push_back(std::move(targetName));
    fixture.locals[0].syntax = source;
    fixture.locals[0].name = "source";
    fixture.locals[0].typeKey = fixture.tupleTypeKey;
    fixture.locals[1].syntax = target;
    fixture.locals[1].name = "target";
    fixture.locals[1].typeKey = "builtin:int";
    fixture.source = source;
    fixture.target = target;
    fixture.root = current;
    return fixture;
}

bool deepDeconstructionAssignmentMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t depth,
    std::string_view tupleTypeKey)
{
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    for (std::size_t index = 0; index < depth; ++index)
    {
        if (node == nullptr || node->kind != Kind::TupleDeconstruct ||
            node->typeKey != tupleTypeKey || node->children.size() != 2U)
            return false;
        const auto* element = node->children[1].get();
        if (element->kind != Kind::TupleDeconstructElement ||
            element->typeKey != "builtin:int" || element->elementTypeKey != "builtin:int" ||
            element->integerValue != 0U || !element->conversion.has_value() ||
            element->conversion->kind != nativesharp::frontend::ProjectConversionKind::Identity ||
            element->children.size() != 1U ||
            element->children.front()->kind != Kind::LocalAddress ||
            element->children.front()->localOrdinal != 1U)
            return false;
        node = node->children.front().get();
    }
    return node != nullptr && node->kind == Kind::LocalValue &&
        node->localOrdinal == 0U && node->typeKey == tupleTypeKey;
}

void testDeepDeconstructionAssignmentLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    DeepDeconstructionAssignmentFixture fixture =
        makeDeepDeconstructionAssignmentFixture(nestingDepth);
    nativesharp::frontend::MethodExpressionTypeBinding tupleType;
    tupleType.typeKey = fixture.tupleTypeKey;
    nativesharp::frontend::MethodExpressionTypeBinding intType;
    intType.typeKey = "builtin:int";
    const std::vector<std::pair<const nativesharp::ast::AstNode*,
        const nativesharp::frontend::MethodExpressionTypeBinding*>> typedSyntax{
            {fixture.source, &tupleType}, {fixture.target, &intType}};
    nativesharp::frontend::ProjectMethodBodyModel methodBodyModel;
    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerDeconstructionExpression(
            model, methodBodyModel, nameLookup, objectLayoutModel, fixture.root,
            std::move(fixture.bindings), std::move(fixture.locals), fixture.names,
            typedSyntax, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level deconstruction-assignment source chain lowers iteratively (" +
            error + ")");
    tests.expect(deepDeconstructionAssignmentMatches(
            lowered.get(), nestingDepth, fixture.tupleTypeKey),
        "iterative deconstruction preserves source-before-target order, offsets, conversions, and addresses");
    destroyHirTreeIteratively(lowered);
}

void testDeepHirDeconstructionWalk(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IdentifierExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setText("leaf");
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::TupleExpression,
            nativesharp::SourceRange{}));
        syntax.back()->addChild(current);
        current = syntax.back().get();
    }

    std::size_t expanded = 0;
    std::size_t leaves = 0;
    std::uint64_t observedOffset = 0;
    const auto expand = [&] (
        const nativesharp::ast::AstNode* tuple,
        const std::string& typeKey,
        std::uint64_t byteOffset,
        std::vector<nativesharp::frontend::HirTuplePatternFrame>& children)
    {
        ++expanded;
        children.push_back(nativesharp::frontend::HirTuplePatternFrame{
            tuple->children().front(), typeKey, byteOffset + 1U});
        return true;
    };
    const auto visit = [&] (
        const nativesharp::ast::AstNode*,
        const std::string&,
        std::uint64_t byteOffset)
    {
        ++leaves;
        observedOffset = byteOffset;
        return true;
    };
    const bool ok = nativesharp::frontend::visitHirTuplePatternLeaves(
        current, "synthetic:tuple", 0, expand, visit);
    tests.expect(ok && expanded == nestingDepth && leaves == 1U &&
            observedOffset == nestingDepth,
        "a 32768-level deconstruction pattern preserves preorder, type, and accumulated offset");

    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> branchSyntax;
    const auto makeBranchNode = [&] (nativesharp::ast::AstNodeKind kind, std::string text = {})
    {
        branchSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            kind, nativesharp::SourceRange{}));
        branchSyntax.back()->setText(std::move(text));
        return branchSyntax.back().get();
    };
    auto* branchRoot = makeBranchNode(nativesharp::ast::AstNodeKind::TupleExpression);
    auto* left = makeBranchNode(nativesharp::ast::AstNodeKind::TupleExpression);
    auto* right = makeBranchNode(nativesharp::ast::AstNodeKind::TupleExpression);
    left->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "a"));
    left->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "b"));
    right->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "c"));
    right->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "d"));
    branchRoot->addChild(left);
    branchRoot->addChild(right);
    const auto expandBranch = [] (
        const nativesharp::ast::AstNode* tuple,
        const std::string& typeKey,
        std::uint64_t byteOffset,
        std::vector<nativesharp::frontend::HirTuplePatternFrame>& children)
    {
        for (std::size_t index = 0; index < tuple->children().size(); ++index)
            children.push_back(nativesharp::frontend::HirTuplePatternFrame{
                tuple->children()[index], typeKey, byteOffset + index});
        return true;
    };
    std::vector<std::string> order;
    const bool branchOk = nativesharp::frontend::visitHirTuplePatternLeaves(
        branchRoot, "synthetic:branch", 0, expandBranch,
        [&] (const nativesharp::ast::AstNode* leaf, const std::string&, std::uint64_t)
        {
            order.push_back(leaf->text());
            return true;
        });
    tests.expect(branchOk && order == std::vector<std::string>({"a", "b", "c", "d"}),
        "iterative deconstruction traversal preserves left-to-right leaf order");

    order.clear();
    const bool stopped = nativesharp::frontend::visitHirTuplePatternLeaves(
        branchRoot, "synthetic:branch", 0, expandBranch,
        [&] (const nativesharp::ast::AstNode* leaf, const std::string&, std::uint64_t)
        {
            order.push_back(leaf->text());
            return leaf->text() != "c";
        });
    tests.expect(!stopped && order == std::vector<std::string>({"a", "b", "c"}),
        "iterative deconstruction traversal stops at the first failing leaf");
}

void testDeepAggregateLeaves(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 4096;
    constexpr std::uint64_t initialOffset = 13U;

    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectInheritanceModel inheritanceModel;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    ProjectOopHirModel model;
    std::vector<std::string> typeKeys;
    const bool configured =
        nativesharp::frontend::ProjectOopHirModelTestAccess::configureAggregateLeafChain(
            model,
            nameLookup,
            inheritanceModel,
            objectLayoutModel,
            nestingDepth,
            typeKeys);
    tests.expect(configured, "the deep aggregate fixture builds valid name and layout models");
    if (!configured)
    {
        return;
    }

    nativesharp::ast::AstNode syntax(
        nativesharp::ast::AstNodeKind::IdentifierExpression,
        nativesharp::SourceRange{});
    std::vector<std::unique_ptr<nativesharp::frontend::ProjectOopHirNode>> leaves;
    std::string error;
    const bool lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateLeaves(
            model,
            typeKeys.front(),
            initialOffset,
            &syntax,
            leaves,
            error);
    tests.expect(lowered && error.empty(),
        "a 4096-level aggregate leaf chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(leaves.size() == 1U &&
            leaves.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::AggregateElement &&
            leaves.front()->typeKey == "builtin:int" &&
            leaves.front()->integerValue == initialOffset + 8U * nestingDepth &&
            leaves.front()->syntax == &syntax &&
            leaves.front()->children.empty(),
        "deep aggregate flattening preserves the scalar leaf, provenance, and absolute offset");

    leaves.clear();
    error.clear();
    const bool arrayLowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateLeaves(
            model, "builtin:int[3]", initialOffset, &syntax, leaves, error);
    tests.expect(arrayLowered && error.empty() && leaves.size() == 3U &&
            leaves[0]->integerValue == initialOffset &&
            leaves[1]->integerValue == initialOffset + 4U &&
            leaves[2]->integerValue == initialOffset + 8U,
        "iterative fixed-array flattening preserves ascending source order and stride");

    nativesharp::frontend::ObjectFieldLayout firstField;
    firstField.offset = 8U;
    firstField.typeKey = "builtin:int";
    nativesharp::frontend::ObjectFieldLayout secondField;
    secondField.offset = 24U;
    secondField.typeKey = "builtin:long";
    std::vector<nativesharp::frontend::ObjectFieldLayout> rootFields;
    rootFields.push_back(std::move(firstField));
    rootFields.push_back(std::move(secondField));
    nativesharp::frontend::ProjectOopHirModelTestAccess::setAggregateRootFields(
        objectLayoutModel, std::move(rootFields));
    leaves.clear();
    error.clear();
    const bool structLowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateLeaves(
            model, typeKeys.front(), initialOffset, &syntax, leaves, error);
    tests.expect(structLowered && error.empty() && leaves.size() == 2U &&
            leaves[0]->typeKey == "builtin:int" &&
            leaves[0]->integerValue == initialOffset + 8U &&
            leaves[1]->typeKey == "builtin:long" &&
            leaves[1]->integerValue == initialOffset + 24U,
        "iterative struct flattening preserves declared field order and absolute offsets");
}

void testDeepAggregateElements(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 4096;
    constexpr std::uint64_t initialOffset = 17U;

    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectInheritanceModel inheritanceModel;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    nativesharp::frontend::ProjectMethodBodyModel methodBodyModel;
    ProjectOopHirModel model;
    std::vector<std::string> typeKeys;
    const bool configured =
        nativesharp::frontend::ProjectOopHirModelTestAccess::configureAggregateLeafChain(
            model,
            nameLookup,
            inheritanceModel,
            objectLayoutModel,
            nestingDepth,
            typeKeys);
    tests.expect(configured, "the deep aggregate element fixture builds valid semantic models");
    if (!configured)
    {
        return;
    }

    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(7U);
    auto* literal = syntax.back().get();
    auto* current = literal;
    std::vector<nativesharp::ast::AstNode*> aggregates(nestingDepth);
    for (std::size_t remaining = nestingDepth; remaining > 0; --remaining)
    {
        const std::size_t ordinal = remaining - 1U;
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::AggregateInitializerExpression,
            nativesharp::SourceRange{}));
        syntax.back()->addChild(current);
        current = syntax.back().get();
        aggregates[ordinal] = current;
    }

    nativesharp::frontend::MethodExpressionTypeBinding literalType;
    literalType.syntax = literal;
    literalType.typeKey = "builtin:int";
    nativesharp::frontend::ProjectOopHirModelTestAccess::configureAggregateElementBindings(
        model, methodBodyModel, aggregates, literal, typeKeys, literalType);

    std::vector<std::unique_ptr<nativesharp::frontend::ProjectOopHirNode>> elements;
    std::string error;
    const bool lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateElements(
            model,
            aggregates.front(),
            typeKeys.front(),
            initialOffset,
            elements,
            error);
    tests.expect(lowered && error.empty(),
        "a 4096-level aggregate initializer lowers without process-stack recursion (" +
            error + ")");
    tests.expect(elements.size() == 1U &&
            elements.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::AggregateElement &&
            elements.front()->typeKey == "builtin:int" &&
            elements.front()->integerValue == initialOffset + 8U * nestingDepth &&
            elements.front()->syntax == literal &&
            elements.front()->children.size() == 1U &&
            elements.front()->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant &&
            elements.front()->children.front()->integerValue == 7U,
        "deep aggregate lowering preserves the leaf value, provenance, and absolute offset");
}

std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> makeDeepMirHirChain(
    std::size_t depth)
{
    using Node = nativesharp::frontend::ProjectOopHirNode;
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    auto root = std::make_unique<Node>();
    Node* current = root.get();
    for (std::size_t ordinal = 0; ordinal < depth; ++ordinal)
    {
        current->kind = Kind::Block;
        current->typeKey = "builtin:void";
        current->localOrdinal = ordinal;
        if (ordinal + 1U == depth) break;
        current->children.push_back(std::make_unique<Node>());
        current = current->children.back().get();
    }
    root->children.push_back(nullptr);
    return root;
}

bool deepMirChainMatches(const ProjectOopMirNode* root, std::size_t depth)
{
    const ProjectOopMirNode* current = root;
    for (std::size_t ordinal = 0; ordinal < depth; ++ordinal)
    {
        if (current == nullptr || current->kind != ProjectOopMirNodeKind::Block ||
            current->localOrdinal != ordinal) return false;
        const std::size_t expectedChildren = ordinal == 0U ? 2U : ordinal + 1U < depth ? 1U : 0U;
        if (current->children.size() != expectedChildren) return false;
        if (ordinal == 0U && current->children[1] != nullptr) return false;
        current = ordinal + 1U < depth ? current->children[0].get() : nullptr;
    }
    return current == nullptr;
}

void destroyMirTreeIteratively(std::unique_ptr<ProjectOopMirNode> root)
{
    std::vector<std::unique_ptr<ProjectOopMirNode>> work;
    if (root != nullptr) work.push_back(std::move(root));
    while (!work.empty())
    {
        std::unique_ptr<ProjectOopMirNode> current = std::move(work.back());
        work.pop_back();
        for (auto& child : current->children)
            if (child != nullptr) work.push_back(std::move(child));
        current->children.clear();
    }
}

void testDeepMirNodeLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    auto hir = makeDeepMirHirChain(nestingDepth);
    ProjectOopMirModel model;
    std::string error;
    auto mir = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerMirNode(
        model, hir.get(), error);
    tests.expect(mir != nullptr && error.empty(),
        "a 32768-level HIR tree lowers to MIR without process-stack recursion (" + error + ")");
    tests.expect(deepMirChainMatches(mir.get(), nestingDepth),
        "iterative MIR lowering preserves preorder, node identity, child order, and null slots");
    destroyMirTreeIteratively(std::move(mir));
}

void testMirAggregateSpreadDescriptor(TestContext& tests)
{
    using HirNode = nativesharp::frontend::ProjectOopHirNode;
    using HirKind = nativesharp::frontend::ProjectOopHirNodeKind;
    auto spread = std::make_unique<HirNode>();
    spread->kind = HirKind::AggregateSpread;
    auto element = std::make_unique<HirNode>();
    element->kind = HirKind::AggregateElement;
    element->typeKey = "builtin:int";
    element->integerValue = 19U;
    element->secondaryIntegerValue = 73U;
    spread->children.push_back(std::move(element));
    ProjectOopMirModel model;
    std::string error;
    auto mir = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerMirNode(
        model, spread.get(), error);
    const ProjectOopMirNode* lowered = mir != nullptr && mir->children.size() == 1U
        ? mir->children.front().get() : nullptr;
    tests.expect(mir != nullptr && error.empty() &&
            mir->kind == ProjectOopMirNodeKind::AggregateSpreadStore &&
            lowered != nullptr && lowered->kind == ProjectOopMirNodeKind::AggregateElementStore &&
            lowered->immediate == 19U && lowered->secondaryImmediate == 73U,
        "iterative MIR lowering preserves the completed AggregateSpread descriptor override");
}

void testDeepMirFutureKeyDiscovery(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    auto body = makeDeepMirHirChain(nestingDepth);
    nativesharp::frontend::ProjectOopHirNode* leaf = body.get();
    while (!leaf->children.empty() && leaf->children.front() != nullptr)
        leaf = leaf->children.front().get();
    body->typeKey = "future:builtin:long";
    body->elementTypeKey = "future:builtin:long";
    leaf->typeKey = "future:builtin:int";
    leaf->argumentTypeKeys.push_back("future:builtin:int");
    const std::vector<std::string> keys =
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectFutureLayoutKeys(body.get());
    tests.expect(keys == std::vector<std::string>{
            "future:builtin:long", "future:builtin:long",
            "future:builtin:int", "future:builtin:int"},
        "future keys preserve preorder and duplicates through 32768 HIR levels");
    destroyHirTreeIteratively(body);
}

void testMirFutureLayoutPublication(TestContext& tests)
{
    auto body = makeDeepMirHirChain(2U);
    body->typeKey = "future:builtin:long";
    body->elementTypeKey = "future:builtin:long";
    body->children.front()->typeKey = "future:builtin:int";
    body->children.front()->argumentTypeKeys.push_back("future:builtin:int");
    ProjectOopHirModel hirModel;
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperBody(
        hirModel, std::move(body));
    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectMemberModel memberModel;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    ProjectOopMirModel mirModel;
    std::string error;
    const bool imported = nativesharp::frontend::ProjectOopHirModelTestAccess::importMirLayouts(
        mirModel, nameLookup, memberModel, objectLayoutModel, hirModel, error);
    const auto layouts = mirModel.structLayouts();
    tests.expect(imported && error.empty(),
        "future layouts are published without changing the import contract (" + error + ")");
    tests.expect(layouts.size() == 2U && layouts[0].typeKey == "future:builtin:int" &&
            layouts[1].typeKey == "future:builtin:long",
        "iterative future discovery preserves preorder inputs and deterministic "
        "sort/deduplication");
    tests.expect(layouts.size() == 2U && layouts[0].fields.size() == 4U &&
            layouts[1].fields.size() == 4U && layouts[0].fields[2].typeKey == "builtin:int" &&
            layouts[1].fields[2].typeKey == "builtin:long",
        "iterative future discovery preserves the canonical physical result-field layouts");
    auto retainedBody =
        nativesharp::frontend::ProjectOopHirModelTestAccess::takeDumperBody(hirModel);
    destroyHirTreeIteratively(retainedBody);
}

void testDeepMirIteratorLayoutDiscovery(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    auto body = makeDeepMirHirChain(nestingDepth);
    nativesharp::frontend::ProjectOopHirNode* leaf = body.get();
    while (!leaf->children.empty() && leaf->children.front() != nullptr)
        leaf = leaf->children.front().get();
    body->typeKey = "async-iterator:builtin:long";
    body->elementTypeKey = "async-iterator:builtin:long";
    leaf->typeKey = "iterator:builtin:int";
    leaf->argumentTypeKeys.push_back("iterator:builtin:int");
    const std::vector<std::string> keys =
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectIteratorLayoutKeys(body.get());
    tests.expect(keys == std::vector<std::string>{
            "async-iterator:builtin:long", "async-iterator:builtin:long",
            "iterator:builtin:int", "iterator:builtin:int"},
        "iterator keys preserve preorder and duplicates through 32768 HIR levels");
    ProjectOopHirModel hirModel;
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperBody(
        hirModel, std::move(body));
    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectMemberModel memberModel;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    ProjectOopMirModel mirModel;
    std::string error;
    const bool imported = nativesharp::frontend::ProjectOopHirModelTestAccess::importMirLayouts(
        mirModel, nameLookup, memberModel, objectLayoutModel, hirModel, error);
    const auto layouts = mirModel.structLayouts();
    tests.expect(imported && error.empty(),
        "iterator layouts publish through 32768 HIR levels without stack recursion (" +
            error + ")");
    tests.expect(layouts.size() == 2U &&
            layouts[0].typeKey == "async-iterator:builtin:long" &&
            layouts[1].typeKey == "iterator:builtin:int",
        "iterative iterator discovery preserves deterministic sort/deduplication");
    tests.expect(layouts.size() == 2U && layouts[0].fields.size() == 5U &&
            layouts[1].fields.size() == 5U && layouts[0].fields[2].typeKey == "builtin:long" &&
            layouts[1].fields[2].typeKey == "builtin:int",
        "iterative iterator discovery preserves canonical physical item-field layouts");
    auto retainedBody =
        nativesharp::frontend::ProjectOopHirModelTestAccess::takeDumperBody(hirModel);
    destroyHirTreeIteratively(retainedBody);
}

void testDeepMirStringConstantEvaluation(TestContext& tests)
{
    using Node = nativesharp::frontend::ProjectOopHirNode;
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    constexpr std::size_t nestingDepth = 32768U;
    auto expression = std::make_unique<Node>();
    expression->kind = Kind::StringLiteral;
    expression->typeKey = "builtin:string";
    expression->stringValue = "deep-string";
    for (std::size_t depth = 0; depth < nestingDepth; ++depth)
    {
        auto conversion = std::make_unique<Node>();
        conversion->kind = Kind::Conversion;
        conversion->typeKey = "builtin:string";
        conversion->children.push_back(std::move(expression));
        expression = std::move(conversion);
    }
    const std::optional<std::string> value =
        nativesharp::frontend::MirConstantEvaluator::stringValue(expression.get(), {});
    tests.expect(value.has_value() && *value == "deep-string",
        "MIR string constant evaluation preserves 32768 conversions without stack recursion");
    destroyHirTreeIteratively(expression);
}

void testDeepMirIntegerConstantEvaluation(TestContext& tests)
{
    using Node = nativesharp::frontend::ProjectOopHirNode;
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    constexpr std::size_t nestingDepth = 32768U;
    auto expression = std::make_unique<Node>();
    expression->kind = Kind::IntegerConstant;
    expression->typeKey = "builtin:int";
    expression->integerValue = 1U;
    for (std::size_t depth = 0; depth < nestingDepth; ++depth)
    {
        auto parent = std::make_unique<Node>();
        parent->typeKey = "builtin:int";
        if (depth % 3U == 0U) parent->kind = Kind::Conversion;
        else if (depth % 3U == 1U)
        {
            parent->kind = Kind::Unary;
            parent->operation = nativesharp::lexer::TokenKind::Plus;
        }
        else
        {
            parent->kind = Kind::Binary;
            parent->operation = nativesharp::lexer::TokenKind::Plus;
            auto zero = std::make_unique<Node>();
            zero->kind = Kind::IntegerConstant;
            zero->typeKey = "builtin:int";
            parent->children.push_back(std::move(expression));
            parent->children.push_back(std::move(zero));
            expression = std::move(parent);
            continue;
        }
        parent->children.push_back(std::move(expression));
        expression = std::move(parent);
    }
    const std::optional<std::uint64_t> value =
        nativesharp::frontend::MirConstantEvaluator::integerValue(expression.get(), {});
    tests.expect(value.has_value() && *value == 1U,
        "MIR integer evaluation preserves 32768 mixed operations without stack recursion");
    destroyHirTreeIteratively(expression);
}

void testDeepMirFloatingConstantEvaluation(TestContext& tests)
{
    using Node = nativesharp::frontend::ProjectOopHirNode;
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    constexpr std::size_t nestingDepth = 32768U;
    auto expression = std::make_unique<Node>();
    expression->kind = Kind::FloatingConstant;
    expression->typeKey = "builtin:double";
    expression->floatingValue = 1.0;
    for (std::size_t depth = 0; depth < nestingDepth; ++depth)
    {
        auto parent = std::make_unique<Node>();
        parent->typeKey = "builtin:double";
        if (depth % 3U == 0U) parent->kind = Kind::Conversion;
        else if (depth % 3U == 1U)
        {
            parent->kind = Kind::Unary;
            parent->operation = nativesharp::lexer::TokenKind::Plus;
        }
        else
        {
            parent->kind = Kind::Binary;
            parent->operation = nativesharp::lexer::TokenKind::Plus;
            auto zero = std::make_unique<Node>();
            zero->kind = Kind::FloatingConstant;
            zero->typeKey = "builtin:double";
            parent->children.push_back(std::move(expression));
            parent->children.push_back(std::move(zero));
            expression = std::move(parent);
            continue;
        }
        parent->children.push_back(std::move(expression));
        expression = std::move(parent);
    }
    const std::optional<double> value =
        nativesharp::frontend::MirConstantEvaluator::floatingValue(expression.get(), {});
    tests.expect(value.has_value() && *value == 1.0,
        "MIR floating evaluation preserves 32768 mixed operations without stack recursion");
    destroyHirTreeIteratively(expression);
}

void testDeepMirBooleanConstantEvaluation(TestContext& tests)
{
    using Node = nativesharp::frontend::ProjectOopHirNode;
    using Kind = nativesharp::frontend::ProjectOopHirNodeKind;
    constexpr std::size_t nestingDepth = 32768U;
    auto expression = std::make_unique<Node>();
    expression->kind = Kind::BooleanConstant;
    expression->typeKey = "builtin:bool";
    expression->booleanValue = true;
    for (std::size_t depth = 0; depth < nestingDepth; ++depth)
    {
        auto parent = std::make_unique<Node>();
        parent->typeKey = "builtin:bool";
        if (depth % 4U == 0U) parent->kind = Kind::Conversion;
        else if (depth % 4U == 1U)
        {
            parent->kind = Kind::Unary;
            parent->operation = nativesharp::lexer::TokenKind::Bang;
        }
        else
        {
            parent->kind = Kind::Binary;
            const bool equality = depth % 4U == 2U;
            parent->operation = equality
                ? nativesharp::lexer::TokenKind::EqualEqual
                : nativesharp::lexer::TokenKind::AmpAmp;
            auto right = std::make_unique<Node>();
            right->kind = Kind::BooleanConstant;
            right->typeKey = "builtin:bool";
            right->booleanValue = !equality;
            parent->children.push_back(std::move(expression));
            parent->children.push_back(std::move(right));
            expression = std::move(parent);
            continue;
        }
        parent->children.push_back(std::move(expression));
        expression = std::move(parent);
    }
    const std::optional<bool> value =
        nativesharp::frontend::MirConstantEvaluator::booleanValue(expression.get(), {});
    tests.expect(value.has_value() && *value,
        "MIR boolean evaluation preserves 32768 mixed operations without stack recursion");
    destroyHirTreeIteratively(expression);
}

void testDeepMirCleanupContractCollection(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        if (depth % 4U == 0U)
        {
            node->kind = ProjectOopMirNodeKind::LocalDeclaration;
            node->cleanupRegionOrdinal = depth;
            node->localOrdinal = depth;
            node->typeKey = "builtin:int";
        }
        else if (depth % 4U == 1U)
        {
            node->kind = ProjectOopMirNodeKind::Return;
            node->targetScopeOrdinal = depth;
        }
        else if (depth % 4U == 2U)
        {
            node->kind = ProjectOopMirNodeKind::TryRegion;
            node->mirRegionOrdinal = depth;
        }
        else node->kind = ProjectOopMirNodeKind::CatchRegion;
        if (root != nullptr) node->children.push_back(std::move(root));
        root = std::move(node);
    }
    nativesharp::frontend::ProjectOopMirFunction function;
    std::string error;
    const bool collected =
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectMirCleanupContract(
            function, root.get(), nativesharp::language::PanicProfile::NativeUnwind, error);
    bool orderPreserved = function.cleanupRegions.size() == nestingDepth / 4U &&
        function.exceptionFlow.nonlocalExits.size() == nestingDepth / 4U &&
        function.exceptionFlow.protectedRegionOrdinals.size() == nestingDepth / 4U;
    for (std::size_t index = 0; orderPreserved && index < nestingDepth / 4U; ++index)
    {
        const std::size_t base = index * 4U;
        orderPreserved = function.cleanupRegions[index].ordinal == base &&
            function.cleanupRegions[index].action != nullptr &&
            function.cleanupRegions[index].action->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            function.exceptionFlow.nonlocalExits[index].node != nullptr &&
            function.exceptionFlow.nonlocalExits[index].node->kind == ProjectOopMirNodeKind::Return &&
            function.exceptionFlow.protectedRegionOrdinals[index] == base + 2U;
    }
    tests.expect(collected && error.empty() && orderPreserved &&
            function.exceptionFlow.catchRegionCount == nestingDepth / 4U &&
            function.exceptionFlow.requiresPersonality,
        "MIR cleanup collection preserves 32768-node preorder without stack recursion");
    destroyMirTreeIteratively(std::move(root));

    auto duplicateRoot = std::make_unique<ProjectOopMirNode>();
    duplicateRoot->kind = ProjectOopMirNodeKind::LocalDeclaration;
    duplicateRoot->cleanupRegionOrdinal = 7U;
    auto duplicateChild = std::make_unique<ProjectOopMirNode>();
    duplicateChild->kind = ProjectOopMirNodeKind::LocalDeclaration;
    duplicateChild->cleanupRegionOrdinal = 7U;
    duplicateRoot->children.push_back(std::move(duplicateChild));
    nativesharp::frontend::ProjectOopMirFunction duplicateFunction;
    std::string duplicateError;
    const bool duplicateCollected =
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectMirCleanupContract(
            duplicateFunction, duplicateRoot.get(), nativesharp::language::PanicProfile::Abort,
            duplicateError);
    tests.expect(!duplicateCollected && duplicateFunction.cleanupRegions.size() == 1U &&
            duplicateError == "OOP MIR contains duplicate cleanup-region identity #7",
        "MIR cleanup collection preserves the first preorder duplicate diagnostic");
    destroyMirTreeIteratively(std::move(duplicateRoot));
}

void testDeepMirCleanupReferenceValidation(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    nativesharp::frontend::ProjectOopMirFunction function;
    nativesharp::frontend::ProjectOopMirCleanupRegion cleanup;
    cleanup.ordinal = 7U;
    function.cleanupRegions.push_back(cleanup);
    function.exceptionFlow.protectedRegionOrdinals.push_back(11U);
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        node->kind = ProjectOopMirNodeKind::Block;
        node->cleanupRegionOrdinals.push_back(7U);
        node->exceptionTargetRegionOrdinal = 11U;
        if (root != nullptr) node->children.push_back(std::move(root));
        root = std::move(node);
    }
    std::string error;
    const bool valid =
        nativesharp::frontend::ProjectOopHirModelTestAccess::validateMirCleanupReferences(
            function, root.get(), error);
    tests.expect(valid && error.empty(),
        "MIR cleanup validation preserves 32768-node preorder without stack recursion");
    destroyMirTreeIteratively(std::move(root));

    auto cleanupFirst = std::make_unique<ProjectOopMirNode>();
    cleanupFirst->cleanupRegionOrdinals = {7U, 99U};
    cleanupFirst->exceptionTargetRegionOrdinal = 88U;
    auto missingChild = std::make_unique<ProjectOopMirNode>();
    missingChild->cleanupRegionOrdinals = {77U};
    cleanupFirst->children.push_back(std::move(missingChild));
    std::string cleanupError;
    const bool cleanupValid =
        nativesharp::frontend::ProjectOopHirModelTestAccess::validateMirCleanupReferences(
            function, cleanupFirst.get(), cleanupError);
    tests.expect(!cleanupValid &&
            cleanupError == "OOP MIR structured exit references missing cleanup region #99",
        "MIR cleanup validation preserves cleanup-reference and preorder diagnostic priority");
    destroyMirTreeIteratively(std::move(cleanupFirst));

    auto exceptionFirst = std::make_unique<ProjectOopMirNode>();
    exceptionFirst->cleanupRegionOrdinals = {7U};
    exceptionFirst->exceptionTargetRegionOrdinal = 88U;
    auto deferredChild = std::make_unique<ProjectOopMirNode>();
    deferredChild->cleanupRegionOrdinals = {77U};
    exceptionFirst->children.push_back(std::move(deferredChild));
    std::string exceptionError;
    const bool exceptionValid =
        nativesharp::frontend::ProjectOopHirModelTestAccess::validateMirCleanupReferences(
            function, exceptionFirst.get(), exceptionError);
    tests.expect(!exceptionValid &&
            exceptionError == "OOP MIR nonlocal exit references missing exception region #88",
        "MIR cleanup validation preserves exception-reference priority before child traversal");
    destroyMirTreeIteratively(std::move(exceptionFirst));
}

void testDeepMirAwaitedFrameSize(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        node->kind = depth % 2U == 0U
            ? ProjectOopMirNodeKind::AwaitFuture
            : ProjectOopMirNodeKind::Block;
        if (node->kind == ProjectOopMirNodeKind::AwaitFuture)
            node->elementTypeKey = "test:awaited";
        if (root != nullptr) node->children.push_back(std::move(root));
        root = std::move(node);
    }
    auto unmatched = std::make_unique<ProjectOopMirNode>();
    unmatched->kind = ProjectOopMirNodeKind::AwaitFuture;
    unmatched->elementTypeKey = "test:missing";
    root->children.push_back(std::move(unmatched));
    root->children.emplace_back();
    std::vector<nativesharp::frontend::ProjectOopMirStructLayout> layouts(3U);
    layouts[0].typeKey = "test:unrelated";
    layouts[0].size = 41U;
    layouts[1].typeKey = "test:awaited";
    layouts[1].size = 3U;
    layouts[2].typeKey = "test:awaited";
    layouts[2].size = 999U;
    const std::uint64_t frameSize =
        nativesharp::frontend::ProjectOopHirModelTestAccess::sumMirAwaitedFrameSize(
            root.get(), layouts);
    tests.expect(frameSize == nestingDepth / 2U * 3U,
        "MIR awaited frame sizing preserves first layout match, repeated awaits, and null slots");
    destroyMirTreeIteratively(std::move(root));
}

void testDeepMirPollLiveness(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        switch (depth % 8U)
        {
            case 0U: case 1U:
                node->kind = ProjectOopMirNodeKind::LocalDeclaration;
                node->localOrdinal = 7U;
                break;
            case 2U: case 7U:
                node->kind = ProjectOopMirNodeKind::ParameterValue;
                node->parameterOrdinal = 3U;
                break;
            case 3U: node->kind = ProjectOopMirNodeKind::SuspendPoint; break;
            case 4U:
                node->kind = ProjectOopMirNodeKind::LocalValue;
                node->localOrdinal = 7U;
                break;
            case 5U: node->kind = ProjectOopMirNodeKind::AsyncYieldValue; break;
            default:
                node->kind = ProjectOopMirNodeKind::LocalAddress;
                node->localOrdinal = 7U;
                break;
        }
        if (root != nullptr) node->children.push_back(std::move(root));
        node->children.emplace_back();
        root = std::move(node);
    }
    auto syncYield = std::make_unique<ProjectOopMirNode>();
    syncYield->kind = ProjectOopMirNodeKind::YieldValue;
    root->children.push_back(std::move(syncYield));
    std::uint64_t order = 0;
    std::unordered_map<std::size_t, std::uint64_t> localDefinition;
    std::unordered_map<std::size_t, std::uint64_t> localLastUse;
    std::unordered_map<std::size_t, std::uint64_t> parameterLastUse;
    std::vector<std::uint64_t> suspendOrders;
    nativesharp::frontend::ProjectOopHirModelTestAccess::collectMirPollLiveness(
        root.get(), order, localDefinition, localLastUse, parameterLastUse, suspendOrders);
    tests.expect(order == nestingDepth + 1U && localDefinition.size() == 1U &&
            localDefinition.at(7U) == 0U && localLastUse.size() == 1U &&
            localLastUse.at(7U) == 32766U && parameterLastUse.size() == 1U &&
            parameterLastUse.at(3U) == 32767U,
        "MIR poll liveness preserves preorder, first definitions, last uses, and null slots");
    tests.expect(suspendOrders.size() == nestingDepth / 4U &&
            suspendOrders.front() == 3U && suspendOrders[1] == 5U &&
            suspendOrders[suspendOrders.size() - 2U] == 32763U &&
            suspendOrders.back() == 32765U,
        "MIR poll liveness preserves SuspendPoint and AsyncYieldValue boundaries only");
    destroyMirTreeIteratively(std::move(root));
}

void testDeepMirMoveNextLiveness(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        switch (depth % 8U)
        {
            case 0U: case 1U:
                node->kind = ProjectOopMirNodeKind::LocalDeclaration;
                node->localOrdinal = 7U;
                break;
            case 2U: case 7U:
                node->kind = ProjectOopMirNodeKind::ParameterValue;
                node->parameterOrdinal = 3U;
                break;
            case 3U: node->kind = ProjectOopMirNodeKind::YieldValue; break;
            case 4U:
                node->kind = ProjectOopMirNodeKind::LocalValue;
                node->localOrdinal = 7U;
                break;
            case 5U: node->kind = ProjectOopMirNodeKind::AsyncYieldValue; break;
            default:
                node->kind = ProjectOopMirNodeKind::LocalAddress;
                node->localOrdinal = 7U;
                break;
        }
        if (root != nullptr) node->children.push_back(std::move(root));
        node->children.emplace_back();
        root = std::move(node);
    }
    auto suspend = std::make_unique<ProjectOopMirNode>();
    suspend->kind = ProjectOopMirNodeKind::SuspendPoint;
    root->children.push_back(std::move(suspend));
    std::uint64_t order = 0;
    std::unordered_map<std::size_t, std::uint64_t> localDefinition;
    std::unordered_map<std::size_t, std::uint64_t> localLastUse;
    std::unordered_map<std::size_t, std::uint64_t> parameterLastUse;
    std::vector<std::uint64_t> yieldOrders;
    nativesharp::frontend::ProjectOopHirModelTestAccess::collectMirMoveNextLiveness(
        root.get(), order, localDefinition, localLastUse, parameterLastUse, yieldOrders);
    tests.expect(order == nestingDepth + 1U && localDefinition.size() == 1U &&
            localDefinition.at(7U) == 0U && localLastUse.size() == 1U &&
            localLastUse.at(7U) == 32766U && parameterLastUse.size() == 1U &&
            parameterLastUse.at(3U) == 32767U,
        "MIR moveNext liveness preserves preorder, first definitions, last uses, and null slots");
    tests.expect(yieldOrders.size() == nestingDepth / 8U &&
            yieldOrders.front() == 3U && yieldOrders[1] == 11U &&
            yieldOrders[yieldOrders.size() - 2U] == 32755U &&
            yieldOrders.back() == 32763U,
        "MIR moveNext liveness treats only YieldValue as a frame boundary");
    destroyMirTreeIteratively(std::move(root));
}

std::unique_ptr<ProjectOopMirNode> makeDeepMirBorrowSourceTree()
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto declaration = std::make_unique<ProjectOopMirNode>();
        declaration->kind = ProjectOopMirNodeKind::LocalDeclaration;
        auto initializer = std::make_unique<ProjectOopMirNode>();
        initializer->kind = ProjectOopMirNodeKind::BorrowValue;
        switch (depth % 6U)
        {
            case 0U:
                declaration->localOrdinal = 7U;
                initializer->localOrdinal = depth;
                break;
            case 1U:
                declaration->localOrdinal = 7U;
                initializer->localOrdinal = 100000U + depth;
                break;
            case 2U: declaration->localOrdinal = 8U; break;
            case 3U:
                declaration->localOrdinal = 9U;
                initializer->kind = ProjectOopMirNodeKind::LocalValue;
                break;
            case 4U: initializer->localOrdinal = depth; break;
            default:
                declaration->localOrdinal = 10U;
                initializer.reset();
                break;
        }
        declaration->children.push_back(std::move(initializer));
        if (depth % 6U == 3U)
        {
            auto ignoredBorrow = std::make_unique<ProjectOopMirNode>();
            ignoredBorrow->kind = ProjectOopMirNodeKind::BorrowValue;
            ignoredBorrow->localOrdinal = 99U;
            declaration->children.push_back(std::move(ignoredBorrow));
        }
        if (root != nullptr) declaration->children.push_back(std::move(root));
        declaration->children.emplace_back();
        root = std::move(declaration);
    }
    return root;
}

void testDeepMirBorrowSourceCollection(TestContext& tests)
{
    std::unique_ptr<ProjectOopMirNode> root = makeDeepMirBorrowSourceTree();
    std::unordered_map<std::size_t, std::size_t> borrowSource;
    std::unordered_set<std::size_t> borrowNonLocalSource;
    nativesharp::frontend::ProjectOopHirModelTestAccess::collectMirBorrowSources(
        root.get(), borrowSource, borrowNonLocalSource);
    tests.expect(borrowSource.size() == 1U && borrowSource.at(7U) == 132767U,
        "MIR borrow source collection preserves preorder overwrite by local ordinal");
    tests.expect(borrowNonLocalSource.size() == 1U && borrowNonLocalSource.contains(8U) &&
            !borrowSource.contains(9U) && !borrowSource.contains(10U),
        "MIR borrow source collection preserves non-local, first-initializer, and null-slot rules");
    destroyMirTreeIteratively(std::move(root));
}

void testDeepMirLivePollLocalCollection(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        node->kind = ProjectOopMirNodeKind::LocalDeclaration;
        switch (depth % 5U)
        {
            case 0U: node->localOrdinal = 7U; break;
            case 1U: node->localOrdinal = 8U; break;
            case 2U: node->localOrdinal = 9U; break;
            case 3U: node->localOrdinal = 10U; break;
            default: break;
        }
        if (root != nullptr) node->children.push_back(std::move(root));
        node->children.emplace_back();
        root = std::move(node);
    }
    const std::unordered_map<std::size_t, std::uint64_t> localDefinition{
        {7U, 0U}, {8U, 5U}, {9U, 0U}, {10U, 0U}};
    const std::unordered_map<std::size_t, std::uint64_t> localLastUse{
        {7U, 10U}, {8U, 10U}, {10U, 4U}};
    const std::vector<std::uint64_t> suspendOrders{5U};
    std::unordered_set<std::size_t> liveSet{42U};
    nativesharp::frontend::ProjectOopHirModelTestAccess::collectMirLivePollLocals(
        root.get(), localDefinition, localLastUse, suspendOrders, liveSet);
    tests.expect(liveSet.size() == 2U && liveSet.contains(7U) && liveSet.contains(42U),
        "MIR poll local collection preserves strict boundary liveness and existing entries");
    tests.expect(!liveSet.contains(8U) && !liveSet.contains(9U) && !liveSet.contains(10U),
        "MIR poll local collection preserves missing-use, equality, deduplication, and null rules");
    destroyMirTreeIteratively(std::move(root));
}

void testDeepMirPollFrameLocalAppend(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    std::unordered_set<std::size_t> liveSet;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        node->kind = ProjectOopMirNodeKind::LocalDeclaration;
        if (depth % 6U != 5U) node->localOrdinal = depth;
        if (depth % 6U == 0U) node->typeKey = "builtin:byte";
        else if (depth % 6U == 1U || depth % 6U == 3U) node->typeKey = "builtin:int";
        else if (depth % 6U == 4U) node->typeKey = "type:Missing";
        if (depth % 6U == 0U || depth % 6U == 2U ||
            depth % 6U == 3U || depth % 6U == 4U)
            liveSet.insert(depth);
        if (root != nullptr) node->children.push_back(std::move(root));
        node->children.emplace_back();
        root = std::move(node);
    }
    auto lateral = std::make_unique<ProjectOopMirNode>();
    lateral->kind = ProjectOopMirNodeKind::LocalDeclaration;
    lateral->localOrdinal = 0U;
    lateral->typeKey = "builtin:long";
    root->children.push_back(std::move(lateral));
    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    nativesharp::frontend::ProjectOopMirFunction function;
    function.asyncFrameLocals.emplace_back(999U, 1U, "sentinel:existing");
    std::uint64_t frameSize = 3U;
    nativesharp::frontend::ProjectOopHirModelTestAccess::appendMirPollFrameLocals(
        root.get(), liveSet, objectLayoutModel, nameLookup, function, frameSize);
    tests.expect(function.asyncFrameLocals.size() == 10925U &&
            function.asyncFrameLocals[0] == std::make_tuple(999U, 1U, std::string("sentinel:existing")) &&
            function.asyncFrameLocals[1] == std::make_tuple(0U, 3U, std::string("builtin:byte")) &&
            function.asyncFrameLocals[2] == std::make_tuple(3U, 4U, std::string("builtin:int")),
        "MIR poll frame locals preserve existing slots, preorder, liveness, and alignment");
    tests.expect(function.asyncFrameLocals[10923] ==
            std::make_tuple(32766U, 43688U, std::string("builtin:byte")) &&
            function.asyncFrameLocals.back() ==
                std::make_tuple(0U, 43696U, std::string("builtin:long")) &&
            frameSize == 43704U,
        "MIR poll frame locals preserve missing layouts, repeats, sibling order, and final size");
    destroyMirTreeIteratively(std::move(root));
}

void testDeepMirMoveNextFrameLocalAppend(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    std::unordered_map<std::size_t, std::uint64_t> localDefinition;
    std::unordered_map<std::size_t, std::uint64_t> localLastUse;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        node->kind = ProjectOopMirNodeKind::LocalDeclaration;
        switch (depth % 8U)
        {
        case 0U: node->localOrdinal = depth; node->typeKey = "builtin:byte"; break;
        case 1U: node->localOrdinal = depth; node->typeKey = "builtin:int"; break;
        case 2U: node->localOrdinal = depth; node->cleanupRegionOrdinal = depth; break;
        case 3U: node->localOrdinal = depth; node->typeKey = "builtin:int";
            node->cleanupRegionOrdinal = depth; break;
        case 4U: node->localOrdinal = depth; node->typeKey = "type:Missing";
            node->cleanupRegionOrdinal = depth; break;
        case 5U: node->typeKey = "builtin:byte"; node->cleanupRegionOrdinal = depth; break;
        case 6U: node->localOrdinal = depth; node->typeKey = "builtin:long"; break;
        default: node->localOrdinal = depth; node->typeKey = "builtin:short"; break;
        }
        if (depth % 8U == 0U || depth % 8U == 6U)
        {
            localDefinition.emplace(depth, 0U);
            localLastUse.emplace(depth, 10U);
        }
        if (root != nullptr) node->children.push_back(std::move(root));
        node->children.emplace_back();
        root = std::move(node);
    }
    auto lateral = std::make_unique<ProjectOopMirNode>();
    lateral->kind = ProjectOopMirNodeKind::LocalDeclaration;
    lateral->localOrdinal = 0U;
    lateral->typeKey = "builtin:long";
    lateral->cleanupRegionOrdinal = 0U;
    root->children.push_back(std::move(lateral));
    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    nativesharp::frontend::ProjectOopMirFunction function;
    function.asyncFrameLocals.emplace_back(999U, 1U, "sentinel:existing");
    std::uint64_t frameSize = 3U;
    const std::array<std::uint64_t, 1U> yieldOrders{5U};
    nativesharp::frontend::ProjectOopHirModelTestAccess::appendMirMoveNextFrameLocals(
        root.get(), localDefinition, localLastUse, yieldOrders,
        objectLayoutModel, nameLookup, function, frameSize);
    tests.expect(function.asyncFrameLocals.size() == 12290U &&
            function.asyncFrameLocals[0] == std::make_tuple(999U, 1U, std::string("sentinel:existing")) &&
            function.asyncFrameLocals[1] == std::make_tuple(0U, 3U, std::string("builtin:byte")) &&
            function.asyncFrameLocals[2] == std::make_tuple(3U, 4U, std::string("builtin:int")) &&
            function.asyncFrameLocals[3] == std::make_tuple(6U, 8U, std::string("builtin:long")),
        "MIR move-next frame locals preserve existing slots, preorder, liveness, cleanup, and alignment");
    tests.expect(function.asyncFrameLocals[12288] ==
            std::make_tuple(32766U, 65528U, std::string("builtin:long")) &&
            function.asyncFrameLocals.back() ==
                std::make_tuple(0U, 65536U, std::string("builtin:long")) &&
            frameSize == 65544U,
        "MIR move-next frame locals preserve omissions, repeats, sibling order, and final size");
    destroyMirTreeIteratively(std::move(root));
}

void testDeepMirCacheRestore(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root;
    for (std::size_t depth = nestingDepth; depth-- > 0U;)
    {
        auto node = std::make_unique<ProjectOopMirNode>();
        node->kind = ProjectOopMirNodeKind::LocalDeclaration;
        node->typeKey = "type:Node" + std::to_string(depth);
        node->integerValue = depth;
        if (depth % 2U == 0U) node->localOrdinal = depth;
        if (root != nullptr) node->children.push_back(std::move(root));
        else node->children.emplace_back();
        node->children.emplace_back();
        root = std::move(node);
    }
    const std::string payload =
        nativesharp::frontend::ProjectOopHirModelTestAccess::serializeMirNodeTree(root.get());
    auto restored = std::make_unique<ProjectOopMirNode>();
    restored->typeKey = "sentinel:replace";
    const bool restoredOk =
        nativesharp::frontend::ProjectOopHirModelTestAccess::restoreMirNodeTree(payload, restored);
    bool exact = restoredOk;
    ProjectOopMirNode* current = restored.get();
    for (std::size_t depth = 0U; exact && depth < nestingDepth; ++depth)
    {
        exact = current != nullptr && current->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            current->typeKey == "type:Node" + std::to_string(depth) &&
            current->integerValue == depth && current->localOrdinal.has_value() == (depth % 2U == 0U) &&
            (!current->localOrdinal.has_value() || *current->localOrdinal == depth) &&
            current->children.size() == 2U && current->children[1] == nullptr;
        if (exact) current = current->children[0].get();
    }
    tests.expect(exact && current == nullptr,
        "MIR cache restore preserves 32768-node preorder, fields, child counts, and null slots");
    restored->typeKey = "mutated:restored";
    tests.expect(root->typeKey == "type:Node0",
        "MIR cache restore owns an independent node tree");
    auto nullBody = std::make_unique<ProjectOopMirNode>();
    const bool nullOk = nativesharp::frontend::ProjectOopHirModelTestAccess::restoreMirNodeTree(
        nativesharp::frontend::ProjectOopHirModelTestAccess::serializeMirNodeTree(nullptr), nullBody);
    std::string invalidMarker(4U, '\0');
    auto invalidBody = std::make_unique<ProjectOopMirNode>();
    invalidBody->typeKey = "sentinel:invalid";
    const bool invalidOk =
        nativesharp::frontend::ProjectOopHirModelTestAccess::restoreMirNodeTree(
            invalidMarker, invalidBody);
    std::string truncated = payload;
    truncated.resize(truncated.size() - 5U);
    auto truncatedBody = std::make_unique<ProjectOopMirNode>();
    truncatedBody->typeKey = "sentinel:truncated";
    const bool truncatedOk =
        nativesharp::frontend::ProjectOopHirModelTestAccess::restoreMirNodeTree(
            truncated, truncatedBody);
    tests.expect(nullOk && nullBody == nullptr && !invalidOk &&
            invalidBody->typeKey == "sentinel:invalid" && !truncatedOk &&
            truncatedBody->typeKey == "sentinel:truncated",
        "MIR cache restore preserves null-root and transactional read-error semantics");
    destroyMirTreeIteratively(std::move(root));
    destroyMirTreeIteratively(std::move(restored));
}

std::unique_ptr<ProjectOopMirNode> makeDeepMirBlockTree(std::size_t edgeDepth)
{
    auto root = std::make_unique<ProjectOopMirNode>();
    root->kind = ProjectOopMirNodeKind::Block;
    ProjectOopMirNode* current = root.get();
    for (std::size_t depth = 0; depth < edgeDepth; ++depth)
    {
        auto child = std::make_unique<ProjectOopMirNode>();
        child->kind = ProjectOopMirNodeKind::Block;
        current->children.push_back(std::move(child));
        current = current->children.back().get();
    }
    root->children.emplace_back();
    return root;
}

void testMirDumperFormat(TestContext& tests)
{
    auto root = std::make_unique<ProjectOopMirNode>();
    root->kind = ProjectOopMirNodeKind::Block;
    root->typeKey = "builtin:void";
    root->memberOrdinal = 3U;
    root->mirRegionOrdinal = 5U;
    root->cleanupRegionOrdinals = {8U, 13U};
    auto first = std::make_unique<ProjectOopMirNode>();
    first->kind = ProjectOopMirNodeKind::Block;
    first->typeKey = "builtin:int";
    first->localOrdinal = 7U;
    first->children.emplace_back();
    root->children.push_back(std::move(first));
    root->children.emplace_back();
    auto last = std::make_unique<ProjectOopMirNode>();
    last->kind = ProjectOopMirNodeKind::Block;
    last->typeKey = "builtin:bool";
    root->children.push_back(std::move(last));
    const std::string dump =
        nativesharp::frontend::ProjectOopHirModelTestAccess::dumpMirNode(root.get(), 1);
    const std::string expected =
        "  block type=builtin:void member=#3 mir-region=#5 cleanup-path=#8->#13\n"
        "    block type=builtin:int local=#7\n"
        "      <none>\n"
        "    <none>\n"
        "    block type=builtin:bool\n";
    tests.expect(dump == expected,
        "MIR dumping preserves byte-exact fields, indentation, preorder, and null slots");
}

void testDeepMirDumper(TestContext& tests)
{
    constexpr std::size_t traversalDepth = 32768U;
    auto traversalRoot = makeDeepMirBlockTree(traversalDepth);
    std::size_t visitedNodes = 0;
    const std::size_t maximumDepth =
        nativesharp::frontend::ProjectOopHirModelTestAccess::mirNodeTraversalDepth(
            traversalRoot.get(), visitedNodes);
    tests.expect(maximumDepth == traversalDepth && visitedNodes == traversalDepth + 2U,
        "MIR dumper traversal reaches 32768 levels and preserves the null sibling without recursion");
    destroyMirTreeIteratively(std::move(traversalRoot));

    constexpr std::size_t dumpDepth = 8192U;
    auto dumpRoot = makeDeepMirBlockTree(dumpDepth);
    const std::string dump =
        nativesharp::frontend::ProjectOopHirModelTestAccess::dumpMirNode(dumpRoot.get(), 0);
    tests.expect(static_cast<std::size_t>(std::count(dump.begin(), dump.end(), '\n')) ==
            dumpDepth + 2U && dump.starts_with("block\n  block\n") &&
            dump.ends_with("  <none>\n"),
        "an 8192-level MIR tree emits the proportional full text in exact preorder");
    destroyMirTreeIteratively(std::move(dumpRoot));
}

void testDeepMirNodeDestruction(TestContext& tests)
{
    constexpr std::size_t depth = 32768U;
    std::unique_ptr<ProjectOopMirNode> root = makeDeepMirBlockTree(depth);
    root.reset();
    tests.expect(root == nullptr,
        "a directly owned 32768-level MIR tree is destroyed without process recursion");

    nativesharp::frontend::ProjectOopMirFunction function;
    function.body = makeDeepMirBlockTree(depth);
    function.body.reset();
    tests.expect(function.body == nullptr,
        "a function body destroys its 32768-level MIR ownership tree iteratively");

    nativesharp::frontend::ProjectOopMirFieldInitializer initializer;
    initializer.value = makeDeepMirBlockTree(depth);
    initializer.value.reset();
    tests.expect(initializer.value == nullptr,
        "a field initializer destroys its 32768-level MIR ownership tree iteratively");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_project_oop_lowering_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    char* focus = nullptr;
    std::size_t focusLength = 0;
    const int focusError = _dupenv_s(
        &focus, &focusLength, "NSC_PROJECT_OOP_LOWERING_FOCUS");
    const std::string focusValue = focusError == 0 && focus != nullptr
        ? std::string(focus)
        : std::string{};
    std::free(focus);
    if (focusValue == "precise-incremental-invalidation")
    {
        testPreciseIncrementalInvalidation(tests, root);
    }
    else if (focusValue == "hir-default-argument-depth")
    {
        testDeepOptionalDefaultHir(tests, root);
    }
    else if (focusValue == "hir-async-yield-state-depth")
    {
        testDeepYieldStateAssignment(tests);
    }
    else if (focusValue == "hir-generator-throw-depth")
    {
        testDeepGeneratorThrowRewrite(tests);
    }
    else if (focusValue == "hir-async-poll-return-depth")
    {
        testDeepAsyncPollReturnRewrite(tests);
    }
    else if (focusValue == "hir-async-await-scan-depth")
    {
        testDeepAsyncAwaitScan(tests);
    }
    else if (focusValue == "hir-builtin-unary-depth")
    {
        testDeepBuiltinUnaryExpressionLowering(tests);
    }
    else if (focusValue == "hir-user-unary-operator-depth")
    {
        testDeepUserUnaryOperatorLowering(tests, root);
    }
    else if (focusValue == "hir-user-binary-operator-depth")
    {
        testDeepUserBinaryOperatorLowering(tests, root);
    }
    else if (focusValue == "hir-deconstruction-assignment-depth")
    {
        testDeepDeconstructionAssignmentLowering(tests);
    }
    else if (focusValue == "hir-event-add-remove-depth")
    {
        testDeepEventSubscriptionLowering(tests, root);
    }
    else if (focusValue == "hir-property-indexer-setter-depth")
    {
        testDeepPropertyIndexerSetterLowering(tests, root);
    }
    else if (focusValue == "hir-conversion-depth")
    {
        testDeepConversionExpressionLowering(tests);
    }
    else if (focusValue == "hir-class-field-depth")
    {
        testDeepClassFieldExpressionLowering(tests);
    }
    else if (focusValue == "hir-property-getter-depth")
    {
        testDeepPropertyGetterExpressionLowering(tests, root);
    }
    else if (focusValue == "hir-extension-property-getter-depth")
    {
        testDeepExtensionPropertyGetterLowering(tests, root);
    }
    else if (focusValue == "hir-extension-property-setter-depth")
    {
        testDeepExtensionPropertySetterLowering(tests, root);
    }
    else if (focusValue == "hir-property-core-array-assignment-depth")
    {
        testDeepNullConditionalAssignmentLowering(tests, root);
    }
    else if (focusValue == "hir-user-indexer-getter-depth")
    {
        testDeepUserIndexerGetterLowering(tests, root);
    }
    else if (focusValue == "hir-interpolation-handler-depth")
    {
        testDeepInterpolationHandlerLowering(tests, root);
    }
    else if (focusValue == "hir-overflow-policy-depth")
    {
        testDeepOverflowPolicyExpressionLowering(tests);
    }
    else if (focusValue == "hir-builtin-binary-depth")
    {
        testDeepBuiltinBinaryExpressionLowering(tests);
    }
    else if (focusValue == "hir-conditional-depth")
    {
        testDeepConditionalExpressionLowering(tests);
    }
    else if (focusValue == "hir-match-expression-depth")
    {
        testDeepMatchExpressionLowering(tests);
        testMatchExpressionContract(tests);
        testReferenceMatchExpressionLowering(tests);
    }
    else if (focusValue == "hir-mixed-depth")
    {
        testDeepMixedExpressionLowering(tests);
    }
    else if (focusValue == "hir-static-assert-depth")
    {
        testDeepStaticAssertEvaluation(tests);
    }
    else if (focusValue == "hir-static-assert-short-circuit")
    {
        testStaticAssertShortCircuitEvaluation(tests);
    }
    else if (focusValue == "hir-dump-depth")
    {
        testDeepHirDump(tests);
    }
    else if (focusValue.starts_with("hir-coordinator-walker-"))
    {
        testDeepHirCoordinatorWalker(
            tests, focusValue.substr(std::string_view("hir-coordinator-walker-").size()));
    }
    else if (focusValue == "hir-model-destruction-depth")
    {
        testDeepHirModelDestruction(tests);
    }
    else if (focusValue == "hir-node-destruction-depth")
    {
        testDeepHirNodeDestruction(tests);
    }
    else if (focusValue == "hir-block-depth")
    {
        testDeepHirBlockLowering(tests);
    }
    else if (focusValue == "hir-deconstruction-walk-depth")
    {
        testDeepHirDeconstructionWalk(tests);
    }
    else if (focusValue == "hir-aggregate-leaves-depth")
    {
        testDeepAggregateLeaves(tests);
    }
    else if (focusValue == "hir-aggregate-elements-depth")
    {
        testDeepAggregateElements(tests);
    }
    else if (focusValue == "mir-node-lowering-depth")
    {
        testDeepMirNodeLowering(tests);
        testMirAggregateSpreadDescriptor(tests);
    }
    else if (focusValue == "mir-future-layout-depth")
    {
        testDeepMirFutureKeyDiscovery(tests);
        testMirFutureLayoutPublication(tests);
    }
    else if (focusValue == "mir-iterator-layout-depth")
    {
        testDeepMirIteratorLayoutDiscovery(tests);
    }
    else if (focusValue == "mir-string-constant-depth")
    {
        testDeepMirStringConstantEvaluation(tests);
    }
    else if (focusValue == "mir-integer-constant-depth")
    {
        testDeepMirIntegerConstantEvaluation(tests);
    }
    else if (focusValue == "mir-floating-constant-depth")
    {
        testDeepMirFloatingConstantEvaluation(tests);
    }
    else if (focusValue == "mir-boolean-constant-depth")
    {
        testDeepMirBooleanConstantEvaluation(tests);
    }
    else if (focusValue == "mir-cleanup-contract-depth")
    {
        testDeepMirCleanupContractCollection(tests);
    }
    else if (focusValue == "mir-cleanup-reference-depth")
    {
        testDeepMirCleanupReferenceValidation(tests);
    }
    else if (focusValue == "mir-awaited-frame-size-depth")
    {
        testDeepMirAwaitedFrameSize(tests);
    }
    else if (focusValue == "mir-poll-liveness-depth")
    {
        testDeepMirPollLiveness(tests);
    }
    else if (focusValue == "mir-move-next-liveness-depth")
    {
        testDeepMirMoveNextLiveness(tests);
    }
    else if (focusValue == "mir-borrow-source-depth")
    {
        testDeepMirBorrowSourceCollection(tests);
    }
    else if (focusValue == "mir-live-poll-local-depth")
    {
        testDeepMirLivePollLocalCollection(tests);
    }
    else if (focusValue == "mir-poll-frame-local-depth")
    {
        testDeepMirPollFrameLocalAppend(tests);
    }
    else if (focusValue == "mir-move-next-frame-local-depth")
    {
        testDeepMirMoveNextFrameLocalAppend(tests);
    }
    else if (focusValue == "mir-cache-restore-depth")
    {
        testDeepMirCacheRestore(tests);
    }
    else if (focusValue == "mir-dump-depth")
    {
        testMirDumperFormat(tests);
        testDeepMirDumper(tests);
    }
    else if (focusValue == "mir-node-destruction-depth")
    {
        testDeepMirNodeDestruction(tests);
    }
    else if (focusValue == "llvm-imported-constant-reachability")
    {
        testImportedConstantCoordinatorReachability(tests);
    }
    else
    {
        testEndToEndOopLowering(tests, root);
        testDeallocationContract(tests, root);
        testAggregateCallableBoundary(tests, root);
        testNarrowSignedEnumNameLowering(tests, root);
        testPreciseIncrementalInvalidation(tests, root);
        testPartitionStaticFieldReachability(tests, root);
        testImportedConstantCoordinatorReachability(tests);
        testDeepOptionalDefaultHir(tests, root);
        testDeepYieldStateAssignment(tests);
        testDeepGeneratorThrowRewrite(tests);
        testDeepAsyncPollReturnRewrite(tests);
        testDeepAsyncAwaitScan(tests);
        testDeepBuiltinUnaryExpressionLowering(tests);
        testDeepUserUnaryOperatorLowering(tests, root);
        testDeepUserBinaryOperatorLowering(tests, root);
        testDeepEventSubscriptionLowering(tests, root);
        testDeepPropertyIndexerSetterLowering(tests, root);
        testDeepClassFieldExpressionLowering(tests);
        testDeepPropertyGetterExpressionLowering(tests, root);
        testDeepExtensionPropertyGetterLowering(tests, root);
        testDeepExtensionPropertySetterLowering(tests, root);
        testDeepNullConditionalAssignmentLowering(tests, root);
        testDeepUserIndexerGetterLowering(tests, root);
        testDeepInterpolationHandlerLowering(tests, root);
        testDeepOverflowPolicyExpressionLowering(tests);
        testDeepBuiltinBinaryExpressionLowering(tests);
        testDeepConditionalExpressionLowering(tests);
        testDeepMatchExpressionLowering(tests);
        testMatchExpressionContract(tests);
        testReferenceMatchExpressionLowering(tests);
        testDeepMixedExpressionLowering(tests);
        testDeepStaticAssertEvaluation(tests);
        testStaticAssertShortCircuitEvaluation(tests);
        testDeepMirNodeLowering(tests);
        testMirAggregateSpreadDescriptor(tests);
        testDeepMirFutureKeyDiscovery(tests);
        testMirFutureLayoutPublication(tests);
        testDeepMirIteratorLayoutDiscovery(tests);
        testDeepMirStringConstantEvaluation(tests);
        testDeepMirIntegerConstantEvaluation(tests);
        testDeepMirFloatingConstantEvaluation(tests);
        testDeepMirBooleanConstantEvaluation(tests);
        testDeepMirCleanupContractCollection(tests);
        testDeepMirCleanupReferenceValidation(tests);
        testDeepMirAwaitedFrameSize(tests);
        testDeepMirPollLiveness(tests);
        testDeepMirMoveNextLiveness(tests);
        testDeepMirBorrowSourceCollection(tests);
        testDeepMirLivePollLocalCollection(tests);
        testDeepMirPollFrameLocalAppend(tests);
        testDeepMirMoveNextFrameLocalAppend(tests);
        testDeepMirCacheRestore(tests);
        testMirDumperFormat(tests);
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
