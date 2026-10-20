#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "GenericModelSupport.h"
namespace nativesharp::frontend
{
ProjectGenericModel::ProjectGenericModel(
    std::size_t maximumInstantiationDepth,
    std::size_t maximumInstantiationCount) noexcept
    : instantiationBudget_(maximumInstantiationDepth, maximumInstantiationCount)
{
}
namespace internal
{
#if 0 // INSTRUMENTATION CODE: preserved diagnostic breadcrumbs; disabled in normal builds.
GenericWorkProgress::GenericWorkProgress(
    GenericTelemetry& telemetry,
    std::string_view phase,
    std::string detail,
    std::size_t current,
    std::size_t total)
    : telemetry_(&telemetry), phase_(phase), detail_(std::move(detail)),
      current_(current), total_(total)
{
    telemetry_->reportProgress(
        basic::CompilationEventKind::WorkStarted, phase_, detail_, current_, total_);
}

GenericWorkProgress::~GenericWorkProgress()
{
    telemetry_->reportProgress(
        basic::CompilationEventKind::WorkFinished, phase_, detail_, current_, total_);
}

std::string genericUnitDetail(const ProjectSyntaxUnit& unit)
{
    return "module=" + std::string(unit.moduleName()) +
        " project=" + std::to_string(unit.projectOrdinal()) +
        " source=" + std::to_string(unit.sourceOrdinal()) +
        " precompiled=" + (unit.isPrecompiled() ? "1" : "0");
}

std::string compactTraceText(std::string_view value)
{
    constexpr std::size_t maximumLength = 64U;
    std::string result;
    result.reserve(std::min(value.size(), maximumLength) + 3U);
    for (const char character : value.substr(0, maximumLength))
    {
        result.push_back(
            character == '\r' || character == '\n' || character == '\t' ? ' ' : character);
    }
    if (value.size() > maximumLength) result += "...";
    return result;
}

std::string genericNodeDetail(const ProjectSyntaxUnit& unit, const ast::AstNode& node)
{
    return genericUnitDetail(unit) +
        " kind=" + std::string(ast::astNodeKindName(node.kind())) +
        " begin=" + std::to_string(node.range().begin().offset()) +
        " end=" + std::to_string(node.range().end().offset()) +
        " text=" + compactTraceText(node.text());
}

std::string genericTypeDetail(const ast::TypeSyntax& syntax)
{
    return "type=" + compactTraceText(syntax.baseName()) +
        " begin=" + std::to_string(syntax.range().begin().offset()) +
        " end=" + std::to_string(syntax.range().end().offset()) +
        " arguments=" + std::to_string(syntax.typeArguments().size()) +
        " modifiers=" + std::to_string(syntax.modifiers().size()) +
        " function=" + (syntax.isFunction() ? "1" : "0");
}
#endif // INSTRUMENTATION CODE
}
}
