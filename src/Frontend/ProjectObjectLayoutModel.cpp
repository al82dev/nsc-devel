#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <limits>
#include <functional>
#include <set>
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
        buildPromotedFieldLayouts(diagnostics);
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
    for (const ObjectFieldLayout& field : promotedFieldLayouts_)
    {
        if (field.memberOrdinal == memberOrdinal) return &field;
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
    if (const std::optional<language::AggregateLayout> tuple =
            tupleLayoutForTypeKey(nameLookup, typeKey);
        tuple.has_value())
        return {{tuple->size, tuple->alignment}};
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
            // Mismo motivo que en el otro sitio: el ancho del enum es el de su tipo subyacente.
            target::TypeLayout layout = targetLayout_.layoutOf(target::BuiltinLayoutKind::Enum);
            if (const builtins::IntegerTypeInfo* info =
                    builtins::findIntegerType(enumUnderlyingTypeKey(declaration.syntax));
                info != nullptr && info->width >= 8)
            {
                layout.size = info->width / 8U;
                layout.alignment = layout.size;
            }
            return {{layout.size, layout.alignment}};
        }
        // Clase, interfaz y delegado: una referencia, que es un puntero nativo.
        return {{8, 8}};
    }
    return std::nullopt;
}

std::optional<language::AggregateLayout> ProjectObjectLayoutModel::tupleLayoutForTypeKey(
    const ProjectNameLookup& nameLookup,
    std::string_view typeKey) const
{
    const std::optional<std::vector<std::string>> elements =
        ProjectTypeConversion::tupleElementTypeKeys(typeKey);
    if (!elements.has_value()) return std::nullopt;
    // LLVM and the Microsoft x64 ABI need an addressable carrier for every value.  Give the unit
    // tuple one inert byte: it has no observable elements, but can be stored, passed and returned
    // with the ordinary direct-aggregate rule instead of introducing a zero-sized ABI extension.
    if (elements->empty())
    {
        language::AggregateLayout unit;
        unit.size = 1;
        unit.alignment = 1;
        return unit;
    }
    std::vector<language::AggregateFieldLayout> fields;
    fields.reserve(elements->size());
    for (const std::string& element : *elements)
    {
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> layout =
            valueLayoutForTypeKey(nameLookup, element);
        if (!layout.has_value()) return std::nullopt;
        fields.push_back(language::AggregateFieldLayout{
            static_cast<std::size_t>(layout->first),
            static_cast<std::size_t>(layout->second)});
    }
    return language::computeWindowsX64AggregateLayout(fields);
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
            modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray ||
            modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
        {
            current = targetLayout_.layoutOf(BuiltinLayoutKind::Pointer);
            continue;
        }

        if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray)
        {
            if (modifierIndex + 1 != modifiers.size())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    modifier.range,
                    "a flexible array must be the outermost type modifier");
                return std::nullopt;
            }
            current.size = 0;
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
    if (syntax->isTuple())
    {
        if (nameLookup_ == nullptr || memberModel_ == nullptr) return std::nullopt;
        const std::string key = memberModel_->canonicalTypeKey(syntax, *nameLookup_);
        const std::optional<language::AggregateLayout> layout =
            tupleLayoutForTypeKey(*nameLookup_, key);
        if (!layout.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                useRange,
                "tuple field layout could not be computed for '" + key + "'");
            return std::nullopt;
        }
        return TypeLayout{
            static_cast<std::uint64_t>(layout->size),
            static_cast<std::uint64_t>(layout->alignment)};
    }
    if (syntax->isFunction())
    {
        // Both structural carriers are one Windows x64 pointer in storage. A function reference
        // points at the existing delegate cell; a function pointer points directly at code.
        return targetLayout_.layoutOf(BuiltinLayoutKind::Pointer);
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
        // EL TAMANO DE UN ENUM SALE DE SU TIPO SUBYACENTE, desde el 2026-09-03. Antes esto
        // devolvia siempre {4,4}, asi que un `enum : byte` dentro de una struct ocupaba cuatro
        // bytes donde C pone uno y desplazaba todo lo que iba detras -- en silencio.
        // La regla vive en include/nativesharp/Frontend/EnumUnderlyingType.h y la comparten el
        // modelo de layout, el backend y las decisiones de signo.
        const std::string_view underlying = enumUnderlyingTypeKey(type.syntax);
        if (const builtins::IntegerTypeInfo* info = builtins::findIntegerType(underlying);
            info != nullptr && info->width >= 8)
        {
            const std::uint64_t bytes = info->width / 8U;
            return TypeLayout{bytes, bytes};
        }
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
// `size(N)` -- el tamano AFIRMADO por el autor, si lo escribio. Va aparte de
// `target::StructLayoutOptions` a proposito: esa struct describe COMO se calcula el layout, y esto
// no participa en el calculo. Es una comprobacion sobre el resultado.
std::optional<std::uint64_t> parseAssertedSize(const NamedTypeDeclaration& type)
{
    const std::string& text = type.syntax != nullptr ? type.syntax->auxiliaryText() : std::string();
    std::size_t position = 0;
    while (position < text.size())
    {
        const std::size_t comma = text.find(',', position);
        const std::string_view token(
            text.data() + position,
            (comma == std::string::npos ? text.size() : comma) - position);
        position = (comma == std::string::npos ? text.size() : comma + 1);
        if (!token.starts_with("size=")) continue;
        std::uint64_t value = 0;
        bool digits = false;
        for (const char digit : token.substr(5))
        {
            if (std::isdigit(static_cast<unsigned char>(digit)) == 0) return std::nullopt;
            digits = true;
            value = value * 10U + static_cast<std::uint64_t>(digit - '0');
        }
        if (digits) return value;
    }
    return std::nullopt;
}

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
    std::function<bool(std::string_view, std::set<std::size_t>&)> hasTrivialLifetime =
        [&](std::string_view candidate, std::set<std::size_t>& visiting) -> bool
    {
        std::string current(candidate);
        while (const std::optional<std::string> element =
                   ProjectMemberModel::fixedArrayElementTypeKey(current))
            current = *element;
        if (!current.starts_with("type:")) return true;
        const std::string_view identity = std::string_view(current).substr(5);
        for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
        {
            const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[ordinal];
            if (declaration.identity.key() != identity) continue;
            if (declaration.kind != NamedTypeDeclarationKind::Struct) return true;
            if (!visiting.insert(ordinal).second) return false;
            for (const std::size_t nestedOrdinal : memberModel_->membersOfType(ordinal))
            {
                if (nestedOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& nested = memberModel_->members()[nestedOrdinal];
                if (nested.kind == ClassMemberKind::Destructor ||
                    (nested.kind == ClassMemberKind::Field && !nested.isStatic &&
                     (nested.declaredOwnership == sema::OwnershipKind::Owned ||
                      !hasTrivialLifetime(nested.declaredTypeKey, visiting))))
                {
                    visiting.erase(ordinal);
                    return false;
                }
            }
            visiting.erase(ordinal);
            return true;
        }
        return true;
    };
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
        const bool flexible =
            ProjectMemberModel::flexibleArrayElementTypeKey(member.declaredTypeKey).has_value();
        std::optional<std::uint64_t> bitWidth;
        if (member.isBitField)
        {
            bool validBase = ProjectIntegerEvaluator::isIntegerType(member.declaredTypeKey) ||
                member.declaredTypeKey == "builtin:bool";
            if (!validBase && member.declaredTypeKey.starts_with("type:"))
            {
                const std::string_view identity =
                    std::string_view(member.declaredTypeKey).substr(5);
                for (const NamedTypeDeclaration& declaration : nameLookup_->namedTypes())
                {
                    if (declaration.identity.key() == identity)
                    {
                        validBase = declaration.kind == NamedTypeDeclarationKind::Enum;
                        break;
                    }
                }
            }
            const std::optional<ProjectIntegerValue> folded =
                ProjectIntegerEvaluator(nullptr).evaluate(member.bitWidthSyntax, typeOrdinal);
            const bool negative = folded.has_value() &&
                ProjectIntegerEvaluator::isSignedInteger(folded->typeKey) &&
                folded->asSigned() < 0;
            if (!validBase || !folded.has_value() || negative ||
                folded->bits > fieldLayout->size * 8U ||
                (folded->bits == 0 && !member.isUnnamedBitField))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    !validBase
                        ? "a bit-field base type must be an integer, bool, or enum"
                        : !folded.has_value() || negative
                            ? "bit-field width must be a nonnegative integral constant expression"
                            : folded->bits == 0
                                ? "a named bit-field cannot have width zero"
                                : "bit-field width exceeds its storage type");
                namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
                return false;
            }
            bitWidth = folded->bits;
        }
        if (flexible)
        {
            const std::string element =
                *ProjectMemberModel::flexibleArrayElementTypeKey(member.declaredTypeKey);
            std::set<std::size_t> visiting;
            if (member.declaredOwnership != sema::OwnershipKind::None ||
                !hasTrivialLifetime(element, visiting))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    "a flexible array requires an unqualified element type with trivial lifetime");
                namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
                return false;
            }
        }
        physicalFields.push_back(
            target::StructFieldRequest{
                *fieldLayout,
                member.explicitFieldOffset,
                member.explicitFieldAlignment,
                flexible,
                bitWidth,
                member.isUnnamedBitField});
        fieldMembers.push_back(memberOrdinal);
    }

    // C2.2 "union and explicit layout": this is THE one place in the compiler that knows a union
    // is not an ordinary struct. Everything else -- members, by-value semantics, the C ABI
    // classification, the [N x i8] LLVM carrier, destruction -- treats the two identically,
    // because in every respect except where the fields sit, they ARE identical. The markers were
    // written onto the declaration by the parser (`union`, `pack=N`, `align=N`).
    const target::StructLayoutOptions layoutOptions = parseStructLayoutOptions(type);
    for (std::size_t index = 0; index < physicalFields.size(); ++index)
    {
        if (!physicalFields[index].isFlexibleArray) continue;
        if (layoutOptions.isUnion || index + 1 != physicalFields.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                memberModel_->members()[fieldMembers[index]].range,
                "a flexible array is permitted only as the final instance field of a non-union struct");
            namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
            return false;
        }
    }
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

    // Explicit offsets can express the same physical overlap as a union without using the union
    // keyword.  An overlapping field that owns storage has the identical unsound replacement
    // behavior: a write releases bytes that may belong to another live field.  Check the computed
    // ranges (not merely the presence of at(N)) so hand-laid-out but non-overlapping ABI structs
    // remain legal.
    const bool hasExplicitFieldOffsets = std::ranges::any_of(
        physicalFields,
        [](const target::StructFieldRequest& field) { return field.explicitOffset.has_value(); });
    if (hasExplicitFieldOffsets)
    {
        const auto fieldTransitivelyOwnsStorage = [&](std::size_t index) -> bool
        {
            if (index >= fieldMembers.size()) return false;
            const ClassMemberSymbol& member = memberModel_->members()[fieldMembers[index]];
            if (member.declaredOwnership == sema::OwnershipKind::Owned) return true;
            return memberModel_->typeTransitivelyOwnsStorage(member.declaredTypeKey);
        };
        for (std::size_t left = 0; left < layout->fields.size(); ++left)
        {
            for (std::size_t right = left + 1; right < layout->fields.size(); ++right)
            {
                const target::StructFieldLayout& leftLayout = layout->fields[left];
                const target::StructFieldLayout& rightLayout = layout->fields[right];
                const bool overlaps = leftLayout.offset < rightLayout.offset + rightLayout.size &&
                    rightLayout.offset < leftLayout.offset + leftLayout.size;
                if (!overlaps) continue;
                const bool leftOwns = fieldTransitivelyOwnsStorage(left);
                const bool rightOwns = fieldTransitivelyOwnsStorage(right);
                if (!leftOwns && !rightOwns) continue;
                const std::size_t owner = leftOwns ? left : right;
                const std::size_t other = leftOwns ? right : left;
                const ClassMemberSymbol& ownerMember = memberModel_->members()[fieldMembers[owner]];
                const ClassMemberSymbol& otherMember = memberModel_->members()[fieldMembers[other]];
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    ownerMember.range,
                    "field '" + ownerMember.name + "' contains owned storage and overlaps field '" +
                        otherMember.name + "'; explicit layout cannot overlap owning storage "
                        "without active-member tracking");
                namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
                return false;
            }
        }
    }

    StructValueLayout valueLayout;
    valueLayout.typeOrdinal = typeOrdinal;
    valueLayout.typeKey = "type:" + type.identity.key();
    valueLayout.size = layout->size;
    valueLayout.alignment = layout->alignment;
    valueLayout.isUnion = layoutOptions.isUnion;
    valueLayout.pack = layoutOptions.pack;
    valueLayout.minimumAlignment = layoutOptions.minimumAlignment;
    valueLayout.hasExplicitFieldOffsets = hasExplicitFieldOffsets;
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
            member.declaredTypeKey,
            field.bitOffset,
            field.bitWidth});
    }

    // `size(N)`. Aqui es donde se comprueba, porque es el unico sitio donde el tamano YA ESTA
    // CALCULADO -- con su relleno final, que es justo la parte que nadie escribe y donde un
    // binding se desvia de C sin enterarse.
    //
    // EL MENSAJE LLEVA LOS DOS NUMEROS. Un diagnostico que solo dijera "el tamano no coincide"
    // obligaria a otra corrida para saber cual es: quien lo lee esta comparando contra un header
    // de C y lo que necesita es la diferencia, no la noticia.
    if (const std::optional<std::uint64_t> asserted = parseAssertedSize(type);
        asserted.has_value() && *asserted != layout->size)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "struct '" + type.identity.key() + "' declares size(" + std::to_string(*asserted) +
                ") but its fields lay out to " + std::to_string(layout->size) +
                " bytes with alignment " + std::to_string(layout->alignment) +
                "; size() verifies the layout, it does not pad it");
        // MARCAR `Failed` ANTES DE SALIR, como hacen los otros tres puntos de fallo de esta
        // funcion. Dejarlo en `Visiting` haria que la SIGUIENTE consulta de este mismo tipo
        // --cualquier struct que lo tenga como campo-- lo tomara por un ciclo, y el segundo
        // diagnostico diria "referencia circular" sobre una struct que no tiene ninguna. Un error
        // que inventa un segundo error distinto es peor que el primero.
        namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
        return false;
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

    // Item 328. Treat the object header/base subobject as an already-laid-out prefix and route
    // every instance field through the same target authority used by value structs. This keeps
    // class bit-field allocation units, zero-width barriers and ordinary trailing fields from
    // growing a second MSVC layout algorithm here.
    std::vector<target::StructFieldRequest> physicalFields;
    std::vector<std::size_t> fieldMembers;
    physicalFields.push_back(target::StructFieldRequest{
        TypeLayout{offset, aggregateAlignment},
        std::nullopt,
        std::nullopt,
        false,
        std::nullopt,
        false});

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

        if (ProjectMemberModel::flexibleArrayElementTypeKey(member.declaredTypeKey).has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "a flexible array belongs only to the final field of a struct; class allocation has a fixed object size");
            return false;
        }

        const std::optional<TypeLayout> fieldLayout = layoutTypeSyntax(
            member.declaredTypeSyntax,
            diagnostics,
            member.range);
        if (!fieldLayout.has_value())
        {
            return false;
        }

        std::optional<std::uint64_t> bitWidth;
        if (member.isBitField)
        {
            bool validBase = ProjectIntegerEvaluator::isIntegerType(member.declaredTypeKey) ||
                member.declaredTypeKey == "builtin:bool";
            if (!validBase && member.declaredTypeKey.starts_with("type:"))
            {
                const std::string_view identity =
                    std::string_view(member.declaredTypeKey).substr(5);
                for (const NamedTypeDeclaration& declaration : nameLookup_->namedTypes())
                {
                    if (declaration.identity.key() == identity)
                    {
                        validBase = declaration.kind == NamedTypeDeclarationKind::Enum;
                        break;
                    }
                }
            }
            const std::optional<ProjectIntegerValue> folded =
                ProjectIntegerEvaluator(nullptr).evaluate(member.bitWidthSyntax, typeOrdinal);
            const bool negative = folded.has_value() &&
                ProjectIntegerEvaluator::isSignedInteger(folded->typeKey) &&
                folded->asSigned() < 0;
            if (!validBase || !folded.has_value() || negative ||
                folded->bits > fieldLayout->size * 8U ||
                (folded->bits == 0 && !member.isUnnamedBitField))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    !validBase
                        ? "a bit-field base type must be an integer, bool, or enum"
                        : !folded.has_value() || negative
                            ? "bit-field width must be a nonnegative integral constant expression"
                            : folded->bits == 0
                                ? "a named bit-field cannot have width zero"
                                : "bit-field width exceeds its storage type");
                return false;
            }
            bitWidth = folded->bits;
        }

        physicalFields.push_back(target::StructFieldRequest{
            *fieldLayout,
            std::nullopt,
            member.explicitFieldAlignment,
            false,
            bitWidth,
            member.isUnnamedBitField});
        fieldMembers.push_back(memberOrdinal);
    }

    const std::optional<target::StructLayout> physicalLayout =
        targetLayout_.layoutStruct(physicalFields, {});
    if (!physicalLayout.has_value() || physicalLayout->fields.size() != physicalFields.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "class object layout overflows the Windows x64 object-size model for '" +
                type.identity.fullyQualifiedName() + "'");
        return false;
    }

    for (std::size_t index = 0; index < fieldMembers.size(); ++index)
    {
        const std::size_t memberOrdinal = fieldMembers[index];
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        const target::StructFieldLayout& field = physicalLayout->fields[index + 1];
        layout.fields.push_back(ObjectFieldLayout{
            memberOrdinal,
            field.offset,
            field.size,
            field.alignment,
            member.declaredTypeKey,
            field.bitOffset,
            field.bitWidth});
    }

    layout.size = physicalLayout->size;
    layout.alignment = physicalLayout->alignment;

    if (layout.size < offset)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "class object layout is smaller than its header or base subobject");
        return false;
    }

    const std::size_t layoutIndex = classLayouts_.size();
    classLayouts_.push_back(std::move(layout));
    classLayoutIndexByType_[typeOrdinal] = layoutIndex;
    return true;
}

