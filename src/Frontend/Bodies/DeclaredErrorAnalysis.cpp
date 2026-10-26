#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <string>
#include <vector>

// EN: Freeze 3.0 C-13: handle-or-propagate for `throws E`. A call, a delegate invocation, a `new`
//     and a `this(...)`/`base(...)` initializer that reach a callable declaring `throws E` must be
//     inside a try whose unfiltered catch accepts E, or in a callable whose own `throws` is E or a
//     base of E. A filtered catch does not count: its filter may decline the error.
// ES: Congelamiento 3.0 C-13: manejar o propagar para `throws E`. Una llamada, una invocacion de
//     delegate, un `new` y un inicializador `this(...)`/`base(...)` que llegan a un callable con
//     `throws E` deben estar dentro de un try cuyo catch sin filtro acepte E, o en un callable cuyo
//     propio `throws` sea E o una base de E. Un catch con filtro no cuenta: su filtro puede rechazar
//     el error.
namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kAnyException = "<any-exception>";
}

bool ProjectMethodBodyModel::declaredErrorIsCovered(
    std::string_view errorTypeKey,
    std::span<const std::string> handlers,
    std::string_view enclosingErrorTypeKey) const
{
    if (errorTypeKey.empty()) return true;
    const auto accepts = [&](std::string_view handler)
    {
        if (handler == kAnyException || handler == errorTypeKey) return true;
        if (inheritanceModel_ == nullptr || handler.empty()) return false;
        const std::optional<std::size_t> error =
            inheritanceModel_->typeOrdinalForTypeKey(std::string(errorTypeKey));
        const std::optional<std::size_t> base =
            inheritanceModel_->typeOrdinalForTypeKey(std::string(handler));
        return error.has_value() && base.has_value() &&
            inheritanceModel_->isDerivedFrom(*error, *base, true);
    };
    for (const std::string& handler : handlers)
    {
        if (accepts(handler)) return true;
    }
    return !enclosingErrorTypeKey.empty() && accepts(enclosingErrorTypeKey);
}

std::vector<std::string> ProjectMethodBodyModel::activeErrorHandlers() const
{
    std::vector<std::string> handlers;
    for (const std::vector<std::string>& frame : activeErrorHandlers_)
        handlers.insert(handlers.end(), frame.begin(), frame.end());
    return handlers;
}

std::string ProjectMethodBodyModel::enclosingErrorTypeKey(std::size_t methodMemberOrdinal) const
{
    if (currentLambdaOrdinal_.has_value()) return currentLambdaErrorTypeKey_;
    const ClassMemberSymbol* callable = methodSymbol(methodMemberOrdinal);
    return callable == nullptr ? std::string{} : callable->declaredErrorTypeKey;
}

std::vector<std::string> ProjectMethodBodyModel::tryErrorHandlers(const ast::AstNode* statement) const
{
    std::vector<std::string> handlers;
    if (statement == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr) return handlers;
    for (const ast::AstNode* clause : statement->children())
    {
        if (clause == nullptr || clause->kind() != ast::AstNodeKind::CatchClause) continue;
        if (clause->second() != nullptr) continue;
        handlers.push_back(clause->flag()
            ? std::string(kAnyException)
            : memberModel_->canonicalTypeKey(clause->typeSyntax(), *nameLookup_));
    }
    return handlers;
}

void ProjectMethodBodyModel::requireDeclaredErrorHandled(
    const ast::AstNode* site,
    std::string_view errorTypeKey,
    std::string_view calleeName,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (site == nullptr || errorTypeKey.empty() || inheritanceModel_ == nullptr) return;
    // EN: Only an exception class can be caught; a typed generator error such as `int` is a value
    //     the consumer reads, not a throw.
    // ES: Solo una clase de excepcion se puede atrapar; un error tipado de generator como `int` es
    //     un valor que el consumidor lee, no un throw.
    const std::optional<std::size_t> errorOrdinal =
        inheritanceModel_->typeOrdinalForTypeKey(std::string(errorTypeKey));
    if (!errorOrdinal.has_value() || !isExceptionTypeOrdinal(*errorOrdinal)) return;
    const std::vector<std::string> handlers = activeErrorHandlers();
    if (declaredErrorIsCovered(errorTypeKey, handlers, enclosingErrorTypeKey(methodMemberOrdinal)))
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::UnhandledDeclaredError,
        diagnostics::DiagnosticSeverity::Error,
        site->range(),
        "'" + std::string(calleeName) + "' throws '" + std::string(errorTypeKey) +
            "', which is neither caught here nor declared by the enclosing callable; catch it "
            "or declare a compatible 'throws'");
}
}
