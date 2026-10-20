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
struct MethodCallBinding final
{
    MethodCallResolutionKind resolution = MethodCallResolutionKind::NoViableCandidate;
    const ast::AstNode* syntax = nullptr;
    std::size_t callerMethodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string methodName;
    bool explicitThis = false;
    bool implicitThis = false;
    bool baseReceiver = false;
    bool staticCall = false;
    bool interfaceCall = false;
    bool forwardedReceiver = false;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    std::vector<std::size_t> candidateMemberOrdinals;
    std::vector<MethodCallCandidate> evaluatedCandidates;
    std::vector<std::size_t> viableCandidateMemberOrdinals;
    std::optional<std::size_t> selectedMemberOrdinal;
    std::vector<ProjectImplicitConversion> selectedConversions;
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<bool> selectedParameterPackElements;
    std::optional<std::size_t> selectedParameterPackOrdinal;
    bool selectedUsesExpandedParameterPack = false;
    bool selectedSynthesizesEmptyParameterPack = false;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    // Source expressions in the same order as selectedArgumentParameterOrdinals. Retained so
    // T06-F07 can propagate escape contracts after the complete call graph has been analyzed.
    std::vector<const ast::AstNode*> selectedArgumentSyntaxes;
    std::string resultTypeKey;
};
}
