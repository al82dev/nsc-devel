#pragma once

#include <string_view>

namespace nativesharp::ast
{
enum class AstNodeKind
{
    CompilationUnit,
    NamespaceDeclaration,
    UsingNamespaceDirective,
    UsingAliasDirective,
    FunctionDeclaration,
    ParameterDeclaration,
    GlobalVariableDeclaration,
    StructDeclaration,
    ClassDeclaration,
    InterfaceDeclaration,
    DelegateDeclaration,
    FieldDeclaration,
    MethodDeclaration,
    ConstructorDeclaration,
    DestructorDeclaration,
    PropertyDeclaration,
    IndexerDeclaration,
    EventDeclaration,
    BaseConstructorInitializer,
    ThisConstructorInitializer,
    EnumDeclaration,
    EnumMemberDeclaration,
    ExternCDeclaration,

    BlockStatement,
    VariableDeclarationStatement,
    IfStatement,
    WhileStatement,
    DoWhileStatement,
    ForStatement,
    ForeachStatement,
    // `switch (governing) { case a: case b: ...; default: ...; }`
    //
    // SwitchStatement: first() is the governing expression; children() are the sections in
    // source order.
    // SwitchSection:   children() are the labels first, then the statements. A label is a
    //                  SwitchLabel node; the statements are anything parseStatement produces.
    //                  The split point is stored so the two runs never have to be
    //                  distinguished by kind.
    // SwitchLabel:     first() is the constant expression, or nullptr for `default`.
    //                  flag() is true for `default`.
    //
    // There is NO fall-through: a section that finishes leaves the switch. See doc 72.
    SwitchStatement,
    SwitchSection,
    SwitchLabel,
    BreakStatement,
    ContinueStatement,
    ReturnStatement,
    TryStatement,
    CatchClause,
    ThrowStatement,
    UsingStatement,
    StaticAssertStatement,
    // `asm { ... }`. Item 287 records the structural assembly block and its canonical token
    // spelling. Assembly-specific tokens and per-instruction locations are introduced by item
    // 288; operands and executable semantics are deliberately later layers.
    InlineAssemblyStatement,
    // One non-blank physical line inside InlineAssemblyStatement. text() is the exact
    // non-whitespace spelling and range() begins at its first significant source byte.
    InlineAssemblyLine,
    // A positional `in(expression : "constraint")` operand. first() is the NativeSharp
    // expression, integerValue() is its zero-based input ordinal, auxiliaryText() is the
    // normalized operand constraint, and flag() records whether it was written explicitly.
    InlineAssemblyInputOperand,
    // A positional `out(target : "constraint")` operand. first() is a writable NativeSharp
    // lvalue, integerValue() is its zero-based `$outN` ordinal, auxiliaryText() is the normalized
    // operand constraint, and flag() records whether it was written explicitly.
    InlineAssemblyOutputOperand,
    // One canonical name from `clobber("name", ...)`. text() is lowercase, integerValue() is the
    // stable source ordinal, auxiliaryText() classifies it as register, flags, or memory, and
    // flag() marks a Windows x64 nonvolatile register that the backend must preserve.
    InlineAssemblyClobber,
    ExpressionStatement,

    IdentifierExpression,
    IntegerLiteralExpression,
    FloatingLiteralExpression,
    CharacterLiteralExpression,
    StringLiteralExpression,
    BooleanLiteralExpression,
    NullLiteralExpression,
    ThisExpression,
    BaseExpression,
    UnaryExpression,
    BinaryExpression,
    // `condition ? whenTrue : whenFalse`. first() is the condition, second() the true
    // branch, third() the false branch. Both branches are lazily evaluated, like `&&`.
    ConditionalExpression,
    AssignmentExpression,
    CallExpression,
    IndexExpression,
    MemberExpression,
    PointerMemberExpression,
    PostfixExpression,
    CastExpression,
    TypeTestExpression,
    SafeCastExpression,
    LambdaExpression,
    SizeofTypeExpression,
    SizeofExpression,
    AlignofTypeExpression,
    AlignofExpression,
    OffsetofExpression,
    DefaultValueExpression,
    NewExpression,
    ObjectCreationExpression,
    DeleteExpression,
    StackallocExpression,
    MoveExpression,
    // C7 "aggregate initializer": `V v = {1, 2};` -- the braced form, with the elements as
    // children in declaration order and nested braces for nested aggregates.
    AggregateInitializerExpression
};

std::string_view astNodeKindName(AstNodeKind kind) noexcept;
}
