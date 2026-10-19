#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
std::optional<std::string> expressionQualifiedName(const ast::AstNode* expression)
{
    std::vector<std::string_view> members;
    std::size_t resultSize = 0;
    const ast::AstNode* current = expression;
    while (current != nullptr && current->kind() == ast::AstNodeKind::MemberExpression)
    {
        if (current->first() == nullptr || current->text().empty()) return std::nullopt;
        members.push_back(current->text());
        resultSize += current->text().size() + 1U;
        current = current->first();
    }
    if (current == nullptr || current->kind() != ast::AstNodeKind::IdentifierExpression ||
        current->text().empty())
    {
        return std::nullopt;
    }

    resultSize += current->text().size();
    std::string result;
    result.reserve(resultSize);
    result.append(current->text());
    for (auto iterator = members.rbegin(); iterator != members.rend(); ++iterator)
    {
        result.push_back('.');
        result.append(*iterator);
    }
    return result;
}
}

void ProjectNameLookup::bindExpressionTypeReceiver(const ProjectSyntaxUnit& unit, const ast::AstNode* expression,
                                                   const std::vector<std::string>& namespaceSegments,
                                                   const LookupEnvironment& environment, UnitBindingIndex& index)
{
    if (expression == nullptr || index.expressionTypeBindingBySyntax.contains(expression))
    {
        return;
    }

    const std::optional<std::string> writtenName = expressionQualifiedName(expression);
    if (!writtenName.has_value())
    {
        return;
    }
    std::vector<std::size_t> candidates =
        resolveExpressionTypeCandidates(unit.projectOrdinal(), namespaceSegments, environment, *writtenName);
    if (candidates.empty())
    {
        return;
    }

    const std::size_t ordinal = index.expressionTypeBindings.size();
    index.expressionTypeBindings.push_back(
        ExpressionTypeNameBinding{expression, *writtenName, std::move(candidates), environment.enclosingTypeOrdinals});
    index.expressionTypeBindingBySyntax.emplace(expression, ordinal);
}

}
