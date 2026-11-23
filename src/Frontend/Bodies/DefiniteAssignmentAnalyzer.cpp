#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include <functional>
#include <set>

namespace nativesharp::frontend
{
namespace
{
// The direct operands of a node: first/second/third and the children, each once. / Los operandos directos.
void forEachOperand(const ast::AstNode* node, const std::function<void(const ast::AstNode*)>& visit)
{
    if (node == nullptr) return;
    std::set<const ast::AstNode*> seen;
    for (const ast::AstNode* operand : {node->first(), node->second(), node->third()})
        if (operand != nullptr && seen.insert(operand).second) visit(operand);
    for (const ast::AstNode* child : node->children())
        if (child != nullptr && seen.insert(child).second) visit(child);
}

bool namesLocal(const ast::AstNode* node, const std::string& name)
{
    return node != nullptr && node->kind() == ast::AstNodeKind::IdentifierExpression && node->text() == name;
}

bool mentionsLocal(const ast::AstNode* node, const std::string& name)
{
    if (node == nullptr) return false;
    if (namesLocal(node, name)) return true;
    bool found = false;
    forEachOperand(node, [&](const ast::AstNode* operand) { found = found || mentionsLocal(operand, name); });
    return found;
}
}

// EN: R1-04. Flow over the structured statements that follow the declaration of a local initialized by a call with
//     the project contract Result="Owned" Release="f". The local is consumed when it is passed to f or to a Transfer
//     parameter, returned, assigned to other storage, used to initialize another local or captured by a lambda
//     (the last three hand it over: the receiver is then responsible). It must be consumed on every path that leaves
//     its block normally and before every `return` that does not return it. Both branches of an `if`, every `switch`
//     section and the `try` block with each `catch` must consume it; a loop may run zero times, so a release inside
//     it does not count after it. A `throw` leaves through an exception, as a C caller would. `break`, `continue`
//     and `goto` stop the check for that local (no diagnostic) rather than guessing.
// ES: R1-04. Flujo sobre las sentencias estructuradas que siguen a la declaracion de un local inicializado con una
//     llamada con el contrato Result="Owned" Release="f". El local se consume al pasarlo a f o a un parametro
//     Transfer, devolverlo, asignarlo a otro almacenamiento, inicializar otro local con el o capturarlo en una lambda
//     (en los tres ultimos el receptor queda a cargo). Debe consumirse en todo camino que sale de su bloque
//     normalmente y antes de cada `return` que no lo devuelve. Ambas ramas de un `if`, cada seccion de `switch` y el
//     `try` con cada `catch` deben consumirlo; un bucle puede no ejecutarse, asi que una liberacion dentro no cuenta
//     despues. Un `throw` sale por excepcion, como en C. `break`, `continue` y `goto` detienen la comprobacion de
//     ese local (sin diagnostico) en lugar de adivinar.
void ProjectMethodBodyModel::checkOwnedResultsReleased(
    const ast::AstNode* body, diagnostics::DiagnosticEngine& diagnostics) const
{
    if (body == nullptr || namespaceFunctionModel_ == nullptr) return;
    const auto functions = namespaceFunctionModel_->functions();
    // Without an owned contract there is nothing to track. / Sin contrato con dueno no hay nada que seguir.
    if (std::none_of(functions.begin(), functions.end(),
            [](const NamespaceFunctionSymbol& function) { return !function.cppOwnedRelease.empty(); })) return;
    const auto calledFunction = [&](const ast::AstNode* node) -> const NamespaceFunctionSymbol* {
        const auto* call = node != nullptr && node->kind() == ast::AstNodeKind::CallExpression
            ? namespaceFunctionCall(node) : nullptr;
        return call && call->selectedFunctionOrdinal && *call->selectedFunctionOrdinal < functions.size()
            ? &functions[*call->selectedFunctionOrdinal] : nullptr;
    };
    struct Tracked { std::string name; std::string release; const ast::AstNode* declaration; };
    struct State { bool consumed = false; bool reachable = true; bool abandoned = false; };
    // An expression that consumes the local. / Una expresion que consume el local.
    const std::function<bool(const ast::AstNode*, const Tracked&)> consumes =
        [&](const ast::AstNode* node, const Tracked& local) -> bool {
        if (node == nullptr) return false;
        if (node->kind() == ast::AstNodeKind::LambdaExpression) return mentionsLocal(node, local.name);
        if (node->kind() == ast::AstNodeKind::AssignmentExpression && namesLocal(node->second(), local.name))
            return true;
        if (const auto* function = calledFunction(node))
        {
            const auto& arguments = node->children();
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                if (!namesLocal(arguments[i], local.name)) continue;
                if (function->name == local.release) return true;
                if (i < function->parameters.size() && function->parameters[i].cppTransfer) return true;
            }
        }
        bool found = false;
        forEachOperand(node, [&](const ast::AstNode* operand) { found = found || consumes(operand, local); });
        return found;
    };
    const auto report = [&](const ast::AstNode* at, const Tracked& local, std::string_view where) {
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall, diagnostics::DiagnosticSeverity::Error,
            at->range(), "the owned C/C++ result in '" + local.name + "' (project contract Result=\"Owned\") is not "
            "released " + std::string(where) + ": release it with '" + local.release +
            "', transfer it, return it or store it on every path");
    };
    const std::function<State(const ast::AstNode*, const Tracked&, State)> flow =
        [&](const ast::AstNode* statement, const Tracked& local, State state) -> State {
        if (statement == nullptr || !state.reachable || state.abandoned) return state;
        const auto merge = [](State left, const State& right) {
            if (left.abandoned || right.abandoned) { left.abandoned = true; return left; }
            if (!left.reachable) return right;
            if (!right.reachable) return left;
            left.consumed = left.consumed && right.consumed;
            return left;
        };
        switch (statement->kind())
        {
            case ast::AstNodeKind::BlockStatement:
                for (const ast::AstNode* child : statement->children()) state = flow(child, local, state);
                return state;
            case ast::AstNodeKind::IfStatement:
            {
                if (consumes(statement->first(), local)) state.consumed = true;
                const State taken = flow(statement->second(), local, state);
                const State other = statement->third() ? flow(statement->third(), local, state) : state;
                return merge(taken, other);
            }
            case ast::AstNodeKind::WhileStatement:
            case ast::AstNodeKind::DoWhileStatement:
            case ast::AstNodeKind::ForStatement:
            case ast::AstNodeKind::ForeachStatement:
            {
                State inside = state;
                forEachOperand(statement, [&](const ast::AstNode* part) { inside = flow(part, local, inside); });
                if (inside.abandoned) return inside;
                return state;
            }
            case ast::AstNodeKind::SwitchStatement:
            {
                if (consumes(statement->first(), local)) state.consumed = true;
                State joined{state.consumed, false, false};
                bool hasDefault = false;
                for (const ast::AstNode* section : statement->children())
                {
                    if (section == nullptr) continue;
                    State path = state;
                    for (const ast::AstNode* item : section->children())
                    {
                        if (item != nullptr && item->kind() == ast::AstNodeKind::SwitchLabel)
                        {
                            hasDefault = hasDefault || item->flag();
                            continue;
                        }
                        path = flow(item, local, path);
                    }
                    joined = merge(joined, path);
                }
                return hasDefault ? joined : merge(joined, state);
            }
            case ast::AstNodeKind::TryStatement:
            {
                State joined{state.consumed, false, false};
                const ast::AstNode* finallyBlock = nullptr;
                forEachOperand(statement, [&](const ast::AstNode* part) {
                    if (part->kind() == ast::AstNodeKind::CatchClause || part == statement->first())
                        joined = merge(joined, flow(part, local, state));
                    else if (part->kind() == ast::AstNodeKind::BlockStatement) finallyBlock = part;
                });
                return finallyBlock ? flow(finallyBlock, local, joined) : joined;
            }
            case ast::AstNodeKind::CatchClause:
            {
                forEachOperand(statement, [&](const ast::AstNode* part) {
                    if (part->kind() == ast::AstNodeKind::BlockStatement) state = flow(part, local, state);
                });
                return state;
            }
            case ast::AstNodeKind::ReturnStatement:
                if (!state.consumed && !mentionsLocal(statement->first(), local.name) &&
                    !consumes(statement->first(), local))
                    report(statement, local, "before this return");
                state.reachable = false;
                return state;
            case ast::AstNodeKind::ThrowStatement:
                state.reachable = false;
                return state;
            case ast::AstNodeKind::BreakStatement:
            case ast::AstNodeKind::ContinueStatement:
                state.abandoned = true;
                return state;
            case ast::AstNodeKind::VariableDeclarationStatement:
                if (namesLocal(statement->first(), local.name) || consumes(statement->first(), local))
                    state.consumed = true;
                return state;
            default:
                if (consumes(statement, local)) state.consumed = true;
                return state;
        }
    };
    // Declarations directly inside a block: the statements after them are the local's scope.
    // Declaraciones directamente dentro de un bloque: las sentencias siguientes son el ambito del local.
    const std::function<void(const ast::AstNode*)> visit = [&](const ast::AstNode* node) {
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) return;
        if (node->kind() == ast::AstNodeKind::BlockStatement)
        {
            const auto& statements = node->children();
            for (std::size_t i = 0; i < statements.size(); ++i)
            {
                const ast::AstNode* declaration = statements[i];
                const auto* function = declaration != nullptr &&
                    declaration->kind() == ast::AstNodeKind::VariableDeclarationStatement
                    ? calledFunction(declaration->first()) : nullptr;
                if (function == nullptr || function->cppOwnedRelease.empty()) continue;
                const Tracked local{declaration->text(), function->cppOwnedRelease, declaration};
                State state;
                for (std::size_t j = i + 1; j < statements.size(); ++j) state = flow(statements[j], local, state);
                if (!state.abandoned && state.reachable && !state.consumed)
                    report(declaration, local, "before the end of its block");
            }
        }
        forEachOperand(node, visit);
    };
    visit(body);
}

