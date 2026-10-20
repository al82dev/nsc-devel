#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisContext.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisStatistics.h"
#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectConstructionSite.h"
#include "nativesharp/Frontend/ProjectOwnershipFlowStatistics.h"
#include "nativesharp/Frontend/ProjectEscapePropagationStatistics.h"
#include "nativesharp/Frontend/ProjectMethodBodyBuildOptions.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Sema/ExpressionInfo.h"
#include "nativesharp/Frontend/MethodDelegateOperationKind.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct MethodDelegateBinding final
{
    MethodDelegateOperationKind kind = MethodDelegateOperationKind::Create;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string delegateTypeKey;
    std::size_t delegateTypeOrdinal = 0;
    std::optional<std::size_t> selectedMemberOrdinal;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    bool staticTarget = false;
    // T08-F05. A method value keeps its category through HIR: a virtual/interface target is
    // resolved from the bound receiver when the native delegate cell is created.  A base-qualified
    // method value remains a direct target, matching ordinary base-call semantics.
    bool virtualDispatch = false;
    bool interfaceDispatch = false;
    bool ownsReceiver = false;
    std::optional<std::size_t> receiverSourceLocalOrdinal;
    std::optional<std::size_t> receiverSourceParameterOrdinal;
    // T08-F02. Non-empty only when invoking a zero-size anonymous non-capturing lambda. In that
    // case there is no delegate cell/function-pointer child to evaluate: HIR/MIR call this
    // generated static callable directly.
    std::string directLambdaName;
    // T08-F04. A capturing generic lambda can have several monomorphized entry points while
    // retaining one lexical environment. The callee expression supplies the delegate cell whose
    // receiver is passed to directLambdaName; its stored code pointer is intentionally ignored.
    bool directLambdaUsesEnvironment = false;
    std::string resultTypeKey;
    std::vector<ProjectImplicitConversion> argumentConversions;
    // NS304-T04-F04: valores default (sintaxis) de parametros omitidos en la invocacion, en orden
    // de parametro. El HIR los baja como argumentos reales.
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::string> selectedDefaultArgumentTypeKeys;
};
}
