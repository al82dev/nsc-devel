#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <limits>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using lexer::TokenKind;
using target::BuiltinLayoutKind;
using target::TypeLayout;

std::optional<BuiltinLayoutKind> builtinLayoutKind(TokenKind kind) noexcept
{
    switch (kind)
    {
        case TokenKind::KwBool: return BuiltinLayoutKind::Bool;
        case TokenKind::KwSByte: return BuiltinLayoutKind::SByte;
        case TokenKind::KwByte: return BuiltinLayoutKind::Byte;
        case TokenKind::KwChar: return BuiltinLayoutKind::Char;
        case TokenKind::KwShort: return BuiltinLayoutKind::Short;
        case TokenKind::KwUShort: return BuiltinLayoutKind::UShort;
        case TokenKind::KwInt: return BuiltinLayoutKind::Int;
        case TokenKind::KwUInt: return BuiltinLayoutKind::UInt;
        case TokenKind::KwLong: return BuiltinLayoutKind::Long;
        case TokenKind::KwULong: return BuiltinLayoutKind::ULong;
        case TokenKind::KwNInt: return BuiltinLayoutKind::NInt;
        case TokenKind::KwNUInt: return BuiltinLayoutKind::NUInt;
        case TokenKind::KwFloat: return BuiltinLayoutKind::Float;
        case TokenKind::KwDouble: return BuiltinLayoutKind::Double;
        case TokenKind::KwString: return BuiltinLayoutKind::Pointer;
        default: return std::nullopt;
    }
}

std::optional<std::uint64_t> checkedAdd(std::uint64_t left, std::uint64_t right) noexcept
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left)
    {
        return std::nullopt;
    }
    return left + right;
}

std::optional<std::uint64_t> checkedAlignUp(std::uint64_t value, std::uint64_t alignment) noexcept
{
    if (alignment == 0)
    {
        return std::nullopt;
    }
    const std::uint64_t remainder = value % alignment;
    if (remainder == 0)
    {
        return value;
    }
    return checkedAdd(value, alignment - remainder);
}
}

bool ProjectObjectLayoutModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectConstructionModel& constructionModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_)
    {
        return !diagnostics.hasErrors();
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() ||
        !constructionModel.isBuilt())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            SourceRange(),
            "project object layout requires completed name, member, inheritance and construction models");
        built_ = true;
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    inheritanceModel_ = &inheritanceModel;
    constructionModel_ = &constructionModel;
    structLayoutIndexByType_.resize(nameLookup.namedTypes().size());
    classLayoutIndexByType_.resize(nameLookup.namedTypes().size());
    namedTypeLayouts_.resize(nameLookup.namedTypes().size(), TypeLayout{0, 1});
    namedLayoutStates_.resize(nameLookup.namedTypes().size(), NamedLayoutState::Unvisited);

    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        if (nameLookup.namedTypes()[typeOrdinal].kind == NamedTypeDeclarationKind::Struct)
        {
            buildStructLayout(typeOrdinal, diagnostics, nameLookup.namedTypes()[typeOrdinal].range);
        }
    }

    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        if (nameLookup.namedTypes()[typeOrdinal].kind == NamedTypeDeclarationKind::Class)
        {
            buildClassLayout(typeOrdinal, diagnostics);
        }
    }

    if (!diagnostics.hasErrors())
    {
        buildAllocationPlans();
    }
    built_ = true;
    return !diagnostics.hasErrors();
}

std::span<const ClassObjectLayout> ProjectObjectLayoutModel::classLayouts() const noexcept
{
    return classLayouts_;
}

std::span<const StructValueLayout> ProjectObjectLayoutModel::structLayouts() const noexcept
{
    return structLayouts_;
}

std::span<const ObjectAllocationPlan> ProjectObjectLayoutModel::allocationPlans() const noexcept
{
    return allocationPlans_;
}

const ClassObjectLayout* ProjectObjectLayoutModel::layoutForType(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= classLayoutIndexByType_.size() || !classLayoutIndexByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *classLayoutIndexByType_[typeOrdinal];
    return index < classLayouts_.size() ? &classLayouts_[index] : nullptr;
}

const StructValueLayout* ProjectObjectLayoutModel::structLayoutForType(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= structLayoutIndexByType_.size() || !structLayoutIndexByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *structLayoutIndexByType_[typeOrdinal];
    return index < structLayouts_.size() ? &structLayouts_[index] : nullptr;
}

