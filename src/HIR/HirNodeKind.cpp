#include "nativesharp/HIR/HirNodeKind.h"

namespace nativesharp::hir
{
std::string_view hirNodeKindName(HirNodeKind kind) noexcept
{
    switch (kind)
    {
        case HirNodeKind::CompilationUnit: return "CompilationUnit";
        case HirNodeKind::StructDeclaration: return "StructDeclaration";
        case HirNodeKind::FieldDeclaration: return "FieldDeclaration";
        case HirNodeKind::EnumDeclaration: return "EnumDeclaration";
        case HirNodeKind::EnumMemberDeclaration: return "EnumMemberDeclaration";
        case HirNodeKind::GlobalVariableDeclaration: return "GlobalVariableDeclaration";
        case HirNodeKind::FunctionDeclaration: return "FunctionDeclaration";
        case HirNodeKind::ParameterDeclaration: return "ParameterDeclaration";
        case HirNodeKind::Block: return "Block";
        case HirNodeKind::LocalVariableDeclaration: return "LocalVariableDeclaration";
        case HirNodeKind::If: return "If";
        case HirNodeKind::While: return "While";
        case HirNodeKind::For: return "For";
        case HirNodeKind::Break: return "Break";
        case HirNodeKind::Continue: return "Continue";
        case HirNodeKind::Return: return "Return";
        case HirNodeKind::ExpressionStatement: return "ExpressionStatement";
        case HirNodeKind::Constant: return "Constant";
        case HirNodeKind::SymbolAddress: return "SymbolAddress";
        case HirNodeKind::Load: return "Load";
        case HirNodeKind::Store: return "Store";
        case HirNodeKind::Conversion: return "Conversion";
        case HirNodeKind::Unary: return "Unary";
        case HirNodeKind::Binary: return "Binary";
        case HirNodeKind::Conditional: return "Conditional";
        case HirNodeKind::Call: return "Call";
        case HirNodeKind::IndexAddress: return "IndexAddress";
        case HirNodeKind::FieldAddress: return "FieldAddress";
        case HirNodeKind::DereferenceAddress: return "DereferenceAddress";
        case HirNodeKind::ExtractField: return "ExtractField";
        case HirNodeKind::CompoundAssignment: return "CompoundAssignment";
        case HirNodeKind::Increment: return "Increment";
        case HirNodeKind::Allocation: return "Allocation";
        case HirNodeKind::Deallocation: return "Deallocation";
        case HirNodeKind::StackAllocation: return "StackAllocation";
    }
    return "Unknown";
}
}
