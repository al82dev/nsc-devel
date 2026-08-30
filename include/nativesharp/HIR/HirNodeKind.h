#pragma once

#include <string_view>

namespace nativesharp::hir
{
enum class HirNodeKind
{
    CompilationUnit,
    StructDeclaration,
    FieldDeclaration,
    EnumDeclaration,
    EnumMemberDeclaration,
    GlobalVariableDeclaration,
    FunctionDeclaration,
    ParameterDeclaration,

    Block,
    LocalVariableDeclaration,
    If,
    While,
    For,
    Break,
    Continue,
    Return,
    ExpressionStatement,

    Constant,
    SymbolAddress,
    Load,
    Store,
    Conversion,
    Unary,
    Binary,
    Call,
    IndexAddress,
    FieldAddress,
    DereferenceAddress,
    ExtractField,
    CompoundAssignment,
    Increment,
    Allocation,
    Deallocation,
    StackAllocation
};

std::string_view hirNodeKindName(HirNodeKind kind) noexcept;
}
