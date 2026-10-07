#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using RawMeasurement =
    std::tuple<long long, long long, long long, long long, long long, std::string>;
using Measurement = std::tuple<long long, long long, long long, long long, std::string>;
using MetricSamples = std::array<std::vector<long long>, 4>;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

std::string buildSource(std::size_t callableCount)
{
    std::string source = "class WaveProbe { ";
    source.reserve(callableCount * 220U);
    for (std::size_t index = 0; index < callableCount; ++index)
    {
        const std::string suffix = std::to_string(index);
        source.append("public static int M")
            .append(suffix)
            .append("(int value) { int total = value + ")
            .append(suffix)
            .append("; if (total > 0) { total = total + 3; } else { total = 1; } ")
            .append("while (total < 8) { total = total + 1; } return total; } ");
    }
    return source + "}\n";
}

long long phaseMicroseconds(std::string_view output, std::string_view phase)
{
    const std::string prefix = "timing phase=" + std::string(phase) + " us=";
    const std::size_t begin = output.find(prefix);
    if (begin == std::string_view::npos) return -1;
    const std::size_t valueBegin = begin + prefix.size();
    const std::size_t valueEnd = output.find(' ', valueBegin);
    return std::stoll(std::string(output.substr(valueBegin, valueEnd - valueBegin)));
}

long long phasePeakBytes(std::string_view output, std::string_view phase)
{
    const std::string prefix = "timing phase=" + std::string(phase) + " ";
    const std::size_t begin = output.find(prefix);
    if (begin == std::string_view::npos) return -1;
    const std::size_t lineEnd = output.find('\n', begin);
    constexpr std::string_view marker = "peak_working_set_bytes=";
    const std::size_t markerBegin = output.find(marker, begin);
    if (markerBegin == std::string_view::npos || markerBegin > lineEnd) return -1;
    const std::size_t valueBegin = markerBegin + marker.size();
    const std::size_t space = output.find(' ', valueBegin);
    const std::size_t valueEnd = space == std::string_view::npos || space > lineEnd
        ? lineEnd : space;
    return std::stoll(std::string(output.substr(valueBegin, valueEnd - valueBegin)));
}

std::string semanticSummary(std::string_view output)
{
    constexpr std::string_view prefix = "project method-body semantic check:";
    const std::size_t begin = output.find(prefix);
    if (begin == std::string_view::npos) return {};
    const std::size_t end = output.find('\n', begin);
    return std::string(output.substr(begin, end - begin));
}

RawMeasurement measure(const std::filesystem::path& project, std::string_view jobs)
{
    const std::string projectText = project.string();
    const std::vector<std::string_view> arguments{
        "--check-project-method-bodies", projectText, "-O0", "-j", jobs, "--time"};
    std::ostringstream output;
    std::ostringstream error;
    const auto started = Clock::now();
    const nativesharp::driver::Driver driver;
    const nativesharp::ExitCode code = driver.run(arguments, output, error);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - started).count();
    if (code != nativesharp::ExitCode::Success || !error.str().empty())
        throw std::runtime_error("performance probe failed: " + error.str());
    return {
        elapsed,
        phaseMicroseconds(output.str(), "method-bodies"),
        phaseMicroseconds(output.str(), "method-bodies-wave-0-analysis"),
        phaseMicroseconds(output.str(), "method-bodies-wave-0-merge"),
        phasePeakBytes(output.str(), "method-bodies"),
        semanticSummary(output.str())};
}

void appendSample(
    MetricSamples& samples,
    std::string& summary,
    long long& maximumPeakBytes,
    RawMeasurement measurement)
{
    auto [wall, bodies, analysis, merge, peakBytes, currentSummary] = std::move(measurement);
    if (!summary.empty() && summary != currentSummary)
        throw std::runtime_error("performance probe changed between repetitions");
    summary = std::move(currentSummary);
    samples[0].push_back(wall);
    samples[1].push_back(bodies);
    samples[2].push_back(analysis);
    samples[3].push_back(merge);
    maximumPeakBytes = std::max(maximumPeakBytes, peakBytes);
}

long long median(std::vector<long long> values)
{
    std::ranges::sort(values);
    return values[values.size() / 2U];
}

Measurement summarize(const MetricSamples& samples, std::string summary)
{
    return {median(samples[0]), median(samples[1]), median(samples[2]),
        median(samples[3]), std::move(summary)};
}

std::array<Measurement, 3> measureConfigurations(
    const std::filesystem::path& project,
    long long& maximumPeakBytes)
{
    constexpr std::array<std::string_view, 3> jobs{"1", "2", "12"};
    constexpr std::size_t repetitions = 3;
    std::array<MetricSamples, jobs.size()> samples;
    std::array<std::string, jobs.size()> summaries;
    for (std::size_t index = 0; index < jobs.size(); ++index)
    {
        RawMeasurement warmup = measure(project, jobs[index]);
        maximumPeakBytes = std::max(maximumPeakBytes, std::get<4>(warmup));
        summaries[index] = std::move(std::get<5>(warmup));
    }
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        for (std::size_t offset = 0; offset < jobs.size(); ++offset)
        {
            const std::size_t index = (repetition + offset) % jobs.size();
            appendSample(
                samples[index], summaries[index], maximumPeakBytes,
                measure(project, jobs[index]));
        }
    }
    std::array<Measurement, jobs.size()> result;
    for (std::size_t index = 0; index < jobs.size(); ++index)
        result[index] = summarize(samples[index], std::move(summaries[index]));
    return result;
}

int runProbe()
{
    constexpr std::size_t callableCount = 1024;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nsc_callable_wave_performance";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    const std::filesystem::path project = root / "CallableWavePerformance.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>CallableWavePerformance</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>\n");
    writeFile(root / "Main.ns", buildSource(callableCount));
    long long maximumPeakBytes = 0;
    const std::array<Measurement, 3> measurements =
        measureConfigurations(project, maximumPeakBytes);
    const auto& [j1Wall, j1Bodies, j1Analysis, j1Merge, j1Summary] = measurements[0];
    const auto& [j2Wall, j2Bodies, j2Analysis, j2Merge, j2Summary] = measurements[1];
    const auto& [j12Wall, j12Bodies, j12Analysis, j12Merge, j12Summary] = measurements[2];
    std::filesystem::remove_all(root, cleanupError);
    if (j1Summary.empty() || j1Summary != j2Summary || j2Summary != j12Summary)
        throw std::runtime_error("performance probe semantic summaries differ");
    std::cout << "callables=" << callableCount
              << " warmup=1 repetitions=3 schedule=rotating statistic=median\n"
              << "j1 wall_ms=" << j1Wall << " method_bodies_us=" << j1Bodies
              << " wave_analysis_us=" << j1Analysis << " wave_merge_us=" << j1Merge << '\n'
              << "j2 wall_ms=" << j2Wall << " method_bodies_us=" << j2Bodies
              << " wave_analysis_us=" << j2Analysis << " wave_merge_us=" << j2Merge << '\n'
              << "j12 wall_ms=" << j12Wall << " method_bodies_us=" << j12Bodies
              << " wave_analysis_us=" << j12Analysis << " wave_merge_us=" << j12Merge << '\n'
              << "maximum_peak_working_set_bytes=" << maximumPeakBytes << '\n';
    return 0;
}
}

// NOLINTNEXTLINE(bugprone-exception-escape): all exception categories are reported below.
int main()
{
    try
    {
        return runProbe();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "FAIL: unknown callable-wave performance exception\n";
    }
    return 1;
}
