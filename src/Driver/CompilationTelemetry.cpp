#include "nativesharp/Driver/CompilationTelemetry.h"
#include "Internal/CompilationTimeTracePhase.h"

#include "nativesharp/Driver/CompilerConsole.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#endif

namespace nativesharp::driver
{
namespace
{
std::string jsonEscape(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character >= 0x20U) escaped.push_back(static_cast<char>(character));
            break;
        }
    }
    return escaped;
}

bool parseUnsigned(std::string_view text, long long& value) noexcept
{
    if (text.empty()) return false;
    long long parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed < 0)
        return false;
    value = parsed;
    return true;
}

void parseTimeTracePhase(
    std::string_view line,
    std::vector<CompilationTimeTracePhase>& phases)
{
    constexpr std::string_view phasePrefix = "timing phase=";
    constexpr std::string_view memoryMarker = " peak_working_set_bytes=";
    const std::size_t durationSeparator = line.find(" us=");
    const std::size_t memorySeparator = line.find(memoryMarker);
    if (durationSeparator == std::string_view::npos ||
        durationSeparator <= phasePrefix.size())
        return;
    long long microseconds = 0;
    const std::size_t durationEnd = memorySeparator == std::string_view::npos
        ? line.size()
        : memorySeparator;
    long long phasePeakWorkingSetBytes = 0;
    if (parseUnsigned(line.substr(
            durationSeparator + 4U,
            durationEnd - (durationSeparator + 4U)), microseconds) &&
        (memorySeparator == std::string_view::npos ||
         parseUnsigned(line.substr(memorySeparator + memoryMarker.size()),
             phasePeakWorkingSetBytes)))
        phases.push_back({
            std::string(line.substr(
                phasePrefix.size(), durationSeparator - phasePrefix.size())),
            microseconds,
            phasePeakWorkingSetBytes});
}

void parseTimeTraceTotal(
    std::string_view line,
    long long& totalMilliseconds,
    long long& peakWorkingSetBytes)
{
    constexpr std::string_view totalPrefix = "timing total_ms=";
    constexpr std::string_view memoryMarker = " peak_working_set_bytes=";
    const std::size_t memorySeparator = line.find(memoryMarker);
    const std::size_t totalEnd = memorySeparator == std::string_view::npos
        ? line.size()
        : memorySeparator;
    parseUnsigned(
        line.substr(totalPrefix.size(), totalEnd - totalPrefix.size()),
        totalMilliseconds);
    if (memorySeparator != std::string_view::npos)
        parseUnsigned(line.substr(memorySeparator + memoryMarker.size()),
            peakWorkingSetBytes);
}

void parseTimeTraceProjectReads(
    std::string_view line,
    long long& projectDescriptorFileReads,
    long long& projectDescriptorCacheHits)
{
    constexpr std::string_view projectReadsPrefix =
        "timing project_descriptor_file_reads=";
    constexpr std::string_view cacheMarker = " project_descriptor_cache_hits=";
    const std::size_t cacheSeparator = line.find(cacheMarker);
    const std::size_t readsEnd = cacheSeparator == std::string_view::npos
        ? line.size()
        : cacheSeparator;
    parseUnsigned(
        line.substr(projectReadsPrefix.size(), readsEnd - projectReadsPrefix.size()),
        projectDescriptorFileReads);
    if (cacheSeparator != std::string_view::npos)
        parseUnsigned(line.substr(cacheSeparator + cacheMarker.size()),
            projectDescriptorCacheHits);
}

void appendTimeTracePhases(
    std::ostringstream& output,
    const std::vector<CompilationTimeTracePhase>& phases)
{
    for (std::size_t index = 0; index < phases.size(); ++index)
    {
        if (index != 0) output << ',';
        output << "\n    {\"name\": \"" << jsonEscape(phases[index].name)
               << "\", \"duration_us\": " << phases[index].durationMicroseconds
               << ", \"peak_working_set_bytes\": "
               << phases[index].peakWorkingSetBytes << '}';
    }
    if (!phases.empty()) output << '\n';
}
}

bool parseProgressFormat(std::string_view value, ProgressFormat& format) noexcept
{
    if (value == "files")
    {
        format = ProgressFormat::Files;
        return true;
    }
    if (value == "text")
    {
        format = ProgressFormat::Text;
        return true;
    }
    if (value == "json")
    {
        format = ProgressFormat::Json;
        return true;
    }
    return false;
}

std::uint64_t currentProcessPeakWorkingSetBytes() noexcept
{
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(
            GetCurrentProcess(), &counters, static_cast<DWORD>(sizeof(counters))) != 0)
        return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#endif
    return 0;
}