const ObjectFieldLayout* ProjectObjectLayoutModel::fieldLayoutForMember(std::size_t memberOrdinal) const noexcept
{
    for (const StructValueLayout& layout : structLayouts_)
    {
        for (const ObjectFieldLayout& field : layout.fields)
        {
            if (field.memberOrdinal == memberOrdinal) return &field;
        }
    }
    for (const ClassObjectLayout& layout : classLayouts_)
    {
        for (const ObjectFieldLayout& field : layout.fields)
        {
            if (field.memberOrdinal == memberOrdinal) return &field;
        }
    }
    return nullptr;
}

target::TypeLayout ProjectObjectLayoutModel::classReferenceLayout() const noexcept
{
    return targetLayout_.layoutOf(BuiltinLayoutKind::Pointer);
}

std::optional<std::pair<std::uint64_t, std::uint64_t>>
ProjectObjectLayoutModel::valueLayoutForTypeKey(
    const ProjectNameLookup& nameLookup,
    std::string_view typeKey) const
{
    if (typeKey == "builtin:bool" || typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
        typeKey == "builtin:char") return {{1, 1}};
    if (typeKey == "builtin:short" || typeKey == "builtin:ushort") return {{2, 2}};
    if (typeKey == "builtin:int" || typeKey == "builtin:uint" || typeKey == "builtin:float") return {{4, 4}};
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" || typeKey == "builtin:nint" ||
        typeKey == "builtin:nuint" || typeKey == "builtin:double" || typeKey == "builtin:string" ||
        ProjectTypeConversion::isNativeArray(typeKey) || typeKey.ends_with("*")) return {{8, 8}};
    // C2.1 "fixed arrays": inline storage, so its size is the element stride times the count --
    // not a pointer. This is what lets a `float[4]` FIELD be measured the same way a `float[4]`
    // local is.
    if (const std::optional<std::string> element =
            ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        element.has_value())
    {
        const std::optional<std::uint64_t> count = ProjectMemberModel::fixedArrayCount(typeKey);
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout =
            valueLayoutForTypeKey(nameLookup, *element);
        if (!count.has_value() || !elementLayout.has_value()) return std::nullopt;
        return {{elementLayout->first * *count, elementLayout->second}};
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup.namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& declaration = nameLookup.namedTypes()[ordinal];
        if ("type:" + declaration.identity.key() != typeKey) continue;
        if (declaration.kind == NamedTypeDeclarationKind::Struct)
        {
            const StructValueLayout* layout = structLayoutForType(ordinal);
            if (layout != nullptr) return {{layout->size, layout->alignment}};
            return std::nullopt;
        }
        // ID 78, P1 -- UN `enum` NO MIDE OCHO.
        //
        // Esta funcion es el servicio canonico de tamano del nivel crudo: de ella salen el paso de
        // un arreglo, `sizeof`, `stackalloc` y el paso de la aritmetica de punteros. Un `enum` caia
        // hasta el `return {{8, 8}}` de abajo, que existe para las REFERENCIAS -- clase, interfaz,
        // delegado -- porque todas ellas miden un puntero. Pero un enum es un tipo POR VALOR, y la
        // otra funcion de tamano de esta misma clase, `layoutNamedType`, contesta 4 para el mismo
        // tipo (BuiltinLayoutKind::Enum).
        //
        // MEDIDO, y la consecuencia no es cosmetica: el offset de un campo `enum` dentro de un
        // struct lo da layoutNamedType (4), y el paso de un `Color*` lo daba esta (8). Con
        // `struct S { Color a; Color b; }`, `Color* p = &s.a; p + 1` aterrizaba OCHO bytes mas
        // alla -- pasado `s.b` por completo, fuera del struct. Dos reglas de tamano para un mismo
        // tipo es exactamente la duplicacion semantica que el milestone CC1 existe para quitar; el
        // enum era la que quedaba viva.
        if (declaration.kind == NamedTypeDeclarationKind::Enum)
        {
            const target::TypeLayout layout = targetLayout_.layoutOf(target::BuiltinLayoutKind::Enum);
            return {{layout.size, layout.alignment}};
        }
        // Clase, interfaz y delegado: una referencia, que es un puntero nativo.
        return {{8, 8}};
    }
    return std::nullopt;
}

