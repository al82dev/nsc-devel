#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <sstream>

namespace nativesharp::frontend
{
namespace
{
void dumpNode(std::ostringstream& output, const ProjectOopHirNode* node, int depth)
{
    if (node == nullptr)
    {
        output << std::string(static_cast<std::size_t>(depth * 2), ' ') << "<none>\n";
        return;
    }
    output << std::string(static_cast<std::size_t>(depth * 2), ' ')
           << projectOopHirNodeKindName(node->kind);
    if (!node->typeKey.empty()) output << " type=" << node->typeKey;
    if (node->memberOrdinal.has_value()) output << " member=#" << *node->memberOrdinal;
    if (node->namespaceFunctionOrdinal.has_value())
        output << " namespace-function=#" << *node->namespaceFunctionOrdinal;
    if (node->namespaceStateOrdinal.has_value())
        output << " namespace-state=#" << *node->namespaceStateOrdinal;
    if (node->localOrdinal.has_value()) output << " local=#" << *node->localOrdinal;
    if (node->hirRegionOrdinal.has_value()) output << " hir-region=#" << *node->hirRegionOrdinal;
    if (node->exceptionTargetRegionOrdinal.has_value()) output << " exception-target=#" << *node->exceptionTargetRegionOrdinal;
    if (node->cleanupRegionOrdinal.has_value()) output << " cleanup=#" << *node->cleanupRegionOrdinal;
    if (node->targetScopeOrdinal.has_value()) output << " target-scope=#" << *node->targetScopeOrdinal;
    if (node->exceptionTypeOrdinal.has_value()) output << " exception-type=#" << *node->exceptionTypeOrdinal;
    if (node->catchAll) output << " catch=all";
    if (node->disposeContractMemberOrdinal.has_value()) output << " dispose-contract=#" << *node->disposeContractMemberOrdinal;
    if (node->disposeImplementationMemberOrdinal.has_value()) output << " dispose-implementation=#" << *node->disposeImplementationMemberOrdinal;
    if (node->interfaceCleanupDispatch) output << " cleanup-dispatch=interface";
    if (!node->cleanupRegionOrdinals.empty())
    {
        output << " cleanup-path=";
        for (std::size_t index = 0; index < node->cleanupRegionOrdinals.size(); ++index)
        {
            if (index != 0) output << "->";
            output << '#' << node->cleanupRegionOrdinals[index];
        }
    }
    if (node->cleanupContinuationKind.has_value())
        output << " continuation=" << methodCleanupContinuationKindName(*node->cleanupContinuationKind);
    if (node->borrowKind.has_value())
        output << " borrow=" << methodBorrowKindName(*node->borrowKind);
    if (node->localBorrowSourceKind.has_value())
        output << " borrow-source=" << methodLocalBorrowSourceKindName(*node->localBorrowSourceKind);
    if (node->parameterOrdinal.has_value()) output << " param=#" << *node->parameterOrdinal;
    if (node->constructionOrdinal.has_value()) output << " construction=#" << *node->constructionOrdinal;
    const bool runtimeTypeNode =
        node->kind == ProjectOopHirNodeKind::RuntimeTypeTest ||
        node->kind == ProjectOopHirNodeKind::SafeReferenceCast ||
        node->kind == ProjectOopHirNodeKind::CheckedReferenceCast;
    if (runtimeTypeNode && node->targetTypeOrdinal.has_value())
        output << " target-type=#" << *node->targetTypeOrdinal
               << " target-kind=" << (node->targetIsInterface ? "interface" : "class");
    if (node->operation != lexer::TokenKind::Invalid) output << " op=" << lexer::tokenKindName(node->operation);
    if (node->conversion.has_value()) output << " conversion=" << projectConversionKindName(node->conversion->kind);
    if (node->kind == ProjectOopHirNodeKind::IntegerConstant) output << " value=" << node->integerValue;
    if (node->kind == ProjectOopHirNodeKind::FloatingConstant) output << " value=" << node->floatingValue;
    if (node->kind == ProjectOopHirNodeKind::BooleanConstant) output << " value=" << (node->booleanValue ? "true" : "false");
    if (node->referenceLocal) output << " reference-local=true";
    if (node->pinnedStorage) output << " pinned=true";
    if (node->referenceResult) output << " reference-result=true";
    if (node->nullConditionalCall) output << " null-conditional=true";
    if (node->stackPromotedObject) output << " stack-promoted=true";
    if (node->volatileAccess) output << " volatile=true";
    if (!node->elementTypeKey.empty()) output << " element=" << node->elementTypeKey;
    if ((node->kind == ProjectOopHirNodeKind::ArrayElementValue ||
         node->kind == ProjectOopHirNodeKind::ArrayElementAddress ||
         node->kind == ProjectOopHirNodeKind::StringElementValue) && node->booleanValue)
        output << " index=from-end";
    if (node->nativeArrayRank > 1) output << " rank=" << node->nativeArrayRank;
    if (node->kind == ProjectOopHirNodeKind::ParameterPack)
        output << " elements=" << node->children.size() << " storage=stack";
    if (node->kind == ProjectOopHirNodeKind::AggregateSpread)
        output << " targets=" << (node->children.empty() ? 0U : node->children.size() - 1U)
               << " source-evaluations=1";
    if (node->kind == ProjectOopHirNodeKind::AggregateElement)
        output << " offset=" << node->integerValue;
    if (node->kind == ProjectOopHirNodeKind::StringLiteral) output << " bytes=" << node->stringValue.size();
    if (node->kind == ProjectOopHirNodeKind::InlineAssembly)
        output << " source-bytes=" << node->stringValue.size();
    if (node->kind == ProjectOopHirNodeKind::InlineAssemblyLine)
        output << " text=\"" << node->stringValue << "\" contract=" << node->elementTypeKey;
    if (node->kind == ProjectOopHirNodeKind::InlineAssemblyInput ||
        node->kind == ProjectOopHirNodeKind::InlineAssemblyOutput)
        output << " ordinal=" << node->integerValue << " constraint=" << node->stringValue
               << " explicit=" << (node->booleanValue ? "true" : "false");
    if (node->kind == ProjectOopHirNodeKind::InlineAssemblyClobber)
        output << " ordinal=" << node->integerValue << " name=" << node->stringValue
               << " category=" << node->elementTypeKey
               << " preserve=" << (node->booleanValue ? "true" : "false");
    if (node->kind == ProjectOopHirNodeKind::Deallocation)
        output << " destruction=" << (node->dynamicDestruction ? "dynamic-descriptor" : "direct-free");
    if (node->kind == ProjectOopHirNodeKind::ObjectDestruction)
        output << " destruction=" << (node->dynamicDestruction ? "dynamic-object-only" : "trivial-object");
    if (node->objectLifetimeOwnership != sema::OwnershipKind::None ||
        node->storageOwnership != sema::OwnershipKind::None)
        output << " object-ownership=" << sema::ownershipKindName(node->objectLifetimeOwnership)
               << " storage-ownership=" << sema::ownershipKindName(node->storageOwnership);
    output << '\n';
    for (const auto& child : node->children)
    {
        dumpNode(output, child.get(), depth + 1);
    }
}
}

std::string ProjectOopHirModelDumper::dump(const ProjectOopHirModel& model)
{
    std::ostringstream output;
    output << "project.oop.hir functions=" << model.functions().size() << '\n';
    for (const ProjectOopHirFunction& function : model.functions())
    {
        output << "function #" << function.memberOrdinal;
        if (function.namespaceFunctionOrdinal.has_value())
            output << " namespace-function=#" << *function.namespaceFunctionOrdinal;
        output << ' ' << function.mangledName
               << " abi=" << (function.isStatic ? "static" : "this:first")
               << " inlining=" << inliningPolicyName(function.inliningPolicy)
               << " noheap=" << (function.isNoHeap ? "true" : "false")
               << " effects=" << function.effects.bits()
               << (function.isExportedC ? " export-c=" + function.cAbiName : "")
               << (function.isVariadic ? " variadic" : "")
               << (function.isImportedNative ? " import-native" : "")
               << " return=" << function.returnTypeKey
               << " ref-return=" << (function.referenceReturn ? "true" : "false")
               << " return-ownership=" << sema::ownershipKindName(function.returnOwnership)
               << " params=" << function.parameterTypeKeys.size()
               << " locals=" << function.localOrdinals.size()
               << (function.constructor ? " constructor" : function.destructor ? " destructor" : " method");
        if (!function.genericSpecializationKey.empty())
            output << " generic-specialization=" << function.genericSpecializationKey;
        output << '\n';
        for (std::size_t index = 0; index < function.parameterOwnerships.size(); ++index)
        {
            if (index < function.parameterPassKinds.size())
            {
                output << "  parameter-pass[" << index << "]="
                       << parameterPassKindName(function.parameterPassKinds[index]) << '\n';
            }
            output << "  parameter-ownership[" << index << "]="
                   << sema::ownershipKindName(function.parameterOwnerships[index]) << '\n';
        }
        dumpNode(output, function.body.get(), 1);
    }
    output << "field-initializers=" << model.fieldInitializers().size() << '\n';
    for (const ProjectOopHirFieldInitializer& initializer : model.fieldInitializers())
    {
        output << "field-initializer member=#" << initializer.memberOrdinal
               << " type=#" << initializer.declaringTypeOrdinal
               << " static=" << (initializer.isStatic ? "true" : "false")
               << " const=" << (initializer.isConst ? "true" : "false") << '\n';
        dumpNode(output, initializer.value.get(), 1);
    }
    return output.str();
}
}
