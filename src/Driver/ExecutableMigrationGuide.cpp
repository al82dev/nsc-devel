#include "nativesharp/Driver/ExecutableMigrationGuide.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>

namespace nativesharp::driver
{
namespace
{
std::string sourceLine(std::string_view source, std::uint32_t offset)
{
    const std::size_t bounded = std::min<std::size_t>(offset, source.size());
    const std::size_t beginSearch = bounded == 0 ? 0 : bounded - 1;
    const std::size_t previousNewline = source.rfind('\n', beginSearch);
    const std::size_t begin = previousNewline == std::string_view::npos
        ? 0 : previousNewline + 1;
    const std::size_t nextNewline = source.find('\n', bounded);
    return std::string(source.substr(begin,
        nextNewline == std::string_view::npos ? source.size() - begin : nextNewline - begin));
}

std::string applyEditToLine(
    std::string_view source,
    const MigrationTextEdit& edit)
{
    const std::size_t bounded = std::min<std::size_t>(edit.offset, source.size());
    const std::size_t beginSearch = bounded == 0 ? 0 : bounded - 1;
    const std::size_t previousNewline = source.rfind('\n', beginSearch);
    const std::size_t lineBegin = previousNewline == std::string_view::npos
        ? 0 : previousNewline + 1;
    std::string result = sourceLine(source, edit.offset);
    const std::size_t localOffset = edit.offset - lineBegin;
    if (localOffset <= result.size() && localOffset + edit.length <= result.size())
        result.replace(localOffset, edit.length, edit.replacement);
    return result;
}
}

ExecutableMigrationGuide buildExecutableMigrationGuide(
    const MigrationAnalysis& analysis,
    std::string_view source)
{
    ExecutableMigrationGuide guide;
    guide.sourcePath = analysis.sourcePath;
    std::map<std::string, std::vector<ExecutableMigrationStep>> grouped;
    for (const MigrationFinding& finding : analysis.findings)
    {
        ExecutableMigrationStep step;
        step.finding = finding;
        step.before = sourceLine(source, finding.offset);
        step.automatic = finding.safeFix.has_value();
        step.after = step.automatic
            ? applyEditToLine(source, *finding.safeFix)
            : "<manual decision: " + finding.alternative + ">";
        grouped[std::string(language::migrationDiagnosticCode(finding.issue))].push_back(
            std::move(step));
    }
    if (grouped.empty())
    {
        MigrationFinding verification;
        verification.message = "no known 2.x to 3.0 migration risk detected";
        verification.alternative = "compile the source and preserve behavioral tests";
        grouped["VERIFY"].push_back({verification, "<unchanged>", "<unchanged>", false});
    }
    int order = 1;
    for (auto& [identity, steps] : grouped)
        guide.chapters.push_back({std::move(identity), order++, std::move(steps)});
    return guide;
}

bool validateExecutableMigrationGuide(
    const ExecutableMigrationGuide& guide,
    std::string& error)
{
    std::vector<language::MigrationChapter> chapters;
    chapters.reserve(guide.chapters.size());
    for (const ExecutableMigrationChapter& chapter : guide.chapters)
    {
        if (chapter.steps.empty())
        {
            error = "executable migration chapter has no steps";
            return false;
        }
        for (const ExecutableMigrationStep& step : chapter.steps)
        {
            if (step.before.empty() || step.after.empty())
            {
                error = "executable migration step lacks before/after text";
                return false;
            }
            if (step.automatic && (!step.finding.safeFix.has_value() ||
                    step.finding.safeFix->equivalenceProof.empty()))
            {
                error = "automatic migration step lacks an equivalence proof";
                return false;
            }
        }
        chapters.push_back({chapter.identity, chapter.order, true, true, true});
    }
    return language::validateMigrationGuide(chapters, error);
}

std::string renderExecutableMigrationGuide(
    const ExecutableMigrationGuide& guide)
{
    std::ostringstream output;
    output << "# NativeSharp 2.x → 3.0 migration\n\n"
           << "Source: `" << guide.sourcePath.generic_string() << "`\n\n"
           << "## Incremental strategy\n\n"
           << "1. `nsc --analyze-migration " << guide.sourcePath.generic_string() << "`\n"
           << "2. `nsc --apply-migration-fixes " << guide.sourcePath.generic_string()
           << " -o <migrated.ns>`\n"
           << "3. Resolve every item in the manual-decision inventory.\n"
           << "4. Compile the migrated project with `-O0`.\n"
           << "5. Compare old/new NSMETA with `nsc --migration-compatibility-matrix`.\n\n";
    for (const ExecutableMigrationChapter& chapter : guide.chapters)
    {
        output << "## " << chapter.order << ". " << chapter.identity << "\n\n";
        for (const ExecutableMigrationStep& step : chapter.steps)
        {
            output << "Location: " << step.finding.line << ':' << step.finding.column << "\n\n"
                   << "Before:\n\n```nativesharp\n" << step.before << "\n```\n\n"
                   << "After:\n\n```nativesharp\n" << step.after << "\n```\n\n"
                   << "Action: " << (step.automatic ? "automatic-safe" : "manual") << "\n\n";
            if (step.automatic)
                output << "Proof: " << step.finding.safeFix->equivalenceProof << "\n\n";
        }
    }
    output << "## Manual-decision inventory\n\n";
    bool wroteManual = false;
    for (const ExecutableMigrationChapter& chapter : guide.chapters)
    {
        for (const ExecutableMigrationStep& step : chapter.steps)
        {
            if (step.automatic || chapter.identity == "VERIFY") continue;
            wroteManual = true;
            output << "- " << chapter.identity << " at " << step.finding.line << ':'
                   << step.finding.column << ": " << step.finding.alternative << '\n';
        }
    }
    if (!wroteManual) output << "- none\n";
    return output.str();
}

bool writeExecutableMigrationGuide(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    ExecutableMigrationGuide& guide,
    std::string& error)
{
    error.clear();
    std::ifstream sourceStream(sourcePath, std::ios::binary);
    if (!sourceStream)
    {
        error = "cannot open migration source: " + sourcePath.string();
        return false;
    }
    const std::string source{std::istreambuf_iterator<char>(sourceStream),
        std::istreambuf_iterator<char>()};
    if (!sourceStream.good() && !sourceStream.eof())
    {
        error = "cannot read migration source: " + sourcePath.string();
        return false;
    }
    const MigrationAnalysis analysis = analyzeMigrationSource(sourcePath, source);
    guide = buildExecutableMigrationGuide(analysis, source);
    if (!validateExecutableMigrationGuide(guide, error)) return false;

    std::error_code directoryError;
    if (!destinationPath.parent_path().empty())
        std::filesystem::create_directories(destinationPath.parent_path(), directoryError);
    if (directoryError)
    {
        error = "cannot create migration guide directory: " + directoryError.message();
        return false;
    }
    std::ofstream destination(destinationPath, std::ios::binary | std::ios::trunc);
    if (!destination)
    {
        error = "cannot open migration guide output: " + destinationPath.string();
        return false;
    }
    const std::string rendered = renderExecutableMigrationGuide(guide);
    destination.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
    if (!destination)
    {
        error = "cannot write migration guide output: " + destinationPath.string();
        return false;
    }
    return true;
}
}