bool ProjectObjectLayoutModel::isBuilt() const noexcept
{
    return built_;
}

std::optional<TypeLayout> ProjectObjectLayoutModel::layoutTypeSyntax(
    const ast::TypeSyntax* syntax,
    diagnostics::DiagnosticEngine& diagnostics,
    SourceRange useRange)
{
    if (syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            useRange,
            "object layout cannot compute a missing field type");
        return std::nullopt;
    }

    const auto& modifiers = syntax->modifiers();
    TypeLayout current{0, 1};
    std::size_t modifierIndex = 0;

    if (!modifiers.empty() && modifiers.front().kind == ast::TypeModifierSyntaxKind::Pointer)
    {
        current = targetLayout_.layoutOf(BuiltinLayoutKind::Pointer);
        modifierIndex = 1;
    }
    else
    {
        const std::optional<TypeLayout> base = layoutBaseType(syntax, diagnostics, useRange);
        if (!base.has_value())
        {
            return std::nullopt;
        }
        current = *base;
    }

    for (; modifierIndex < modifiers.size(); ++modifierIndex)
    {
        const ast::TypeModifierSyntax& modifier = modifiers[modifierIndex];
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer ||
            modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray)
        {
            current = targetLayout_.layoutOf(BuiltinLayoutKind::Pointer);
            continue;
        }

        const std::optional<std::uint64_t> count = evaluateArrayCount(modifier.arrayCount);
        if (!count.has_value() || *count == 0)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArrayCount,
                diagnostics::DiagnosticSeverity::Error,
                modifier.range,
                "class/struct field array count must be a positive compile-time integer expression in Stage 29");
            return std::nullopt;
        }
        const std::optional<target::ArrayLayout> array = targetLayout_.layoutArray(current, *count);
        if (!array.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArrayCount,
                diagnostics::DiagnosticSeverity::Error,
                modifier.range,
                "field array layout overflows the Windows x64 object-size model");
            return std::nullopt;
        }
        current = TypeLayout{array->size, array->alignment};
    }
    return current;
}

std::optional<TypeLayout> ProjectObjectLayoutModel::layoutBaseType(
    const ast::TypeSyntax* syntax,
    diagnostics::DiagnosticEngine& diagnostics,
    SourceRange useRange)
{
    if (syntax == nullptr)
    {
        return std::nullopt;
    }
    if (const std::optional<BuiltinLayoutKind> builtin = builtinLayoutKind(syntax->baseTokenKind()); builtin.has_value())
    {
        return targetLayout_.layoutOf(*builtin);
    }
    if (syntax->baseTokenKind() == TokenKind::KwVoid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            useRange,
            "bare void has no object field layout");
        return std::nullopt;
    }
    if (nameLookup_ == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::size_t> ordinal = nameLookup_->resolvedTypeOrdinal(syntax);
    if (!ordinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownTypeName,
            diagnostics::DiagnosticSeverity::Error,
            useRange,
            "field type has no resolved named-type layout");
        return std::nullopt;
    }
    return layoutNamedType(*ordinal, diagnostics, useRange);
}

std::optional<TypeLayout> ProjectObjectLayoutModel::layoutNamedType(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    SourceRange useRange)
{
    if (nameLookup_ == nullptr || typeOrdinal >= nameLookup_->namedTypes().size() ||
        typeOrdinal >= namedLayoutStates_.size())
    {
        return std::nullopt;
    }

    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[typeOrdinal];
    if (type.kind == NamedTypeDeclarationKind::Class ||
        type.kind == NamedTypeDeclarationKind::Interface ||
        type.kind == NamedTypeDeclarationKind::Delegate)
    {
        // Stage 36 keeps both class and interface references as one native pointer.
        return targetLayout_.layoutOf(BuiltinLayoutKind::Pointer);
    }
    if (type.kind == NamedTypeDeclarationKind::Enum)
    {
        return targetLayout_.layoutOf(BuiltinLayoutKind::Enum);
    }
    if (type.kind != NamedTypeDeclarationKind::Struct)
    {
        return std::nullopt;
    }

    if (namedLayoutStates_[typeOrdinal] == NamedLayoutState::Complete)
    {
        return namedTypeLayouts_[typeOrdinal];
    }
    if (namedLayoutStates_[typeOrdinal] == NamedLayoutState::Failed)
    {
        return std::nullopt;
    }
    if (!buildStructLayout(typeOrdinal, diagnostics, useRange))
    {
        return std::nullopt;
    }
    return namedTypeLayouts_[typeOrdinal];
}

