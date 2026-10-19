#pragma once

#include "nativesharp/Frontend/ComptimeAttributeDirective.h"
#include "nativesharp/Frontend/ComptimeAttributeRetention.h"
#include "nativesharp/Frontend/ComptimeAttributeTarget.h"
#include "nativesharp/Frontend/ComptimeAuthoredDiagnostic.h"
#include "nativesharp/Frontend/ComptimeCache.h"
#include "nativesharp/Frontend/ComptimeDependency.h"
#include "nativesharp/Frontend/ComptimeDiagnosticSeverity.h"
#include "nativesharp/Frontend/ComptimeEvaluation.h"
#include "nativesharp/Frontend/ComptimeExecutionContext.h"
#include "nativesharp/Frontend/ComptimeLimits.h"
#include "nativesharp/Frontend/ComptimeResult.h"
#include "nativesharp/Frontend/ProjectComptimeRecord.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
// Executes the deliberately small NativeSharp 2.0 comptime core. `comptime` is a contextual
// marker and `#run(integer-expression)` is replaced with its signed 64-bit result before lexing.
// Replacements retain the original byte count, so every later source range still names the
// original file offset. The engine never invokes native code or the host environment.
class ComptimeEngine final
{
public:
    explicit ComptimeEngine(
        ComptimeLimits limits = {},
        std::shared_ptr<ComptimeCache> cache = {},
        ComptimeExecutionContext context = {}) noexcept;
    ComptimeResult execute(std::string_view source) const;

private:
    enum class Scan;

    bool initializeExecution(
        std::string_view& source,
        ComptimeResult& result) const;
    bool cancellationRequested(ComptimeResult& result) const;
    static bool advanceScanState(
        std::string_view source,
        std::size_t& cursor,
        Scan& state,
        bool& escaped);
    bool processCapabilityDirective(
        std::string_view source,
        std::size_t& cursor,
        ComptimeResult& result,
        bool& matched) const;
    static bool processAuthoredDiagnosticDirective(
        std::string_view source,
        std::size_t& cursor,
        ComptimeResult& result,
        bool& matched);
    static bool processAttributeDirective(
        std::string_view source,
        std::size_t& cursor,
        ComptimeResult& result,
        bool& matched);
    bool locateRunExpression(
        std::string_view source,
        std::size_t cursor,
        std::size_t& open,
        std::size_t& end,
        ComptimeResult& result) const;
    bool resolveRunExpression(
        std::string_view expression,
        std::size_t cursor,
        std::string& resolvedExpression,
        std::string& dependencyDigest,
        ComptimeResult& result) const;
    bool evaluateRunExpression(
        const std::string& resolvedExpression,
        const std::string& dependencyDigest,
        std::size_t cursor,
        std::optional<std::int64_t>& value,
        bool& cacheHit,
        ComptimeResult& result) const;
    static bool commitRunEvaluation(
        std::size_t cursor,
        std::size_t end,
        const std::string& dependencyDigest,
        std::int64_t value,
        bool cacheHit,
        ComptimeResult& result);
    bool processRunDirective(
        std::string_view source,
        std::size_t& cursor,
        ComptimeResult& result,
        bool& matched) const;

    ComptimeLimits limits_;
    std::shared_ptr<ComptimeCache> cache_;
    ComptimeExecutionContext context_;
};
}
