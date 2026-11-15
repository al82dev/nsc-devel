#include "nativesharp/Frontend/MacroExpansionEngine.h"
#include "Internal/Definition.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
using internal::Definition;

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

// EN: LKF-383: the directives were found with a plain text search, so `#macro` or `#expand` inside a
//     string or character literal or a comment was taken as a directive: `"#expand f(1)"` failed with
//     "unknown macro" and a commented-out definition was registered. Only code text counts now.
//     Strings (also verbatim `@"..."` and raw `"""..."""`), characters and both comment forms are
//     skipped.
// ES: LKF-383: las directivas se buscaban con una búsqueda de texto simple, así que `#macro` o
//     `#expand` dentro de un literal de cadena o de carácter o de un comentario se tomaba por una
//     directiva: `"#expand f(1)"` fallaba con "unknown macro" y una definición comentada se
//     registraba. Ahora solo cuenta el texto de código. Se saltan las cadenas (también las verbatim
//     `@"..."` y las raw `"""..."""`), los caracteres y las dos formas de comentario.
std::size_t findDirective(const std::string& source, std::string_view directive, std::size_t from)
{
    std::size_t index = 0;
    while (index < source.size())
    {
        const char current = source[index];
        const char next = index + 1 < source.size() ? source[index + 1] : '\0';
        if (current == '/' && next == '/')
        {
            const std::size_t end = source.find('\n', index);
            if (end == std::string::npos) return std::string::npos;
            index = end + 1;
            continue;
        }
        if (current == '/' && next == '*')
        {
            const std::size_t end = source.find("*/", index + 2);
            if (end == std::string::npos) return std::string::npos;
            index = end + 2;
            continue;
        }
        if (current == '"' && source.compare(index, 3, "\"\"\"") == 0)
        {
            const std::size_t end = source.find("\"\"\"", index + 3);
            if (end == std::string::npos) return std::string::npos;
            index = end + 3;
            continue;
        }
        if (current == '"' || current == '\'')
        {
            const bool verbatim = current == '"' && index > 0 && source[index - 1] == '@';
            std::size_t cursor = index + 1;
            while (cursor < source.size())
            {
                if (!verbatim && source[cursor] == '\\') { cursor += 2; continue; }
                if (source[cursor] == current)
                {
                    if (verbatim && cursor + 1 < source.size() && source[cursor + 1] == '"') { cursor += 2; continue; }
                    break;
                }
                if (!verbatim && source[cursor] == '\n') break;
                ++cursor;
            }
            index = cursor + 1;
            continue;
        }
        if (index >= from && source.compare(index, directive.size(), directive) == 0) return index;
        ++index;
    }
    return std::string::npos;
}

bool collectMacroDefinitions(
    MacroExpansionResult& result,
    std::unordered_map<std::string, Definition>& definitions)
{
    std::size_t search = 0;
    while ((search = findDirective(result.source, "#macro", search)) != std::string::npos)
    {
        std::size_t cursor = search + 6;
        std::string name;
        if (!readIdentifier(result.source, cursor, name))
        { result.error = "macro definition requires a name"; return false; }
        skipSpace(result.source, cursor);
        if (cursor >= result.source.size() || result.source[cursor] != '(')
        { result.error = "macro definition requires a parameter list"; return false; }
        const std::size_t parametersEnd = matching(result.source, cursor, '(', ')');
        if (parametersEnd == std::string::npos)
        { result.error = "unterminated macro parameter list"; return false; }
        bool valid = false;
        std::vector<std::string> parameters = splitArguments(
            std::string_view(result.source).substr(cursor + 1, parametersEnd - cursor - 1), valid);
        if (!valid || std::ranges::any_of(parameters, [](const std::string& parameter) {
                return parameter.empty() || !identifierStart(parameter.front()) ||
                    !std::ranges::all_of(parameter, identifierByte);
            }))
        { result.error = "macro parameters must be identifiers"; return false; }
        std::ranges::sort(parameters);
        if (std::ranges::adjacent_find(parameters) != parameters.end())
        { result.error = "macro parameter names must be unique"; return false; }
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
            { result.error = "macro context requires an identifier"; return false; }
            if (keyword == "statement") context = MacroExpansionContext::Statement;
            else if (keyword == "expression") context = MacroExpansionContext::Expression;
            else if (keyword == "member") context = MacroExpansionContext::Member;
            else if (keyword == "type") context = MacroExpansionContext::Type;
            else
            { result.error = "macro context must be statement, expression, member, or type"; return false; }
        }
        skipSpace(result.source, cursor);
        if (cursor >= result.source.size() || result.source[cursor] != '{')
        { result.error = "macro definition requires a token-tree body"; return false; }
        const std::size_t bodyEnd = matching(result.source, cursor, '{', '}');
        if (bodyEnd == std::string::npos)
        { result.error = "unterminated macro body"; return false; }
        if (!definitions.emplace(name, Definition{std::move(parameters),
                result.source.substr(cursor + 1, bodyEnd - cursor - 1), context,
                search, bodyEnd + 1 - search}).second)
        { result.error = "duplicate macro definition '" + name + "'"; return false; }
        for (std::size_t index = search; index <= bodyEnd; ++index)
            if (result.source[index] != '\r' && result.source[index] != '\n') result.source[index] = ' ';
        search = bodyEnd + 1;
    }
    return true;
}

