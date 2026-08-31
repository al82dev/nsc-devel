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
    ForStatement,
    ForeachStatement,
    BreakStatement,
    ContinueStatement,
    ReturnStatement,
    TryStatement,
    CatchClause,
    ThrowStatement,
    UsingStatement,
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
