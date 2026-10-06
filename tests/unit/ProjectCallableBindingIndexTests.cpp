#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"

#include "../support/TestContext.h"

namespace
{
using nativesharp::SourceRange;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::frontend::ProjectCallableBindingIndex;
using nativesharp::tests::TestContext;

void testCompositeKeys(TestContext& tests)
{
    AstNode first(AstNodeKind::IdentifierExpression, SourceRange{});
    AstNode second(AstNodeKind::IdentifierExpression, SourceRange{});
    ProjectCallableBindingIndex index;

    index.record(7, &first, 11);
    index.record(8, &first, 12);
    index.record(7, &second, 13);

    tests.expect(index.size() == 3, "three distinct composite keys are indexed");
    tests.expect(index.latest(&first, 7) == 11, "first callable resolves its own binding");
    tests.expect(index.latest(&first, 8) == 12, "shared syntax does not collide across callables");
    tests.expect(index.latest(&second, 7) == 13, "shared callable does not collide across syntax nodes");
    tests.expect(!index.latest(&second, 8).has_value(), "an absent composite key remains absent");
}

void testNewestBindingWins(TestContext& tests)
{
    AstNode syntax(AstNodeKind::IdentifierExpression, SourceRange{});
    ProjectCallableBindingIndex index;

    index.record(3, &syntax, 21);
    index.record(3, &syntax, 22);

    tests.expect(index.size() == 1, "replacing one key does not grow the index");
    tests.expect(index.latest(&syntax, 3) == 22, "the newest binding replaces the earlier ordinal");
}

void testNullAndClear(TestContext& tests)
{
    AstNode syntax(AstNodeKind::IdentifierExpression, SourceRange{});
    ProjectCallableBindingIndex index;

    index.record(1, nullptr, 1);
    tests.expect(index.size() == 0, "null syntax is not indexed");
    tests.expect(!index.latest(nullptr, 1).has_value(), "null syntax cannot resolve a binding");

    index.record(1, &syntax, 4);
    index.clear();
    tests.expect(index.size() == 0, "clear removes every composite key");
    tests.expect(!index.latest(&syntax, 1).has_value(), "cleared bindings are no longer visible");
}
}

int main()
{
    TestContext tests;
    testCompositeKeys(tests);
    testNewestBindingWins(tests);
    testNullAndClear(tests);
    return tests.result();
}
