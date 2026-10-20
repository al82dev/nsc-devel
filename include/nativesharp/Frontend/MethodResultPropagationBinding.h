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
struct MethodResultPropagationBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    const ast::AstNode* operandSyntax = nullptr;
    std::string sourceCarrierTypeKey;
    std::string successTypeKey;
    std::string sourceErrorTypeKey;
    std::string returnCarrierTypeKey;
    std::string returnErrorTypeKey;
    std::size_t sourceSuccessTag = 0;
    std::size_t sourceErrorTag = 0;
    std::size_t returnErrorTag = 0;
    std::size_t sourceSuccessTestMemberOrdinal = 0;
    std::size_t sourceSuccessExtractorMemberOrdinal = 0;
    std::size_t sourceErrorExtractorMemberOrdinal = 0;
    std::size_t returnErrorFactoryMemberOrdinal = 0;
    // NS304-T13-F08: the Ok factory of the SOURCE carrier (used to wrap an owned value on success).
    std::size_t sourceSuccessFactoryMemberOrdinal = 0;
    std::optional<std::size_t> errorConversionMemberOrdinal;
};
}
