#include "nativesharp/Frontend/MacroExpansionEngine.h"

#include <algorithm>
#include <unordered_map>

namespace nativesharp::frontend
{
namespace
{
struct Definition final
{
    std::vector<std::string> parameters;
    std::string body;
    MacroExpansionContext context = MacroExpansionContext::Statement;
    std::size_t definitionOffset = 0;
    std::size_t definitionLength = 0;
};

bool identifierStart(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}
bool identifierByte(char value) noexcept
{
    return identifierStart(value) || (value >= '0' && value <= '9');
}
bool space(char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}
void skipSpace(std::string_view source, std::size_t& cursor)
{
    while (cursor < source.size() && space(source[cursor])) ++cursor;
}
bool readIdentifier(std::string_view source, std::size_t& cursor, std::string& value)
{
    skipSpace(source, cursor);
    if (cursor >= source.size() || !identifierStart(source[cursor])) return false;
    const std::size_t begin = cursor++;
    while (cursor < source.size() && identifierByte(source[cursor])) ++cursor;
    value.assign(source.substr(begin, cursor - begin));
    return true;
}

std::size_t matching(
    std::string_view source,
    std::size_t open,
    char left,
    char right)
{
    std::size_t depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (std::size_t cursor = open; cursor < source.size(); ++cursor)
    {
        const char value = source[cursor];
        if (quoted)
        {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') quoted = false;
            continue;
        }
        if (value == '"') { quoted = true; continue; }
        if (value == left) ++depth;
        else if (value == right && --depth == 0) return cursor;
    }
    return std::string_view::npos;
}

std::vector<std::string> splitArguments(std::string_view text, bool& valid)
{
    std::vector<std::string> result;
    std::size_t begin = 0;
    std::size_t depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (std::size_t cursor = 0; cursor <= text.size(); ++cursor)
    {
        const char value = cursor < text.size() ? text[cursor] : ',';
        if (quoted)
        {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') quoted = false;
            continue;
        }
        if (value == '"') { quoted = true; continue; }
        if (value == '(' || value == '[' || value == '{') ++depth;
        else if (value == ')' || value == ']' || value == '}')
        {
            if (depth == 0) { valid = false; return {}; }
            --depth;
        }
        if (value != ',' || depth != 0) continue;
        std::string_view item = text.substr(begin, cursor - begin);
        while (!item.empty() && space(item.front())) item.remove_prefix(1);
        while (!item.empty() && space(item.back())) item.remove_suffix(1);
        result.emplace_back(item);
        begin = cursor + 1;
    }
    valid = !quoted && depth == 0;
    if (result.size() == 1 && result.front().empty()) result.clear();
    return result;
}

std::string instantiate(
    const Definition& definition,
    const std::vector<std::string>& arguments,
    std::size_t scope,
    std::string& error)
{
    std::unordered_map<std::string, std::string_view> values;
    for (std::size_t index = 0; index < arguments.size(); ++index)
        values.emplace(definition.parameters[index], arguments[index]);
    std::string output;
    for (std::size_t cursor = 0; cursor < definition.body.size();)
    {
        if (definition.body[cursor] != '$')
        {
            output.push_back(definition.body[cursor++]);
            continue;
        }
        ++cursor;
        const bool hygienic = cursor < definition.body.size() && definition.body[cursor] == '#';
        if (hygienic) ++cursor;
        const std::size_t begin = cursor;
        while (cursor < definition.body.size() && identifierByte(definition.body[cursor])) ++cursor;
        if (begin == cursor)
        {
            error = "macro '$' marker requires an identifier";
            return {};
        }
        const std::string name = definition.body.substr(begin, cursor - begin);
        if (hygienic)
        {
            output += "__nsc_macro_" + std::to_string(scope) + "_" + name;
            continue;
        }
        const auto found = values.find(name);
        if (found == values.end())
        {
            error = "macro body refers to undeclared parameter '$" + name + "'";
            return {};
        }
        output.append(found->second);
    }
    return output;
}
}

MacroExpansionEngine::MacroExpansionEngine(std::size_t maximumExpansions) noexcept
    : maximumExpansions_(maximumExpansions) {}

MacroExpansionResult MacroExpansionEngine::expand(std::string_view source) const
{
    MacroExpansionResult result;
    if (source.find("__nsc_macro_") != std::string_view::npos)
    {
        result.error = "identifier prefix '__nsc_macro_' is reserved for hygienic expansion";
        return result;
    }
    result.source.assign(source);
    std::unordered_map<std::string, Definition> definitions;

    std::size_t search = 0;
    while ((search = result.source.find("#macro", search)) != std::string::npos)
    {
        std::size_t cursor = search + 6;
        std::string name;
        if (!readIdentifier(result.source, cursor, name))
        { result.error = "macro definition requires a name"; return result; }
        skipSpace(result.source, cursor);
        if (cursor >= result.source.size() || result.source[cursor] != '(')
        { result.error = "macro definition requires a parameter list"; return result; }
        const std::size_t parametersEnd = matching(result.source, cursor, '(', ')');
        if (parametersEnd == std::string::npos)
        { result.error = "unterminated macro parameter list"; return result; }
        bool valid = false;
        std::vector<std::string> parameters = splitArguments(
            std::string_view(result.source).substr(cursor + 1, parametersEnd - cursor - 1), valid);
        if (!valid || std::ranges::any_of(parameters, [](const std::string& parameter) {
                return parameter.empty() || !identifierStart(parameter.front()) ||
                    !std::ranges::all_of(parameter, identifierByte);
            }))
        { result.error = "macro parameters must be identifiers"; return result; }
        std::ranges::sort(parameters);
        if (std::ranges::adjacent_find(parameters) != parameters.end())
        { result.error = "macro parameter names must be unique"; return result; }
        // Restore declared order after uniqueness validation.
        parameters = splitArguments(
            std::string_view(result.source).substr(cursor + 1, parametersEnd - cursor - 1), valid);
        cursor = parametersEnd + 1;
        skipSpace(result.source, cursor);
        MacroExpansionContext context = MacroExpansionContext::Statement;
        if (cursor < result.source.size() && result.source[cursor] != '{')
        {
            std::string keyword;
            if (!readIdentifier(result.source, cursor, keyword))
            { result.error = "macro context requires an identifier"; return result; }
            if (keyword == "statement") context = MacroExpansionContext::Statement;
            else if (keyword == "expression") context = MacroExpansionContext::Expression;
            else if (keyword == "member") context = MacroExpansionContext::Member;
            else if (keyword == "type") context = MacroExpansionContext::Type;
            else
            { result.error = "macro context must be statement, expression, member, or type"; return result; }
        }
        skipSpace(result.source, cursor);
        if (cursor >= result.source.size() || result.source[cursor] != '{')
        { result.error = "macro definition requires a token-tree body"; return result; }
        const std::size_t bodyEnd = matching(result.source, cursor, '{', '}');
        if (bodyEnd == std::string::npos)
        { result.error = "unterminated macro body"; return result; }
        if (!definitions.emplace(name, Definition{std::move(parameters),
                result.source.substr(cursor + 1, bodyEnd - cursor - 1), context,
                search, bodyEnd + 1 - search}).second)
        { result.error = "duplicate macro definition '" + name + "'"; return result; }
        for (std::size_t index = search; index <= bodyEnd; ++index)
            if (result.source[index] != '\r' && result.source[index] != '\n') result.source[index] = ' ';
        search = bodyEnd + 1;
    }

    std::size_t scope = 0;
    while (true)
    {
        const std::size_t invocation = result.source.find("#expand");
        if (invocation == std::string::npos) break;
        if (result.expansions.size() >= maximumExpansions_)
        {
            result.error = "macro expansion limit exceeded\ncomptime backtrace:";
            const std::size_t first = result.expansions.size() > 8
                ? result.expansions.size() - 8 : 0;
            for (std::size_t index = result.expansions.size(); index > first; --index)
                result.error += "\n  #expand " + result.expansions[index - 1].macroName;
            return result;
        }
        std::size_t cursor = invocation + 7;
        std::string name;
        if (!readIdentifier(result.source, cursor, name))
        { result.error = "macro expansion requires a name"; return result; }
        const auto definition = definitions.find(name);
        if (definition == definitions.end())
        { result.error = "unknown macro '" + name + "'"; return result; }
        skipSpace(result.source, cursor);
        if (cursor >= result.source.size() || result.source[cursor] != '(')
        { result.error = "macro expansion requires an argument list"; return result; }
        const std::size_t end = matching(result.source, cursor, '(', ')');
        if (end == std::string::npos)
        { result.error = "unterminated macro invocation"; return result; }
        bool valid = false;
        const std::vector<std::string> arguments = splitArguments(
            std::string_view(result.source).substr(cursor + 1, end - cursor - 1), valid);
        if (!valid || arguments.size() != definition->second.parameters.size())
        { result.error = "macro argument count does not match definition '" + name + "'"; return result; }
        ++scope;
        std::string error;
        std::string expansion = instantiate(definition->second, arguments, scope, error);
        if (!error.empty()) { result.error = error; return result; }
        const std::size_t invocationLength = end + 1 - invocation;
        result.source.replace(invocation, invocationLength, expansion);
        result.expansions.push_back({name, invocation, invocationLength, expansion.size(),
            scope, definition->second.context,
            definition->second.definitionOffset, definition->second.definitionLength});
    }
    result.success = true;
    return result;
}

std::string macroExpansionBacktrace(std::span<const MacroExpansionRecord> expansions)
{
    std::string result;
    for (std::size_t index = 0; index < expansions.size(); ++index)
    {
        const MacroExpansionRecord& expansion = expansions[index];
        result += "  #expand " + expansion.macroName;
        result += " (invocation offset " + std::to_string(expansion.invocationOffset);
        result += ", template offset " + std::to_string(expansion.definitionOffset) + ")";
        if (index + 1 != expansions.size()) result += "\n";
    }
    return result;
}
}
