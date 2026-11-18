#pragma once

// Experimental standalone codec, not wired to NativeSharp metadata.
#include "ViewProvenanceSummaryPrototype.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend::view_provenance_prototype
{
// Arguments and output eligibility must come from the independently resolved
// callable signature, never from the bytes being checked.
inline std::optional<Summary> decodeContract(
    const std::vector<std::uint8_t>& bytes, std::size_t parameterCount,
    const std::vector<bool>& outputParameters, bool permitsViewReturn,
    const std::string& callableKey)
{
    if (callableKey.empty() || outputParameters.size() != parameterCount || bytes.size() < 3 ||
        bytes[0] != 'V' || bytes[1] != 'P') return std::nullopt;
    if (bytes[2] == 0)
        return bytes.size() == 3 ? std::optional<Summary>(Summary{}) : std::nullopt;
    if (bytes[2] != 1 || bytes.size() < 6) return std::nullopt;
    const std::size_t keySize = bytes[3];
    if (keySize != callableKey.size() || bytes.size() < 6 + keySize) return std::nullopt;
    for (std::size_t i = 0; i < keySize; ++i)
        if (bytes[4 + i] != static_cast<std::uint8_t>(callableKey[i])) return std::nullopt;
    if (bytes[4 + keySize] != parameterCount) return std::nullopt;
    const std::size_t count = bytes[5 + keySize];
    if (bytes.size() != 6 + keySize + count * 3) return std::nullopt;
    Summary result;
    result.verified = true;
    result.parameterCount = parameterCount;
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t base = 6 + keySize + i * 3;
        const std::size_t source = bytes[base];
        const std::uint8_t rawKind = bytes[base + 1];
        const std::size_t target = bytes[base + 2];
        if (source >= parameterCount || rawKind > 2) return std::nullopt;
        const SinkKind kind = static_cast<SinkKind>(rawKind);
        if ((kind == SinkKind::Return && (!permitsViewReturn || target != 0)) ||
            (kind == SinkKind::Output && (target >= parameterCount || !outputParameters[target])) ||
            (kind == SinkKind::External && target != 0)) return std::nullopt;
        for (const Edge& previous : result.edges)
            if (previous.sourceParameter == source && previous.sink.kind == kind &&
                previous.sink.ordinal == target) return std::nullopt;
        result.edges.push_back({source, {kind, target}});
    }
    return result;
}

inline std::optional<std::vector<std::uint8_t>> encodeContract(const Summary& summary, const std::string& callableKey)
{
    if (!summary.verified) return std::vector<std::uint8_t>{'V', 'P', 0};
    if (callableKey.empty() || callableKey.size() > 255 || summary.parameterCount > 255 ||
        summary.edges.size() > 255) return std::nullopt;
    std::vector<std::uint8_t> bytes{'V', 'P', 1, static_cast<std::uint8_t>(callableKey.size())};
    for (const char c : callableKey) bytes.push_back(static_cast<std::uint8_t>(c));
    bytes.push_back(static_cast<std::uint8_t>(summary.parameterCount));
    bytes.push_back(static_cast<std::uint8_t>(summary.edges.size()));
    for (const Edge& edge : summary.edges)
    {
        if (edge.sourceParameter > 255 || edge.sink.ordinal > 255 ||
            static_cast<unsigned>(edge.sink.kind) > 2) return std::nullopt;
        bytes.push_back(static_cast<std::uint8_t>(edge.sourceParameter));
        bytes.push_back(static_cast<std::uint8_t>(edge.sink.kind));
        bytes.push_back(static_cast<std::uint8_t>(edge.sink.ordinal));
    }
    return bytes;
}
}
