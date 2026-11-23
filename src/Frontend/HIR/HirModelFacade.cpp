#include "HirLoweringSupport.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"

// Public model facade extracted by H01B1. The build implementation lives in
// HirBuildCoordinator.cpp; lowering components are split by the remaining H01 slices.

namespace nativesharp::frontend
{
ProjectOopHirModel::ProjectOopHirModel(ProjectCompilationIndexes& indexes) noexcept
    : indexes_(&indexes)
{
}

void ProjectOopHirModel::setCppVariadicCallPlanProvider(CppVariadicCallPlanProvider provider)
{
    cppVariadicCallPlanProvider_ = std::move(provider);
}

void ProjectOopHirModel::setCppFunctionPointerPlanProvider(CppFunctionPointerPlanProvider provider)
{
    cppFunctionPointerPlanProvider_ = std::move(provider);
}

void ProjectOopHirModel::setCppVariadicFunctionPointerPlanProvider(CppVariadicFunctionPointerPlanProvider provider)
{
    cppVariadicFunctionPointerPlanProvider_ = std::move(provider);
}

bool ProjectOopHirModel::attachSysVCallbackPlans(
    const std::function<std::shared_ptr<const interop::abi_plan::Plan>(std::string_view)>& planFor,
    std::string& error)
{
    const auto scalar = [](std::string_view key) {
        return key == "builtin:int" || key == "builtin:uint" || key == "builtin:long" || key == "builtin:ulong" ||
            key == "builtin:nint" || key == "builtin:nuint" || key == "builtin:float" || key == "builtin:double" ||
            key == "builtin:bool" || key.ends_with('*') ||
            key == "builtin:i128" || key == "builtin:u128"; // C5-07: LLVM's direct `i128`, as Clang
    };
    // A callback of the convention whose values LLVM alone does not classify as Clang does. / Un callback de la
    // convencion cuyos valores LLVM no clasifica solo como Clang.
    const auto planned = [&](std::string_view key) {
        const auto function = language::parseFunctionTypeIdentity(key);
        if (!function || function->callingConvention != language::CallingConvention::SysV ||
            function->carrier != language::FunctionCarrier::Pointer || function->variadic) return false;
        return !(function->result == "builtin:void" || scalar(function->result)) ||
            !std::all_of(function->parameters.begin(), function->parameters.end(),
                [&](const std::string& parameter) { return scalar(parameter); });
    };
    bool ok = true;
    const auto visit = [&](ProjectOopHirNode* root) {
        std::vector<ProjectOopHirNode*> pending{root};
        while (ok && !pending.empty())
        {
            ProjectOopHirNode* node = pending.back();
            pending.pop_back();
            if (node == nullptr) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            const bool callback = node->kind == ProjectOopHirNodeKind::LambdaCreate ||
                node->kind == ProjectOopHirNodeKind::DelegateCreate ||
                std::any_of(node->children.begin(), node->children.end(), [](const auto& child) {
                    return child && language::isAnonymousLambdaTypeIdentity(child->typeKey); });
            if (!callback || node->cppFunctionPointerPlan || !planned(node->typeKey)) continue;
            node->cppFunctionPointerPlan = planFor(node->typeKey);
            // A C function keeps its own address and needs no plan. / Una funcion C conserva su direccion.
            if (!node->cppFunctionPointerPlan && node->kind != ProjectOopHirNodeKind::DelegateCreate)
            {
                error = "conversion of a NativeSharp callback to the sysv_abi function pointer '" + node->typeKey +
                    "' needs the Clang plan of a C typedef naming that pointer type (a precompiled template body names "
                    "the typedef, as in `T f = lambda;`, so its catalog carries it)";
                ok = false;
            }
        }
    };
    for (ProjectOopHirFunction& function : functions_) visit(function.body.get());
    for (ProjectOopHirFieldInitializer& initializer : fieldInitializers_) visit(initializer.value.get());
    return ok;
}

ProjectOopHirNode::~ProjectOopHirNode()
{
    std::vector<std::unique_ptr<ProjectOopHirNode>> pending;
    pending.swap(children);
    while (!pending.empty())
    {
        std::unique_ptr<ProjectOopHirNode> current = std::move(pending.back());
        pending.pop_back();
        if (current == nullptr) continue;
        std::size_t childCount = 0;
        for (const std::unique_ptr<ProjectOopHirNode>& child : current->children)
            if (child != nullptr) ++childCount;
        pending.reserve(pending.size() + childCount);
        for (std::unique_ptr<ProjectOopHirNode>& child : current->children)
            if (child != nullptr) pending.push_back(std::move(child));
        current->children.clear();
    }
}

ProjectOopHirModel::~ProjectOopHirModel()
{
    for (ProjectOopHirFunction& function : functions_)
        destroyHirTreeIteratively(function.body);
    for (ProjectOopHirFieldInitializer& initializer : fieldInitializers_)
        destroyHirTreeIteratively(initializer.value);
}

bool ProjectOopHirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::string& error,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::size_t workerCount,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel,
    const ProjectMetaprogrammingModel* metaprogrammingModel)
{
    driver::CompilationWorkerPool workerPool(workerCount);
    return build(
        nameLookup, memberModel, inheritanceModel, interfaceModel, methodBodyModel,
        constructionModel, objectLayoutModel, error, genericConstraintModel,
        workerCount, namespaceFunctionModel, metaprogrammingModel, &workerPool);
}

bool ProjectOopHirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::string& error,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::size_t workerCount,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel,
    const ProjectMetaprogrammingModel* metaprogrammingModel,
    driver::CompilationWorkerPool* workerPool)
{
    workerPool_ = workerPool;
    return buildImpl(
        nameLookup,
        memberModel,
        inheritanceModel,
        interfaceModel,
        methodBodyModel,
        constructionModel,
        objectLayoutModel,
        error,
        genericConstraintModel,
        workerCount,
        namespaceFunctionModel,
        metaprogrammingModel,
        std::nullopt,
        nullptr);
}

std::span<const ProjectOopHirFunction> ProjectOopHirModel::functions() const noexcept
{
    return functions_;
}

const MethodExpressionTypeBinding* ProjectOopHirModel::yieldOperandBinding(
    const ast::AstNode* syntax, std::size_t callableOrdinal) const noexcept
{
    return methodBodyModel_ == nullptr || syntax == nullptr ? nullptr :
        methodBodyModel_->expressionTypeBinding(syntax, callableOrdinal);
}

std::span<const ProjectOopHirFieldInitializer> ProjectOopHirModel::fieldInitializers() const noexcept
{
    return fieldInitializers_;
}

const ProjectOopHirFunction* ProjectOopHirModel::functionForMember(std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= functionByMember_.size())
    {
        return functionForNamespaceFunction(memberOrdinal - functionByMember_.size());
    }
    if (!functionByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByMember_[memberOrdinal]];
}

const ProjectOopHirFunction* ProjectOopHirModel::functionForNamespaceFunction(
    std::size_t functionOrdinal) const noexcept
{
    if (functionOrdinal >= functionByNamespaceFunction_.size() ||
        !functionByNamespaceFunction_[functionOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByNamespaceFunction_[functionOrdinal]];
}

bool ProjectOopHirModel::isBuilt() const noexcept
{
    return built_;
}

SourceRange ProjectOopHirModel::lastErrorRange() const noexcept
{
    return lastErrorRange_;
}
}
