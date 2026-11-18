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
#include "nativesharp/Frontend/MethodCallResolutionKind.h"
#include "nativesharp/Frontend/MethodCallCandidate.h"

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
struct NamespaceFunctionCallBinding final
{
    MethodCallResolutionKind resolution = MethodCallResolutionKind::NoViableCandidate;
    const ast::AstNode* syntax = nullptr;
    std::size_t callerCallableOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string writtenName;
    bool forwardedReceiver = false;
    const ast::AstNode* receiverSyntax = nullptr;
    std::vector<std::size_t> candidateFunctionOrdinals;
    std::vector<MethodCallCandidate> evaluatedCandidates;
    std::vector<std::size_t> viableCandidateFunctionOrdinals;
    std::optional<std::size_t> selectedFunctionOrdinal;
    std::vector<ProjectImplicitConversion> selectedConversions;
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<bool> selectedParameterPackElements;
    std::optional<std::size_t> selectedParameterPackOrdinal;
    bool selectedUsesExpandedParameterPack = false;
    bool selectedSynthesizesEmptyParameterPack = false;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    std::vector<const ast::AstNode*> selectedArgumentSyntaxes;
    std::string resultTypeKey;
    // EN: T19-039. The call performs a C `op=` on a `_Complex` carrier: the first argument is the address of the
    //     assignment target, evaluated once, and the second is the right-hand side.
    // ES: T19-039. La llamada realiza un `op=` C sobre un portador `_Complex`: el primer argumento es la direccion
    //     del destino de la asignacion, evaluado una vez, y el segundo es el lado derecho.
    bool cComplexCompoundAssignment = false;
    // T19-040: call arguments the macro took as tokens; HIR skips them. / Argumentos que la macro tomo como
    //     tokens; HIR los salta.
    std::vector<std::size_t> cppMacroTokenArguments;
};
}
