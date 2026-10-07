#include "nativesharp/Driver/MigrationAnalyzer.h"
#include "nativesharp/Driver/DeprecationPolicy.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::driver
{
namespace
{
struct Token final
{
    std::string_view text;
    std::uint32_t offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    bool identifier = false;
    bool number = false;
};

bool identifierStart(char value) noexcept
{
    const unsigned char ch = static_cast<unsigned char>(value);
    return std::isalpha(ch) != 0 || value == '_';
}

bool identifierContinue(char value) noexcept
{
    const unsigned char ch = static_cast<unsigned char>(value);
    return std::isalnum(ch) != 0 || value == '_';
}

std::vector<Token> tokenize(std::string_view source)
{
    std::vector<Token> result;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::size_t cursor = 0;
    const auto advance = [&](char ch) {
        if (ch == '\n') { ++line; column = 1; }
        else { ++column; }
    };
    while (cursor < source.size())
    {
        const char ch = source[cursor];
        if (std::isspace(static_cast<unsigned char>(ch)) != 0)
        {
            advance(ch);
            ++cursor;
            continue;
        }
        if (ch == '/' && cursor + 1 < source.size() && source[cursor + 1] == '/')
        {
            while (cursor < source.size() && source[cursor] != '\n')
            {
                advance(source[cursor]);
                ++cursor;
            }
            continue;
        }
        if (ch == '/' && cursor + 1 < source.size() && source[cursor + 1] == '*')
        {
            advance(source[cursor++]);
            advance(source[cursor++]);
            while (cursor < source.size())
            {
                const char current = source[cursor];
                if (current == '*' && cursor + 1 < source.size() && source[cursor + 1] == '/')
                {
                    advance(source[cursor++]);
                    advance(source[cursor++]);
                    break;
                }
                advance(current);
                ++cursor;
            }
            continue;
        }
        if (ch == '"' || ch == '\'')
        {
            const char delimiter = ch;
            advance(source[cursor++]);
            while (cursor < source.size())
            {
                const char current = source[cursor];
                advance(current);
                ++cursor;
                if (current == '\\' && cursor < source.size())
                {
                    advance(source[cursor]);
                    ++cursor;
                    continue;
                }
                if (current == delimiter) break;
            }
            continue;
        }

        const std::size_t begin = cursor;
        const std::uint32_t tokenLine = line;
        const std::uint32_t tokenColumn = column;
        bool identifier = false;
        bool number = false;
        if (identifierStart(ch))
        {
            identifier = true;
            do
            {
                advance(source[cursor]);
                ++cursor;
            } while (cursor < source.size() && identifierContinue(source[cursor]));
        }
        else if (std::isdigit(static_cast<unsigned char>(ch)) != 0)
        {
            number = true;
            do
            {
                advance(source[cursor]);
                ++cursor;
            } while (cursor < source.size() &&
                (std::isalnum(static_cast<unsigned char>(source[cursor])) != 0 ||
                    source[cursor] == '_' || source[cursor] == '.'));
        }
        else
        {
            advance(ch);
            ++cursor;
            if (cursor < source.size())
            {
                const std::string_view pair = source.substr(begin, 2);
                static constexpr std::array<std::string_view, 9> pairs = {
                    "::", "->", "==", "!=", "<=", ">=", "&&", "||", "??"};
                if (std::find(pairs.begin(), pairs.end(), pair) != pairs.end())
                {
                    advance(source[cursor]);
                    ++cursor;
                }
            }
        }
        result.push_back({source.substr(begin, cursor - begin),
            static_cast<std::uint32_t>(begin), tokenLine, tokenColumn, identifier, number});
    }
    return result;
}

std::unordered_map<std::uint32_t, std::unordered_set<std::string>> suppressions(
    std::string_view source)
{
    std::unordered_map<std::uint32_t, std::unordered_set<std::string>> result;
    std::uint32_t line = 1;
    std::size_t begin = 0;
    while (begin <= source.size())
    {
        const std::size_t end = source.find('\n', begin);
        const std::string_view text = source.substr(
            begin, end == std::string_view::npos ? source.size() - begin : end - begin);
        constexpr std::string_view marker = "nsc-migration:disable-next-line";
        const std::size_t markerOffset = text.find(marker);
        if (markerOffset != std::string_view::npos)
        {
            std::istringstream codes(std::string(text.substr(markerOffset + marker.size())));
            std::string code;
            while (codes >> code) result[line + 1].insert(std::move(code));
        }
        if (end == std::string_view::npos) break;
        begin = end + 1;
        ++line;
    }
    return result;
}

bool isSuppressed(
    const std::unordered_map<std::uint32_t, std::unordered_set<std::string>>& disabled,
    std::uint32_t line,
    language::MigrationIssue issue)
{
    const auto found = disabled.find(line);
    if (found == disabled.end()) return false;
    const std::string code(language::migrationDiagnosticCode(issue));
    return found->second.contains(code) || found->second.contains("all");
}

void addFinding(MigrationAnalysis& analysis,
    const std::unordered_map<std::uint32_t, std::unordered_set<std::string>>& disabled,
    const Token& token,
    language::MigrationIssue issue,
    std::string message,
    std::string alternative,
    std::optional<MigrationTextEdit> safeFix = std::nullopt)
{
    if (isSuppressed(disabled, token.line, issue)) return;
    analysis.findings.push_back({issue, token.offset,
        static_cast<std::uint32_t>(token.text.size()), token.line, token.column,
        std::move(message), std::move(alternative), std::move(safeFix)});
}

bool isNarrowIntegerType(std::string_view text) noexcept
{
    return text == "sbyte" || text == "byte" || text == "short" || text == "ushort";
}

std::optional<std::uint64_t> decimalValue(std::string_view text) noexcept
{
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

bool representableBy(std::string_view type, std::uint64_t value) noexcept
{
    if (type == "sbyte") return value <= 127;
    if (type == "byte") return value <= 255;
    if (type == "short") return value <= 32767;
    if (type == "ushort") return value <= 65535;
    return false;
}
}

MigrationAnalysis analyzeMigrationSource(
    std::filesystem::path sourcePath,
    std::string_view source)
{
    MigrationAnalysis analysis;
    analysis.sourcePath = std::move(sourcePath);
    if (source.substr(0, std::min<std::size_t>(source.size(), 512)).find(
            "nsc-migration:generated") != std::string_view::npos)
    {
        analysis.generatedSource = true;
        return analysis;
    }

    const auto disabled = suppressions(source);
    const std::vector<Token> tokens = tokenize(source);
    static const std::unordered_set<std::string_view> contextual = {
        "async", "await", "comptime", "derive", "dyn", "impl", "match", "opaque",
        "pin", "record", "some", "throws", "trait", "variant", "where", "yield"};
    static const std::unordered_set<std::string_view> declarationPredecessors = {
        "bool", "byte", "char", "class", "double", "enum", "float", "int", "interface",
        "long", "namespace", "nint", "nuint", "sbyte", "short", "string", "struct", "uint",
        "ulong", "ushort", "void"};

    for (std::size_t index = 0; index < tokens.size(); ++index)
    {
        const Token& token = tokens[index];
        const Token* previous = index == 0 ? nullptr : &tokens[index - 1];
        const Token* next = index + 1 < tokens.size() ? &tokens[index + 1] : nullptr;

        if (token.identifier && contextual.contains(token.text) && previous != nullptr &&
            (previous->identifier || declarationPredecessors.contains(previous->text)) &&
            next != nullptr && (next->text == ";" || next->text == "=" || next->text == "(" ||
                next->text == "," || next->text == ")" || next->text == "["))
        {
            addFinding(analysis, disabled, token, language::MigrationIssue::NewlyContextualWord,
                "identifier collides with a NativeSharp 3 contextual word",
                "rename the declaration and every bound reference");
        }

        if (token.identifier && next != nullptr && next->text == "(" &&
            index + 2 < tokens.size() && tokens[index + 2].text == "null")
        {
            addFinding(analysis, disabled, token, language::MigrationIssue::OverloadResolution,
                "a null argument can select a different overload under 3.0 nullability",
                "add an explicit nullable pointer/reference cast or choose the overload by name");
        }

        if (token.text == "owned" && index + 4 < tokens.size() &&
            tokens[index + 1].identifier && tokens[index + 2].identifier &&
            tokens[index + 3].text == "=" && tokens[index + 4].identifier &&
            (index + 5 >= tokens.size() || tokens[index + 5].text != "("))
        {
            addFinding(analysis, disabled, tokens[index + 4], language::MigrationIssue::CopyToMove,
                "initializing an owned value from a named place performs a move",
                "use move explicitly or clone through the type's declared copy protocol");
        }

        if (token.text == "null" && index >= 3 && tokens[index - 1].text == "=" &&
            tokens[index - 2].identifier && tokens[index - 3].text == "*")
        {
            addFinding(analysis, disabled, token, language::MigrationIssue::Nullability,
                "null initializes a pointer whose 3.0 nullability is not explicit",
                "declare the pointer nullable or replace null with a valid address");
        }

        if (token.text == "using" && next != nullptr && next->identifier)
        {
            addFinding(analysis, disabled, token, language::MigrationIssue::ImportLookup,
                "implicit using lookup must be reviewed against explicit imports",
                "replace it with explicit module, symbol, trait, or extension imports");
        }

        if (isNarrowIntegerType(token.text) && index + 3 < tokens.size() &&
            tokens[index + 1].identifier && tokens[index + 2].text == "=" &&
            tokens[index + 3].number)
        {
            std::optional<MigrationTextEdit> safeFix;
            const std::optional<std::uint64_t> value = decimalValue(tokens[index + 3].text);
            if (value.has_value() && representableBy(token.text, *value))
            {
                safeFix = MigrationTextEdit{tokens[index + 3].offset,
                    static_cast<std::uint32_t>(tokens[index + 3].text.size()),
                    "(" + std::string(token.text) + ")" + std::string(tokens[index + 3].text),
                    "the decimal constant is exactly representable by the declared type"};
            }
            addFinding(analysis, disabled, tokens[index + 3],
                language::MigrationIssue::NumericNarrowing,
                "implicit numeric conversion must be checked under the 3.0 range rules",
                "use a checked explicit conversion after proving the value is representable",
                std::move(safeFix));
        }

        if (token.text == "return")
        {
            const auto scopeBegin = index > 24 ? index - 24 : 0;
            const bool hasLifetimeObject = std::any_of(tokens.begin() +
                static_cast<std::ptrdiff_t>(scopeBegin), tokens.begin() +
                static_cast<std::ptrdiff_t>(index), [](const Token& candidate) {
                    return candidate.text == "raii" || candidate.text == "defer";
                });
            if (hasLifetimeObject)
            {
                addFinding(analysis, disabled, token, language::MigrationIssue::CleanupOrder,
                    "early return crosses deterministic cleanup introduced or reordered in 3.0",
                    "verify reverse declaration order and every defer capture before migrating");
            }
        }
    }

    std::sort(analysis.findings.begin(), analysis.findings.end(),
        [](const MigrationFinding& left, const MigrationFinding& right) {
            if (left.offset != right.offset) return left.offset < right.offset;
            return language::migrationDiagnosticCode(left.issue) <
                language::migrationDiagnosticCode(right.issue);
        });
    return analysis;
}

bool analyzeMigrationFile(
    const std::filesystem::path& sourcePath,
    MigrationAnalysis& analysis,
    std::string& error)
{
    error.clear();
    std::ifstream stream(sourcePath, std::ios::binary);
    if (!stream)
    {
        error = "cannot open migration source: " + sourcePath.string();
        return false;
    }
    const std::string source{std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    if (!stream.good() && !stream.eof())
    {
        error = "cannot read migration source: " + sourcePath.string();
        return false;
    }
    analysis = analyzeMigrationSource(sourcePath, source);
    return true;
}

std::string renderMigrationAnalysis(const MigrationAnalysis& analysis)
{
    std::ostringstream output;
    output << "NSMIGRATION-ANALYSIS 1\n"
           << "source=" << analysis.sourcePath.generic_string() << '\n'
           << "generated=" << (analysis.generatedSource ? "true" : "false") << '\n'
           << "findings=" << analysis.findings.size() << '\n';
    for (const MigrationFinding& finding : analysis.findings)
    {
        output << language::migrationDiagnosticCode(finding.issue) << ' '
               << finding.line << ':' << finding.column << ' '
               << finding.message << '\n'
               << "  alternative: " << finding.alternative << '\n';
        if (const DeprecatedFeature* feature = deprecatedFeature(finding.issue))
        {
            const DeprecationDecision decision =
                evaluateDeprecation(*feature, {3, 0}, false, false);
            output << "  deprecation: feature=" << feature->identity
                   << " state=" << deprecationStateName(decision.state)
                   << " since=" << feature->deprecatedSince.major << '.'
                   << feature->deprecatedSince.minor
                   << " removal-not-before=" << feature->removalNotBefore.major << '.'
                   << feature->removalNotBefore.minor << '\n';
        }
        if (finding.safeFix.has_value())
            output << "  safe-fix: " << finding.safeFix->replacement
                   << " proof=" << finding.safeFix->equivalenceProof << '\n';
    }
    return output.str();
}

bool applySafeMigrationFixes(
    std::string_view source,
    const MigrationAnalysis& analysis,
    std::string& migratedSource,
    std::string& error)
{
    error.clear();
    std::vector<MigrationTextEdit> edits;
    for (const MigrationFinding& finding : analysis.findings)
    {
        if (!finding.safeFix.has_value()) continue;
        const language::MigrationFix policyFix{finding.issue,
            finding.safeFix->replacement, !finding.safeFix->equivalenceProof.empty()};
        if (!language::mayApplyAutomaticFix(policyFix))
        {
            error = "migration edit lacks a semantic-equivalence proof";
            return false;
        }
        edits.push_back(*finding.safeFix);
    }
    std::sort(edits.begin(), edits.end(), [](const MigrationTextEdit& left,
        const MigrationTextEdit& right) { return left.offset < right.offset; });
    std::uint64_t previousEnd = 0;
    for (const MigrationTextEdit& edit : edits)
    {
        const std::uint64_t end = static_cast<std::uint64_t>(edit.offset) + edit.length;
        if (end > source.size() || edit.offset < previousEnd)
        {
            error = "migration edits are out of bounds or overlap";
            return false;
        }
        previousEnd = end;
    }

    migratedSource.assign(source);
    for (auto iterator = edits.rbegin(); iterator != edits.rend(); ++iterator)
        migratedSource.replace(iterator->offset, iterator->length, iterator->replacement);
    return true;
}

bool applySafeMigrationFixesToFile(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    MigrationAnalysis& analysis,
    std::size_t& appliedFixCount,
    std::string& error)
{
    appliedFixCount = 0;
    if (sourcePath == destinationPath)
    {
        error = "safe migration fixes require a distinct output path";
        return false;
    }
    std::ifstream stream(sourcePath, std::ios::binary);
    if (!stream)
    {
        error = "cannot open migration source: " + sourcePath.string();
        return false;
    }
    const std::string source{std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    if (!stream.good() && !stream.eof())
    {
        error = "cannot read migration source: " + sourcePath.string();
        return false;
    }
    analysis = analyzeMigrationSource(sourcePath, source);
    std::string migrated;
    if (!applySafeMigrationFixes(source, analysis, migrated, error)) return false;
    for (const MigrationFinding& finding : analysis.findings)
        if (finding.safeFix.has_value()) ++appliedFixCount;

    const std::filesystem::path parent = destinationPath.parent_path();
    std::error_code directoryError;
    if (!parent.empty()) std::filesystem::create_directories(parent, directoryError);
    if (directoryError)
    {
        error = "cannot create migration output directory: " + directoryError.message();
        return false;
    }
    std::ofstream destination(destinationPath, std::ios::binary | std::ios::trunc);
    if (!destination)
    {
        error = "cannot open migration output: " + destinationPath.string();
        return false;
    }
    destination.write(migrated.data(), static_cast<std::streamsize>(migrated.size()));
    if (!destination)
    {
        error = "cannot write migration output: " + destinationPath.string();
        return false;
    }
    return true;
}
}