namespace
{
// C2.2 "union and explicit layout": read back the layout markers the parser wrote onto a
// struct/union declaration -- "union", "pack=N", "align=N", comma-joined in any order. Anything
// unrecognized is ignored rather than fatal: a marker this build does not know about must not
// turn a valid declaration into a hard error.
target::StructLayoutOptions parseStructLayoutOptions(const NamedTypeDeclaration& type)
{
    target::StructLayoutOptions options;
    if (type.syntax == nullptr) return options;
    const std::string& text = type.syntax->auxiliaryText();
    std::size_t position = 0;
    while (position < text.size())
    {
        const std::size_t comma = text.find(',', position);
        const std::string_view token(
            text.data() + position,
            (comma == std::string::npos ? text.size() : comma) - position);
        position = (comma == std::string::npos ? text.size() : comma + 1);
        if (token == "union") { options.isUnion = true; continue; }
        const auto numericSuffix = [&](std::string_view prefix) -> std::optional<std::uint64_t>
        {
            if (!token.starts_with(prefix)) return std::nullopt;
            std::uint64_t value = 0;
            for (const char digit : token.substr(prefix.size()))
            {
                if (std::isdigit(static_cast<unsigned char>(digit)) == 0) return std::nullopt;
                value = value * 10U + static_cast<std::uint64_t>(digit - '0');
            }
            return value;
        };
        if (const auto pack = numericSuffix("pack="); pack.has_value()) { options.pack = *pack; continue; }
        if (const auto align = numericSuffix("align="); align.has_value()) options.minimumAlignment = *align;
    }
    return options;
}
}

bool ProjectObjectLayoutModel::buildStructLayout(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    SourceRange useRange)
{
    if (nameLookup_ == nullptr || memberModel_ == nullptr ||
        typeOrdinal >= nameLookup_->namedTypes().size() ||
        typeOrdinal >= namedLayoutStates_.size() ||
        typeOrdinal >= structLayoutIndexByType_.size())
    {
        return false;
    }
    if (namedLayoutStates_[typeOrdinal] == NamedLayoutState::Complete)
    {
        return structLayoutIndexByType_[typeOrdinal].has_value();
    }
    if (namedLayoutStates_[typeOrdinal] == NamedLayoutState::Failed)
    {
        return false;
    }
    if (namedLayoutStates_[typeOrdinal] == NamedLayoutState::Visiting)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::IncompleteType,
            diagnostics::DiagnosticSeverity::Error,
            useRange,
            "recursive by-value struct field makes type '" +
                nameLookup_->namedTypes()[typeOrdinal].identity.fullyQualifiedName() + "' incomplete");
        namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
        return false;
    }

    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[typeOrdinal];
    if (type.kind != NamedTypeDeclarationKind::Struct)
    {
        return false;
    }

    namedLayoutStates_[typeOrdinal] = NamedLayoutState::Visiting;
    std::vector<target::StructFieldRequest> physicalFields;
    std::vector<std::size_t> fieldMembers;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
        const std::optional<TypeLayout> fieldLayout = layoutTypeSyntax(
            member.declaredTypeSyntax, diagnostics, member.range);
        if (!fieldLayout.has_value())
        {
            namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
            return false;
        }
        physicalFields.push_back(
            target::StructFieldRequest{*fieldLayout, member.explicitFieldOffset});
        fieldMembers.push_back(memberOrdinal);
    }

    // C2.2 "union and explicit layout": this is THE one place in the compiler that knows a union
    // is not an ordinary struct. Everything else -- members, by-value semantics, the C ABI
    // classification, the [N x i8] LLVM carrier, destruction -- treats the two identically,
    // because in every respect except where the fields sit, they ARE identical. The markers were
    // written onto the declaration by the parser (`union`, `pack=N`, `align=N`).
    const target::StructLayoutOptions layoutOptions = parseStructLayoutOptions(type);
    const std::optional<target::StructLayout> layout =
        targetLayout_.layoutStruct(physicalFields, layoutOptions);
    if (!layout.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            useRange,
            "struct layout overflows the Windows x64 value-size model for '" +
                type.identity.fullyQualifiedName() + "'");
        namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
        return false;
    }

    StructValueLayout valueLayout;
    valueLayout.typeOrdinal = typeOrdinal;
    valueLayout.typeKey = "type:" + type.identity.key();
    valueLayout.size = layout->size;
    valueLayout.alignment = layout->alignment;
    valueLayout.isUnion = layoutOptions.isUnion;
    valueLayout.pack = layoutOptions.pack;
    valueLayout.minimumAlignment = layoutOptions.minimumAlignment;
    for (const target::StructFieldRequest& request : physicalFields)
    {
        if (request.explicitOffset.has_value()) valueLayout.hasExplicitFieldOffsets = true;
    }
    for (std::size_t index = 0; index < fieldMembers.size(); ++index)
    {
        const std::size_t memberOrdinal = fieldMembers[index];
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        const target::StructFieldLayout& field = layout->fields[index];
        valueLayout.fields.push_back(ObjectFieldLayout{
            memberOrdinal,
            field.offset,
            field.size,
            field.alignment,
            member.declaredTypeKey});
    }

    namedTypeLayouts_[typeOrdinal] = TypeLayout{layout->size, layout->alignment};
    structLayoutIndexByType_[typeOrdinal] = structLayouts_.size();
    structLayouts_.push_back(std::move(valueLayout));
    namedLayoutStates_[typeOrdinal] = NamedLayoutState::Complete;
    return true;
}