bool ProjectMethodBodyModel::definiteAssignmentTracksLocal(std::size_t localOrdinal) const noexcept
{
    if (localOrdinal >= locals_.size())
    {
        return false;
    }
    const MethodLocalSymbol& local = locals_[localOrdinal];
    if (local.typeKey.empty() || local.typeKey == "<error>")
    {
        return false;
    }
    return !typeKeyHasDefinedDefaultStorage(local.typeKey);
}

// Walks an assignment target down to the identifier the write is rooted at, exactly the way
// verifyOutParameterAssignments walks one: `x = v`, `x.left = 0`, `x.rows[2].a = 1` and `*x = v`
// are all writes rooted at `x`. Returns nullptr when the target is not rooted at a plain name.
const ast::AstNode* ProjectMethodBodyModel::definiteAssignmentTargetRoot(
    const ast::AstNode* target) const noexcept
{
    while (target != nullptr && target->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        switch (target->kind())
        {
            case ast::AstNodeKind::MemberExpression:
            case ast::AstNodeKind::IndexExpression:
            case ast::AstNodeKind::UnaryExpression:
                target = target->first();
                continue;
            default:
                return nullptr;
        }
    }
    return target;
}

// Records that writing `target` assigns the local it is rooted at, and -- when the whole local is
// the target -- suppresses the read diagnostic for that one identifier node, since an assignment
// analyzes its own left-hand side through the ordinary expression path.
//
// The distinction between the two cases is the entire subtlety here. Writing the WHOLE local
// (`x = v`) always assigns it. Writing THROUGH it only assigns it when the local is the storage
// being written -- an aggregate. `T[] items; items[0] = 1;` writes through a reference that was
// never given a value, so `items` stays unassigned and the read of it is still reported; that is
// a null dereference, and treating the store as an assignment would hide it. Aggregates are not
// tracked at all (see typeKeyHasDefinedDefaultStorage), so for them this is a no-op either way.
void ProjectMethodBodyModel::markAssignmentTarget(const ast::AstNode* target, std::size_t scopeOrdinal)
{
    const ast::AstNode* root = definiteAssignmentTargetRoot(target);
    if (root == nullptr)
    {
        return;
    }
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
    if (!name.has_value() || name->kind != ScopeNameKind::Local)
    {
        return;
    }
    if (root != target)
    {
        return;
    }
    assignedLocals_.insert(name->semanticOrdinal);
    definiteAssignmentWriteTarget_ = root;
}

void ProjectMethodBodyModel::checkDefiniteAssignment(
    const ast::AstNode* expression,
    std::size_t localOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (expression == nullptr || expression == definiteAssignmentWriteTarget_ ||
        inUnevaluatedOperand_) return;
    if (!definiteAssignmentTracksLocal(localOrdinal)) return;
    if (assignedLocals_.contains(localOrdinal)) return;
    if (!reportedUnassignedReads_.insert(expression).second) return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "use of unassigned local variable '" + locals_[localOrdinal].name + "'");
    // Treat it as assigned from here on. The program is already rejected; repeating the same
    // message at every later use of the same variable only buries the first one.
    assignedLocals_.insert(localOrdinal);
}

// ---------------------------------------------------------------------------------------------
// C7 "aggregate initializer"
//
// The element list an aggregate target expects, in the order a braced initializer fills it:
// N copies of T for a fixed array `T[N]`, and the instance fields in declaration order for a
// struct. A union is the one shape that is not a list: C initializes its FIRST member and nothing
// else, and so does this, which is why it reports its own arity instead of the field count.
//
// Returns nullopt when the type is not an aggregate at all -- the caller turns that into the
// "requires a struct or fixed-array target" diagnostic.
}