bool expandMacroInvocations(
    MacroExpansionResult& result,
    const std::unordered_map<std::string, Definition>& definitions,
    std::size_t maximumExpansions,
    std::size_t maximumDepth)
{
    std::size_t scope = 0;
    // EN: LKF-382: `scope` numbers the expansions for hygiene and was also compared with the depth
    //     limit, so 33 independent invocations failed with "macro expansion depth limit exceeded"
    //     although none was nested. The depth of an invocation is now one more than the depth of the
    //     innermost expansion whose text contains it.
    // ES: LKF-382: `scope` numera las expansiones para la higiene y también se comparaba con el
    //     límite de profundidad, así que 33 invocaciones independientes fallaban con "macro expansion
    //     depth limit exceeded" aunque ninguna estaba anidada. La profundidad de una invocación es ahora
    //     uno más que la de la expansión más interna cuyo texto la contiene.
    struct ExpandedRegion
    {
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t depth = 0;
    };
    std::vector<ExpandedRegion> regions;
    while (true)
    {
        const std::size_t invocation = findDirective(result.source, "#expand", 0);
        if (invocation == std::string::npos) break;
        if (result.expansions.size() >= maximumExpansions)
        {
            result.error = "macro expansion limit exceeded\ncomptime backtrace:";
            const std::size_t first = result.expansions.size() > 8
                ? result.expansions.size() - 8 : 0;
            for (std::size_t index = result.expansions.size(); index > first; --index)
                result.error += "\n  #expand " + result.expansions[index - 1].macroName;
            return false;
        }
        std::size_t cursor = invocation + 7;
        std::string name;
        if (!readIdentifier(result.source, cursor, name))
        { result.error = "macro expansion requires a name"; return false; }
        const auto definition = definitions.find(name);
        if (definition == definitions.end())
        { result.error = "unknown macro '" + name + "'"; return false; }
        skipSpace(result.source, cursor);
        if (cursor >= result.source.size() || result.source[cursor] != '(')
        { result.error = "macro expansion requires an argument list"; return false; }
        const std::size_t end = matching(result.source, cursor, '(', ')');
        if (end == std::string::npos)
        { result.error = "unterminated macro invocation"; return false; }
        bool valid = false;
        const std::vector<std::string> arguments = splitArguments(
            std::string_view(result.source).substr(cursor + 1, end - cursor - 1), valid);
        if (!valid || arguments.size() != definition->second.parameters.size())
        { result.error = "macro argument count does not match definition '" + name + "'"; return false; }
        ++scope;
        std::size_t depth = 1;
        for (const ExpandedRegion& region : regions)
            if (region.begin <= invocation && invocation < region.end) depth = std::max(depth, region.depth + 1);
        if (depth > maximumDepth)
        { result.error = "macro expansion depth limit exceeded"; return false; }
        std::string error;
        std::string expansion = instantiate(definition->second, arguments, scope, error);
        if (!error.empty()) { result.error = error; return false; }
        // NS304-T14-F07: a macro whose body expands itself is a symbol cycle, diagnosed
        // specifically instead of being absorbed by the step budget.
        if (expansion.find("#expand " + name) != std::string::npos)
        { result.error = "macro cycle detected: '" + name + "' expands to itself"; return false; }
        const std::size_t invocationLength = end + 1 - invocation;
        result.source.replace(invocation, invocationLength, expansion);
        for (ExpandedRegion& region : regions)
        {
            if (region.end <= invocation) continue;
            if (region.begin > invocation) region.begin = region.begin + expansion.size() - invocationLength;
            region.end = region.end + expansion.size() - invocationLength;
        }
        regions.push_back({invocation, invocation + expansion.size(), depth});
        result.expansions.push_back({name, invocation, invocationLength, expansion.size(),
            scope, definition->second.context,
            definition->second.definitionOffset, definition->second.definitionLength});
    }
    return true;
}
}

MacroExpansionEngine::MacroExpansionEngine(
    std::size_t maximumExpansions,
    std::size_t maximumDepth) noexcept
    : maximumExpansions_(maximumExpansions), maximumDepth_(maximumDepth) {}

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
    if (!collectMacroDefinitions(result, definitions)) return result;
    if (!expandMacroInvocations(
            result, definitions, maximumExpansions_, maximumDepth_)) return result;
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