bool ProjectObjectLayoutModel::buildPromotedFieldLayouts(
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return false;

    // Item 326. The ordinary struct/class layouts above contain only physical carrier fields.
    // A promoted name receives a derived lookup layout here: carrier offsets plus the ultimate
    // source field offset. Keeping these entries out of StructValueLayout::fields and
    // ClassObjectLayout::fields prevents header emission, aggregate initialization and
    // destruction from mistaking a semantic alias for duplicate storage.
    for (std::size_t typeOrdinal = 0;
         typeOrdinal < nameLookup_->namedTypes().size();
         ++typeOrdinal)
    {
        for (const std::size_t aliasOrdinal : memberModel_->promotedMembersOfType(typeOrdinal))
        {
            if (aliasOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& alias = memberModel_->members()[aliasOrdinal];
            if (!alias.isAnonymousMemberPromotion ||
                !alias.promotedSourceMemberOrdinal.has_value())
                continue;

            const ObjectFieldLayout* source =
                fieldLayoutForMember(*alias.promotedSourceMemberOrdinal);
            if (source == nullptr)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    alias.range,
                    "promoted anonymous member has no physical source-field layout");
                return false;
            }

            std::uint64_t offset = source->offset;
            std::uint64_t alignment = source->alignment;
            for (const std::size_t carrierOrdinal : alias.anonymousCarrierMemberOrdinals)
            {
                const ObjectFieldLayout* carrier = fieldLayoutForMember(carrierOrdinal);
                if (carrier == nullptr)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        alias.range,
                        "promoted anonymous member has no physical carrier-field layout");
                    return false;
                }
                const std::optional<std::uint64_t> combined = checkedAdd(offset, carrier->offset);
                if (!combined.has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        alias.range,
                        "promoted anonymous member offset overflows the Windows x64 layout model");
                    return false;
                }
                offset = *combined;
                alignment = std::min(alignment, carrier->alignment);
            }
            promotedFieldLayouts_.push_back(ObjectFieldLayout{
                aliasOrdinal,
                offset,
                source->size,
                alignment,
                alias.declaredTypeKey,
                source->bitOffset,
                source->bitWidth});
        }
    }
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
        if (construction.syntax != nullptr &&
            construction.syntax->kind() == ast::AstNodeKind::PlacementConstructionExpression)
        {
            // Item 318 consumes caller-provided storage and therefore has a layout obligation but
            // never an allocation plan. MIR reads the canonical class/struct layout directly.
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