std::string renderCompilationEvent(
    const basic::CompilationEvent& event,
    ProgressFormat format)
{
    if (format == ProgressFormat::Files)
    {
        if (event.kind != basic::CompilationEventKind::WorkFinished || event.phase != "syntax-read")
            return {};
        return "[" + std::to_string(event.current) + "/" + std::to_string(event.total) +
            "] Compiling " + std::string(event.detail);
    }
    const auto durationMicroseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(event.duration).count();
    if (format == ProgressFormat::Json)
    {
        std::ostringstream output;
        output << "{\"schema\":\"nativesharp-progress-event\",\"version\":1"
               << ",\"event\":\"" << basic::compilationEventKindName(event.kind) << '"'
               << ",\"phase\":\"" << jsonEscape(event.phase) << '"';
        if (!event.detail.empty())
            output << ",\"detail\":\"" << jsonEscape(event.detail) << '"';
        if (event.current != 0 || event.total != 0)
            output << ",\"current\":" << event.current << ",\"total\":" << event.total;
        if (event.duration.count() != 0)
            output << ",\"duration_us\":" << durationMicroseconds;
        output << '}';
        return output.str();
    }

    switch (event.kind)
    {
    case basic::CompilationEventKind::PhaseStarted:
        return "-- NSC: " + std::string(event.phase);
    case basic::CompilationEventKind::PhaseFinished:
        return "-- NSC: " + std::string(event.phase) + std::string(compilerText(
            " finished (", " finalizada (")) +
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                event.duration).count()) + " ms)";
    case basic::CompilationEventKind::WorkStarted:
        if (event.phase == "LLVM")
            return "-- NSC: compiling LLVM module " + std::string(event.detail);
        return "-- NSC: " + std::string(event.phase) + " " + std::string(event.detail);
    case basic::CompilationEventKind::WorkFinished:
        return "[" + std::to_string(event.current) + "/" + std::to_string(event.total) +
            "] NSC " + std::string(event.phase) + " " + std::string(event.detail);
    case basic::CompilationEventKind::CacheHit:
        return "-- NSC: cache hit " + std::string(event.phase) + " " +
            std::string(event.detail);
    case basic::CompilationEventKind::CacheMiss:
        return "-- NSC: cache miss " + std::string(event.phase) + " " +
            std::string(event.detail);
    case basic::CompilationEventKind::Skipped:
        return "-- NSC: skipped " + std::string(event.phase) + " " +
            std::string(event.detail);
    case basic::CompilationEventKind::Failed:
        return "-- NSC: failed " + std::string(event.phase) + " " +
            std::string(event.detail);
    }
    return {};
}

std::string renderCompilationTimeTraceJson(std::string_view timingOutput, int exitCode)
{
    std::vector<CompilationTimeTracePhase> phases;
    long long totalMilliseconds = 0;
    long long peakWorkingSetBytes = 0;
    long long projectDescriptorFileReads = 0;
    long long projectDescriptorCacheHits = 0;
    std::size_t position = 0;
    while (position <= timingOutput.size())
    {
        const std::size_t end = timingOutput.find('\n', position);
        const std::string_view line = timingOutput.substr(
            position, end == std::string_view::npos ? timingOutput.size() - position
                                                    : end - position);
        constexpr std::string_view phasePrefix = "timing phase=";
        constexpr std::string_view totalPrefix = "timing total_ms=";
        constexpr std::string_view projectReadsPrefix =
            "timing project_descriptor_file_reads=";
        if (line.starts_with(phasePrefix))
            parseTimeTracePhase(line, phases);
        else if (line.starts_with(totalPrefix))
            parseTimeTraceTotal(line, totalMilliseconds, peakWorkingSetBytes);
        else if (line.starts_with(projectReadsPrefix))
            parseTimeTraceProjectReads(
                line,
                projectDescriptorFileReads,
                projectDescriptorCacheHits);
        if (end == std::string_view::npos) break;
        position = end + 1U;
    }

    std::ostringstream output;
    output << "{\n  \"schema\": \"nativesharp-time-trace\",\n"
           << "  \"version\": 1,\n"
           << "  \"exit_code\": " << exitCode << ",\n"
           << "  \"total_ms\": " << totalMilliseconds << ",\n"
           << "  \"peak_working_set_bytes\": " << peakWorkingSetBytes << ",\n"
           << "  \"project_descriptor_file_reads\": "
           << projectDescriptorFileReads << ",\n"
           << "  \"project_descriptor_cache_hits\": "
           << projectDescriptorCacheHits << ",\n"
           << "  \"phases\": [";
    appendTimeTracePhases(output, phases);
    output << "  ]\n}\n";
    return output.str();
}
}
