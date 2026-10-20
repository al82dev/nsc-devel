#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"

namespace nativesharp::frontend
{
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