std::optional<std::uint64_t> ProjectObjectLayoutModel::evaluateArrayCount(const ast::AstNode* expression) const noexcept
{
    // C2.1 "fixed arrays": delegated to ProjectMemberModel::evaluateConstantArrayCount so the
    // compiler has exactly ONE definition of what counts as a constant array count. The canonical
    // type key (which now carries the count, "builtin:float[4]") and this layout must agree about
    // it -- if they diverged, a field's key and its computed size would describe different arrays.
    return ProjectMemberModel::evaluateConstantArrayCount(expression);
}

bool ProjectObjectLayoutModel::buildClassLayout(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        typeOrdinal >= nameLookup_->namedTypes().size() || typeOrdinal >= classLayoutIndexByType_.size())
    {
        return false;
    }
    if (classLayoutIndexByType_[typeOrdinal].has_value())
    {
        return true;
    }

    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[typeOrdinal];
    const TypeLayout reference = classReferenceLayout();
    const std::optional<std::size_t> baseType = inheritanceModel_->baseTypeOrdinal(typeOrdinal);

    std::uint64_t offset = reference.size;
    std::uint64_t aggregateAlignment = reference.alignment;
    std::uint64_t baseSubobjectSize = 0;
    if (baseType.has_value())
    {
        if (!buildClassLayout(*baseType, diagnostics))
        {
            return false;
        }
        const ClassObjectLayout* baseLayout = layoutForType(*baseType);
        if (baseLayout == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                type.range,
                "derived class layout cannot locate completed base-class layout");
            return false;
        }
        offset = baseLayout->size;
        aggregateAlignment = std::max(aggregateAlignment, baseLayout->alignment);
        baseSubobjectSize = baseLayout->size;
    }

    ClassObjectLayout layout;
    layout.typeOrdinal = typeOrdinal;
    layout.typeKey = "type:" + type.identity.key();
    layout.headerSize = reference.size;
    layout.baseTypeOrdinal = baseType;
    layout.baseSubobjectSize = baseSubobjectSize;
    layout.referenceSize = reference.size;
    layout.referenceAlignment = reference.alignment;

    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Field || member.isStatic)
        {
            continue;
        }

        const std::optional<TypeLayout> fieldLayout = layoutTypeSyntax(
            member.declaredTypeSyntax,
            diagnostics,
            member.range);
        if (!fieldLayout.has_value())
        {
            return false;
        }
        const std::optional<std::uint64_t> alignedOffset = checkedAlignUp(offset, fieldLayout->alignment);
        if (!alignedOffset.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "class field offset overflows the Windows x64 object-size model");
            return false;
        }
        const std::optional<std::uint64_t> afterField = checkedAdd(*alignedOffset, fieldLayout->size);
        if (!afterField.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "class field extent overflows the Windows x64 object-size model");
            return false;
        }

        layout.fields.push_back(ObjectFieldLayout{
            memberOrdinal,
            *alignedOffset,
            fieldLayout->size,
            fieldLayout->alignment,
            member.declaredTypeKey});
        offset = *afterField;
        aggregateAlignment = std::max(aggregateAlignment, fieldLayout->alignment);
    }

    const std::optional<std::uint64_t> finalSize = checkedAlignUp(offset, aggregateAlignment);
    if (!finalSize.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "class object layout overflows the Windows x64 object-size model for '" +
                type.identity.fullyQualifiedName() + "'");
        return false;
    }
    layout.size = *finalSize;
    layout.alignment = aggregateAlignment;

    const std::size_t layoutIndex = classLayouts_.size();
    classLayouts_.push_back(std::move(layout));
    classLayoutIndexByType_[typeOrdinal] = layoutIndex;
    return true;
}

