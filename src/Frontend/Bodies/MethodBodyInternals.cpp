#include "MethodBodyInternals.h"

#include <fstream>
#include <mutex>

namespace nativesharp::frontend::method_body_detail
{
std::mutex& traceMutex()
{
    static std::mutex mutex;
    return mutex;
}

void resetMethodBodyTrace()
{
    if constexpr (!kTraceEnabled) return;
    const std::lock_guard<std::mutex> lock(traceMutex());
    std::ofstream trace(kTracePath, std::ios::binary | std::ios::trunc);
    if (!trace) return;
    trace << "nsc-method-bodies-trace version=1\r\n";
    trace.flush();
}

void writeMethodBodyTrace(std::string_view event, std::string_view detail)
{
    if constexpr (!kTraceEnabled) return;
    const std::lock_guard<std::mutex> lock(traceMutex());
    std::ofstream trace(kTracePath, std::ios::binary | std::ios::app);
    if (!trace) return;
    trace << "[NSC METHOD BODIES INSTRUMENTATION CODE] event=" << event;
    if (!detail.empty()) trace << '|' << detail;
    trace << "\r\n";
    trace.flush();
}

language::PointerContract pointerContract(
    bool nullable,
    bool pointeeReadonly,
    bool hasProvenance,
    std::size_t knownAlignment,
    unsigned addressSpace) noexcept
{
    return language::PointerContract{
        nullable,
        !pointeeReadonly,
        hasProvenance,
        knownAlignment,
        addressSpace};
}

bool nestedPointerTypeKey(std::string_view typeKey) noexcept
{
    return typeKey.size() > 2 && typeKey.ends_with("**");
}

const char* nestedPointerQualificationError(
    std::string_view sourceTypeKey, std::uint8_t sourceLevels, bool sourceBaseConst, bool sourceBaseVolatile,
    std::string_view targetTypeKey, std::uint8_t targetLevels, bool targetBaseConst, bool targetBaseVolatile) noexcept
{
    if (!nestedPointerTypeKey(sourceTypeKey) || !nestedPointerTypeKey(targetTypeKey)) return nullptr;
    const std::size_t depth = language::pointerDepthOfTypeKey(targetTypeKey);
    const auto conversion = language::classifyPointerQualificationConversion(
        {sourceLevels, sourceBaseConst, sourceBaseVolatile}, {targetLevels, targetBaseConst, targetBaseVolatile},
        depth);
    if (conversion == language::PointerQualificationConversion::Identity ||
        conversion == language::PointerQualificationConversion::Allowed)
        return nullptr;
    if (conversion == language::PointerQualificationConversion::UnsafeAddition && sourceLevels == targetLevels &&
        sourceBaseVolatile == targetBaseVolatile && targetBaseConst && !sourceBaseConst)
        return kNestedPointeeConstMessage;
    if (conversion == language::PointerQualificationConversion::UnsafeAddition && sourceLevels == targetLevels &&
        sourceBaseConst == targetBaseConst && targetBaseVolatile && !sourceBaseVolatile)
        return kNestedPointeeVolatileMessage;
    return conversion == language::PointerQualificationConversion::DropsQualifier
        ? "the conversion would discard a qualifier of a pointer level ('T* const*', 'T* volatile*'); use an explicit "
          "cast"
        : "only the object the pointer points to may gain a qualifier implicitly; a deeper pointer level would be "
          "reachable without it through the original pointer; use an explicit cast";
}

bool decayedFixedArrayDropsQualifier(
    std::string_view decayedTypeKey, bool elementConst, bool elementVolatile, std::uint8_t arrayLevels,
    bool arrayBaseConst, bool arrayBaseVolatile, std::string_view parameterTypeKey, std::uint8_t parameterLevels,
    bool parameterBaseConst, bool parameterBaseVolatile) noexcept
{
    if (!nestedPointerTypeKey(decayedTypeKey))
        return (elementConst && !parameterBaseConst) || (elementVolatile && !parameterBaseVolatile);
    std::uint8_t levels = 0;
    if (!language::pointerLevelsAfterAddressOf(arrayLevels, elementConst, elementVolatile, levels)) return true;
    return nestedPointerQualificationError(decayedTypeKey, levels, arrayBaseConst, arrayBaseVolatile,
        parameterTypeKey, parameterLevels, parameterBaseConst, parameterBaseVolatile) != nullptr;
}

bool addsNestedPointeeConst(
    std::string_view sourceTypeKey,
    bool sourcePointeeReadonly,
    std::string_view targetTypeKey,
    bool targetPointeeReadonly) noexcept
{
    return targetPointeeReadonly && !sourcePointeeReadonly && nestedPointerTypeKey(targetTypeKey) &&
        nestedPointerTypeKey(sourceTypeKey);
}
}
