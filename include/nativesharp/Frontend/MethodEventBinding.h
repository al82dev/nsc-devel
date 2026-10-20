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
#include "nativesharp/Frontend/MethodEventOperationKind.h"

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
struct MethodEventBinding final
{
    MethodEventOperationKind kind = MethodEventOperationKind::Add;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::size_t eventMemberOrdinal = 0;
    std::size_t backingMemberOrdinal = 0;
    std::string delegateTypeKey;
    std::size_t delegateTypeOrdinal = 0;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    bool staticEvent = false;
    std::string resultTypeKey;
    std::vector<ProjectImplicitConversion> argumentConversions;
    // NS304-T09-F08: si el event declara accessors custom, el ordinal del metodo add_/remove_ a
    // invocar (en lugar de la storage sintetizada).
    std::optional<std::size_t> customAccessorMemberOrdinal;
};
}