void ProjectObjectLayoutModel::buildAllocationPlans()
{
    if (constructionModel_ == nullptr)
    {
        return;
    }
    for (std::size_t ordinal = 0; ordinal < constructionModel_->constructions().size(); ++ordinal)
    {
        const ObjectConstructionBinding& construction = constructionModel_->constructions()[ordinal];
        if (construction.resolution != ConstructionResolutionKind::ResolvedExplicit &&
            construction.resolution != ConstructionResolutionKind::ResolvedImplicitDefault)
        {
            continue;
        }
        const ClassObjectLayout* layout = layoutForType(construction.targetTypeOrdinal);
        if (layout == nullptr)
        {
            continue;
        }

        ObjectAllocationPlan plan;
        plan.constructionOrdinal = ordinal;
        plan.targetTypeOrdinal = construction.targetTypeOrdinal;
        plan.targetTypeKey = construction.targetTypeKey;
        plan.objectSize = layout->size;
        plan.objectAlignment = layout->alignment;
        plan.referenceSize = layout->referenceSize;
        plan.referenceAlignment = layout->referenceAlignment;
        plan.selectedImplicitDefault = construction.selectedImplicitDefault;
        plan.selectedConstructorMemberOrdinal = construction.selectedConstructorMemberOrdinal;
        plan.constructorEntryMemberOrdinal = construction.selectedConstructorMemberOrdinal.has_value()
            ? construction.selectedConstructorMemberOrdinal
            : construction.implicitDefaultBaseConstructorMemberOrdinal;
        plan.initializationSteps.push_back({ObjectInitializationStepKind::RequestStorage, std::nullopt});
        plan.initializationSteps.push_back({ObjectInitializationStepKind::CheckAllocationResult, std::nullopt});
        plan.initializationSteps.push_back({ObjectInitializationStepKind::BeginObjectLifetime, std::nullopt});
        plan.initializationSteps.push_back({ObjectInitializationStepKind::ZeroInitializeStorage, std::nullopt});
        plan.initializationSteps.push_back({ObjectInitializationStepKind::InitializeObjectInfrastructure, std::nullopt});
        if (plan.constructorEntryMemberOrdinal.has_value())
        {
            plan.initializationSteps.push_back({
                ObjectInitializationStepKind::InvokeConstructor,
                plan.constructorEntryMemberOrdinal});
        }
        plan.initializationSteps.push_back({ObjectInitializationStepKind::ReturnReference, std::nullopt});
        allocationPlans_.push_back(std::move(plan));
    }
}

const char* objectInitializationStepKindName(ObjectInitializationStepKind kind) noexcept
{
    switch (kind)
    {
        case ObjectInitializationStepKind::RequestStorage: return "request-storage";
        case ObjectInitializationStepKind::CheckAllocationResult: return "check-allocation-result";
        case ObjectInitializationStepKind::BeginObjectLifetime: return "begin-object-lifetime";
        case ObjectInitializationStepKind::ZeroInitializeStorage: return "zero-initialize-storage";
        case ObjectInitializationStepKind::InitializeObjectInfrastructure: return "initialize-object-infrastructure";
        case ObjectInitializationStepKind::InvokeConstructor: return "invoke-constructor";
        case ObjectInitializationStepKind::ReturnReference: return "return-reference";
    }
    return "invalid-step";
}
}
