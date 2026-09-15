#include "nativesharp/Interop/CHeaderGenerator.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::interop
{
namespace
{
using frontend::ClassMemberKind;
using frontend::ClassMemberSymbol;
using frontend::NamedTypeDeclaration;
using frontend::NamedTypeDeclarationKind;
using frontend::ProjectCompilation;
using frontend::ProjectMemberModel;
using frontend::ProjectNameLookup;
using frontend::ProjectObjectLayoutModel;

struct HostContext final
{
    const ProjectNameLookup& names;
    const ProjectMemberModel& members;
    const ProjectObjectLayoutModel& layouts;
    std::size_t rootProjectOrdinal = 0;
    std::unordered_map<std::string, std::string> cNames;
};

std::string sanitizeIdentifier(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char character : text)
    {
        const unsigned char value = static_cast<unsigned char>(character);
        if (std::isalnum(value) != 0 || character == '_') result.push_back(character);
        else result.push_back('_');
    }
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front())) != 0)
    {
        result.insert(result.begin(), '_');
    }
    return result;
}

// NSC-LIMIT-009. La identidad completa como identificador C VALIDO, estable e inyectivo.
//
// `sanitizeIdentifier` aplasta a `_` todo lo que no sea alfanumerico, de modo que `Mod::A.B` y
// `Mod::A_B` daban el mismo nombre. NO producia dos typedefs iguales --el bucle de mas abajo
// detecta el choque y anade `_2`-- pero eso es peor de lo que parece: cual de los dos se queda con
// el nombre limpio y cual con el `_2` depende del ORDEN en que se recorren los tipos. Mover una
// declaracion de sitio cambiaba el header, y un header generado es un contrato con el codigo C
// que lo incluye.
//
// La codificacion escapa TODO, empezando por el propio `_`:
//
//     alfanumerico  ->  el mismo caracter
//     `_`           ->  `__`
//     cualquier otro->  `_` + dos digitos hexadecimales en mayuscula
//
// Es reversible, y por tanto inyectiva: `_` siempre abre un escape, y `_` no es un digito hex, asi
// que `__` nunca se confunde con `_HH`. Y a diferencia del simbolo nativo NO puede usar `$`: esto
// tiene que ser un identificador de C.
std::string stableHostIdentifier(std::string_view text)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(text.size() * 3);
    for (const char character : text)
    {
        const auto byte = static_cast<unsigned char>(character);
        const bool asciiAlphaNumeric =
            (byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9');
        if (asciiAlphaNumeric)
        {
            result.push_back(static_cast<char>(byte));
            continue;
        }
        result.push_back('_');
        if (byte == '_')
        {
            result.push_back('_');
            continue;
        }
        result.push_back(kHex[byte >> 4]);
        result.push_back(kHex[byte & 0x0F]);
    }
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front())) != 0)
    {
        result.insert(result.begin(), '_');
    }
    return result;
}

std::string upperIdentifier(std::string_view text)
{
    std::string result = sanitizeIdentifier(text);
    std::transform(
        result.begin(), result.end(), result.begin(),
        [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
    return result;
}

std::pair<std::string_view, std::size_t> splitPointerType(std::string_view typeKey) noexcept
{
    std::size_t pointerDepth = 0;
    while (!typeKey.empty() && typeKey.back() == '*')
    {
        ++pointerDepth;
        typeKey.remove_suffix(1);
    }
    return {typeKey, pointerDepth};
}

const NamedTypeDeclaration* namedTypeForCoreKey(
    std::string_view coreKey,
    const ProjectNameLookup& names) noexcept
{
    if (!coreKey.starts_with("type:")) return nullptr;
    const std::string_view identityKey = coreKey.substr(5);
    for (const NamedTypeDeclaration& type : names.namedTypes())
    {
        if (type.identity.key() == identityKey) return &type;
    }
    return nullptr;
}

const NamedTypeDeclaration* namedTypeForKey(
    std::string_view typeKey,
    const ProjectNameLookup& names) noexcept
{
    return namedTypeForCoreKey(splitPointerType(typeKey).first, names);
}

std::unordered_map<std::string, std::string> buildHostTypeNames(const ProjectNameLookup& names)
{
    std::unordered_map<std::string, std::size_t> simpleCounts;
    for (const NamedTypeDeclaration& type : names.namedTypes()) ++simpleCounts[type.identity.name()];

    // EN ORDEN CANONICO, no en orden de declaracion. El desempate de mas abajo asigna nombres
    // distintos a candidatos iguales, asi que quien llega primero se queda el nombre limpio: en
    // orden de declaracion eso hace que mover una clase de sitio cambie el header. Ordenar por
    // clave de identidad --que no depende de donde se escribio-- lo vuelve reproducible.
    std::vector<const NamedTypeDeclaration*> ordered;
    ordered.reserve(names.namedTypes().size());
    for (const NamedTypeDeclaration& type : names.namedTypes()) ordered.push_back(&type);
    std::sort(
        ordered.begin(), ordered.end(),
        [](const NamedTypeDeclaration* left, const NamedTypeDeclaration* right)
        {
            return left->identity.key() < right->identity.key();
        });

    std::unordered_map<std::string, std::string> result;
    std::unordered_set<std::string> used;
    for (const NamedTypeDeclaration* type : ordered)
    {
        // Nombre CORTO cuando es unico en todo el programa --que es lo que hace legible un header
        // generado-- y la IDENTIDAD COMPLETA codificada cuando no lo es. La segunda forma es
        // inyectiva por construccion, asi que dos tipos distintos ya no pueden pedir el mismo
        // nombre por ese camino.
        std::string base = simpleCounts[type->identity.name()] == 1
            ? "NS_" + sanitizeIdentifier(type->identity.name())
            : "NS_" + stableHostIdentifier(type->identity.key());
        // El desempate se conserva como DEFENSA, no como mecanismo: con la codificacion de arriba
        // los choques dentro de cada familia son imposibles, y el unico residuo concebible es que
        // un nombre corto coincida con la codificacion de otra identidad. Si llegara a pasar, el
        // orden canonico de arriba garantiza al menos que el resultado es el mismo en cada build.
        std::string candidate = base;
        std::size_t suffix = 2;
        while (!used.insert(candidate).second)
        {
            candidate = base + '_' + std::to_string(suffix++);
        }
        result.emplace(type->identity.key(), std::move(candidate));
    }
    return result;
}

std::string hostNamedTypeName(const HostContext& context, const NamedTypeDeclaration& type)
{
    const auto found = context.cNames.find(type.identity.key());
    if (found != context.cNames.end()) return found->second;
    return "NS_" + stableHostIdentifier(type.identity.key());
}

std::string hostType(std::string_view typeKey, const HostContext& context, std::string& error)
{
    const auto [coreKey, pointerDepth] = splitPointerType(typeKey);
    std::string result;
    if (coreKey == "builtin:void") result = "void";
    // ID 89. `sbyte` es `int8_t`, que es la RAZON DE SER de este tipo: una API de C con
    // `signed char` no se podia declarar honestamente sin el.
    else if (coreKey == "builtin:sbyte") result = "int8_t";
    else if (coreKey == "builtin:bool" || coreKey == "builtin:byte" ||
             coreKey == "builtin:char") result = "uint8_t";
    else if (coreKey == "builtin:short") result = "int16_t";
    else if (coreKey == "builtin:ushort") result = "uint16_t";
    else if (coreKey == "builtin:int") result = "int32_t";
    else if (coreKey == "builtin:uint") result = "uint32_t";
    else if (coreKey == "builtin:long") result = "int64_t";
    else if (coreKey == "builtin:ulong") result = "uint64_t";
    else if (coreKey == "builtin:nint") result = "intptr_t";
    else if (coreKey == "builtin:nuint") result = "uintptr_t";
    else if (coreKey == "builtin:float") result = "float";
    else if (coreKey == "builtin:double") result = "double";
    else
    {
        const NamedTypeDeclaration* named = namedTypeForCoreKey(coreKey, context.names);
        if (named == nullptr)
        {
            error = "cannot express NativeSharp ABI type '" + std::string(typeKey) + "' in generated C header";
            return {};
        }
        result = hostNamedTypeName(context, *named);
        if (named->kind == NamedTypeDeclarationKind::Class ||
            named->kind == NamedTypeDeclarationKind::Interface)
        {
            result.push_back('*');
        }
    }
    result.append(pointerDepth, '*');
    return result;
}

void collectNamedTypeFromKey(
    std::string_view typeKey,
    const ProjectNameLookup& names,
    std::set<std::string>& keys)
{
    const NamedTypeDeclaration* named = namedTypeForKey(typeKey, names);
    if (named != nullptr) keys.insert(named->identity.key());
}

std::set<std::string> collectReachableHostTypes(const HostContext& context)
{
    std::set<std::string> keys;
    const auto types = context.names.namedTypes();
    for (const ClassMemberSymbol& member : context.members.members())
    {
        if (!member.isExportedC || member.declaringTypeOrdinal >= types.size()) continue;
        if (types[member.declaringTypeOrdinal].projectOrdinal != context.rootProjectOrdinal) continue;
        collectNamedTypeFromKey(member.declaredTypeKey, context.names, keys);
        for (const auto& parameter : member.parameters)
        {
            collectNamedTypeFromKey(parameter.typeKey, context.names, keys);
        }
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        const std::vector<std::string> snapshot(keys.begin(), keys.end());
        for (const std::string& key : snapshot)
        {
            const NamedTypeDeclaration* type = nullptr;
            std::size_t typeOrdinal = 0;
            for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
            {
                if (types[ordinal].identity.key() == key)
                {
                    type = &types[ordinal];
                    typeOrdinal = ordinal;
                    break;
                }
            }
            if (type == nullptr) continue;

            if (type->kind == NamedTypeDeclarationKind::Struct)
            {
                for (const std::size_t ordinal : context.members.membersOfType(typeOrdinal))
                {
                    const ClassMemberSymbol& field = context.members.members()[ordinal];
                    if (field.kind != ClassMemberKind::Field || field.isStatic) continue;
                    const std::size_t before = keys.size();
                    collectNamedTypeFromKey(field.declaredTypeKey, context.names, keys);
                    changed = changed || keys.size() != before;
                }
            }
            else if (type->kind == NamedTypeDeclarationKind::Delegate && type->syntax != nullptr)
            {
                const std::string returnKey = context.members.canonicalTypeKey(type->syntax->typeSyntax(), context.names);
                const std::size_t beforeReturn = keys.size();
                collectNamedTypeFromKey(returnKey, context.names, keys);
                changed = changed || keys.size() != beforeReturn;
                for (const ast::AstNode* parameter : type->syntax->children())
                {
                    if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration) continue;
                    const std::string parameterKey = context.members.canonicalTypeKey(parameter->typeSyntax(), context.names);
                    const std::size_t before = keys.size();
                    collectNamedTypeFromKey(parameterKey, context.names, keys);
                    changed = changed || keys.size() != before;
                }
            }
        }
    }
    return keys;
}

std::optional<HostContext> makeContext(const ProjectCompilation& compilation, std::string& error)
{
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.objectLayoutModel() == nullptr)
    {
        error = "Stage 48.1 C header generation requires project names, members and object layouts";
        return std::nullopt;
    }
    const auto projects = compilation.projectGraph()->projects();
    if (projects.empty() || compilation.projectGraph()->rootProject() == nullptr)
    {
        error = "Stage 48.1 C header generation requires a root project";
        return std::nullopt;
    }
    return HostContext{
        *compilation.nameLookup(),
        *compilation.memberModel(),
        *compilation.objectLayoutModel(),
        projects.size() - 1,
        buildHostTypeNames(*compilation.nameLookup())};
}

bool writeText(const std::filesystem::path& destination, const std::string& text, std::string& error)
{
    std::error_code ec;
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec)
    {
        error = "failed to create C header directory '" + parent.string() + "': " + ec.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "failed to open generated C header '" + destination.string() + "'";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream)
    {
        error = "failed to write generated C header '" + destination.string() + "'";
        return false;
    }
    return true;
}

void emitOpaque(
    std::ostringstream& out,
    const HostContext& context,
    const NamedTypeDeclaration& type,
    bool isUnion)
{
    // C2.2: the forward typedef's tag kind must MATCH the definition -- C rejects
    // `typedef struct X X;` followed by `union X { ... };` outright.
    const std::string hostName = hostNamedTypeName(context, type);
    out << "typedef " << (isUnion ? "union " : "struct ") << hostName << ' ' << hostName << ";\n";
}

bool emitDelegate(
    std::ostringstream& out,
    const HostContext& context,
    const NamedTypeDeclaration& type,
    std::string& error)
{
    if (type.syntax == nullptr)
    {
        error = "delegate '" + type.identity.fullyQualifiedName() + "' has no syntax metadata";
        return false;
    }
    const std::string returnKey = context.members.canonicalTypeKey(type.syntax->typeSyntax(), context.names);
    const std::string returnType = hostType(returnKey, context, error);
    if (!error.empty()) return false;

    out << "typedef " << returnType << " (NS_CALL *" << hostNamedTypeName(context, type) << ")(";
    bool first = true;
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration) continue;
        if (!first) out << ", ";
        first = false;
        const std::string parameterKey = context.members.canonicalTypeKey(parameter->typeSyntax(), context.names);
        const std::string parameterType = hostType(parameterKey, context, error);
        if (!error.empty()) return false;
        out << parameterType << ' ' << sanitizeIdentifier(parameter->text());
    }
    if (first) out << "void";
    out << ");\n";
    return true;
}

bool isAnonymousMemberCarrier(const ClassMemberSymbol& member) noexcept
{
    return member.isCompilerGenerated && member.syntax != nullptr &&
        member.syntax->auxiliaryText().find("anonymous-member-carrier") != std::string::npos;
}

bool isPromotedAnonymousLayoutType(const NamedTypeDeclaration& type) noexcept
{
    return type.syntax != nullptr &&
        type.syntax->auxiliaryText().find("promote") != std::string::npos;
}

bool emitStruct(
    std::ostringstream& out,
    const HostContext& context,
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    std::string& error)
{
    const frontend::StructValueLayout* layout = context.layouts.structLayoutForType(typeOrdinal);
    if (layout == nullptr)
    {
        error = "missing layout for host struct '" + type.identity.fullyQualifiedName() + "'";
        return false;
    }
    const std::string hostName = hostNamedTypeName(context, type);
    // C2.2 "union and explicit layout": what the host sees must be the SAME aggregate, declared
    // the way C declares it -- a `union` is emitted as a union, `pack(N)` as the pragma pair, and
    // `align(N)` as an alignment attribute. The NS_STATIC_ASSERTs below are the double-entry
    // bookkeeping that makes this claim checkable rather than asserted: the host's own compiler
    // verifies size, alignment and every field offset against the numbers NativeSharp computed.
    //
    // Explicit per-field offsets are the one shape a plain C declaration cannot express, so
    // rather than emit a header whose static assertions would fail on the host, that is refused
    // here with the reason -- an overlapping record crosses the boundary as a union.
    if (layout->hasExplicitFieldOffsets)
    {
        error = "host header cannot express explicit field offsets in struct '" +
            type.identity.fullyQualifiedName() + "'; expose it as a union across the C boundary";
        return false;
    }
    if (layout->pack != 0)
    {
        out << "#pragma pack(push, " << layout->pack << ")\n";
    }
    out << (layout->isUnion ? "union " : "struct ") << hostName << "\n{\n";
    // `align(N)` becomes an alignment specifier on the FIRST member, which is where C11 accepts
    // one and where it raises the whole aggregate's alignment. (After the tag it is not valid C.)
    bool pendingAlignment = layout->minimumAlignment != 0;
    std::function<bool(
        const ClassMemberSymbol&,
        std::string_view,
        bool&,
        std::uint64_t)> emitField;
    emitField = [&](const ClassMemberSymbol& field,
                    const std::string_view indentation,
                    bool& aggregatePendingAlignment,
                    const std::uint64_t aggregateMinimumAlignment) -> bool
    {
        const bool anonymousCarrier = isAnonymousMemberCarrier(field);
        if (anonymousCarrier)
        {
            const NamedTypeDeclaration* nestedType =
                namedTypeForCoreKey(field.declaredTypeKey, context.names);
            const auto namedTypes = context.names.namedTypes();
            if (nestedType == nullptr || nestedType->kind != NamedTypeDeclarationKind::Struct)
            {
                error = "host header cannot resolve anonymous carrier type '" +
                    field.declaredTypeKey + "'";
                return false;
            }
            const std::size_t nestedOrdinal =
                static_cast<std::size_t>(nestedType - namedTypes.data());
            const frontend::StructValueLayout* nestedLayout =
                context.layouts.structLayoutForType(nestedOrdinal);
            if (nestedLayout == nullptr)
            {
                error = "missing layout for anonymous host aggregate '" +
                    nestedType->identity.fullyQualifiedName() + "'";
                return false;
            }
            if (nestedLayout->hasExplicitFieldOffsets)
            {
                error = "host header cannot express explicit field offsets in anonymous aggregate '" +
                    nestedType->identity.fullyQualifiedName() + "'";
                return false;
            }
            if (nestedLayout->pack != 0)
                out << "#pragma pack(push, " << nestedLayout->pack << ")\n";
            out << indentation;
            if (field.explicitFieldAlignment.has_value())
                out << "NS_ALIGNAS(" << *field.explicitFieldAlignment << ") ";
            if (aggregatePendingAlignment)
            {
                out << "NS_ALIGNAS(" << aggregateMinimumAlignment << ") ";
                aggregatePendingAlignment = false;
            }
            out << (nestedLayout->isUnion ? "union" : "struct") << "\n"
                << indentation << "{\n";
            bool nestedPendingAlignment = nestedLayout->minimumAlignment != 0;
            for (const std::size_t nestedMemberOrdinal : context.members.membersOfType(nestedOrdinal))
            {
                const ClassMemberSymbol& nestedField = context.members.members()[nestedMemberOrdinal];
                if (nestedField.kind != ClassMemberKind::Field || nestedField.isStatic) continue;
                if (!emitField(nestedField, std::string(indentation) + "    ",
                        nestedPendingAlignment, nestedLayout->minimumAlignment))
                    return false;
            }
            out << indentation << "};\n";
            if (nestedLayout->pack != 0) out << "#pragma pack(pop)\n";
            return true;
        }

        // C2.1 "fixed arrays": C spells an array field with the extent AFTER the name, so the
        // element type and the count are emitted on either side of it.
        const std::optional<std::string> element =
            frontend::ProjectMemberModel::fixedArrayElementTypeKey(field.declaredTypeKey);
        const std::optional<std::string> flexibleElement =
            frontend::ProjectMemberModel::flexibleArrayElementTypeKey(field.declaredTypeKey);
        const std::string fieldType = hostType(
            element.has_value() ? *element
                : flexibleElement.has_value() ? *flexibleElement : field.declaredTypeKey,
            context,
            error);
        if (!error.empty()) return false;
        out << indentation;
        if (field.explicitFieldAlignment.has_value())
            out << "NS_ALIGNAS(" << *field.explicitFieldAlignment << ") ";
        if (aggregatePendingAlignment)
        {
            out << "NS_ALIGNAS(" << aggregateMinimumAlignment << ") ";
            aggregatePendingAlignment = false;
        }
        out << fieldType << ' ' << sanitizeIdentifier(field.name);
        if (element.has_value())
        {
            const std::optional<std::uint64_t> count =
                frontend::ProjectMemberModel::fixedArrayCount(field.declaredTypeKey);
            if (!count.has_value())
            {
                error = "host header cannot express fixed-array field '" + field.name + "'";
                return false;
            }
            out << '[' << *count << ']';
        }
        else if (flexibleElement.has_value())
        {
            out << "[]";
        }
        out << ";\n";
        return true;
    };
    for (const std::size_t ordinal : context.members.membersOfType(typeOrdinal))
    {
        const ClassMemberSymbol& field = context.members.members()[ordinal];
        if (field.kind != ClassMemberKind::Field || field.isStatic) continue;
        if (!emitField(field, "    ", pendingAlignment, layout->minimumAlignment)) return false;
    }
    out << "};\n";
    if (layout->pack != 0)
    {
        out << "#pragma pack(pop)\n";
    }
    out << "NS_STATIC_ASSERT(sizeof(" << hostName << ") == " << layout->size
        << ", \"NativeSharp host struct size mismatch: " << hostName << "\");\n";
    out << "NS_STATIC_ASSERT(NS_ALIGNOF(" << hostName << ") == " << layout->alignment
        << ", \"NativeSharp host struct alignment mismatch: " << hostName << "\");\n";
    for (const frontend::ObjectFieldLayout& fieldLayout : layout->fields)
    {
        if (fieldLayout.memberOrdinal >= context.members.members().size()) continue;
        const ClassMemberSymbol& field = context.members.members()[fieldLayout.memberOrdinal];
        if (isAnonymousMemberCarrier(field)) continue;
        out << "NS_STATIC_ASSERT(offsetof(" << hostName << ", " << sanitizeIdentifier(field.name)
            << ") == " << fieldLayout.offset << ", \"NativeSharp host struct offset mismatch: "
            << hostName << '.' << sanitizeIdentifier(field.name) << "\");\n";
    }
    for (const std::size_t promotedOrdinal : context.members.promotedMembersOfType(typeOrdinal))
    {
        if (promotedOrdinal >= context.members.members().size()) continue;
        const ClassMemberSymbol& promoted = context.members.members()[promotedOrdinal];
        const frontend::ObjectFieldLayout* promotedLayout =
            context.layouts.fieldLayoutForMember(promotedOrdinal);
        if (promotedLayout == nullptr) continue;
        out << "NS_STATIC_ASSERT(offsetof(" << hostName << ", " << sanitizeIdentifier(promoted.name)
            << ") == " << promotedLayout->offset << ", \"NativeSharp host struct offset mismatch: "
            << hostName << '.' << sanitizeIdentifier(promoted.name) << "\");\n";
    }
    return true;
}

bool emitExport(
    std::ostringstream& out,
    const HostContext& context,
    const ClassMemberSymbol& member,
    std::string& error)
{
    const std::string returnType = hostType(member.declaredTypeKey, context, error);
    if (!error.empty()) return false;
    out << "NS_API " << returnType << " NS_CALL " << sanitizeIdentifier(member.cAbiName) << '(';
    if (member.parameters.empty()) out << "void";
    else
    {
        for (std::size_t index = 0; index < member.parameters.size(); ++index)
        {
            if (index != 0) out << ", ";
            const auto& parameter = member.parameters[index];
            const std::string parameterType = hostType(parameter.typeKey, context, error);
            if (!error.empty()) return false;
            out << parameterType << ' ' << sanitizeIdentifier(parameter.name);
        }
    }
    out << ");\n";
    return true;
}

std::vector<std::string> definitionDependencies(
    const HostContext& context,
    const NamedTypeDeclaration& type,
    std::size_t typeOrdinal)
{
    std::vector<std::string> dependencies;
    auto consider = [&](std::string_view typeKey)
    {
        const auto [coreKey, pointerDepth] = splitPointerType(typeKey);
        const NamedTypeDeclaration* dependency = namedTypeForCoreKey(coreKey, context.names);
        if (dependency == nullptr) return;
        if (dependency->kind == NamedTypeDeclarationKind::Delegate ||
            (dependency->kind == NamedTypeDeclarationKind::Struct && pointerDepth == 0))
        {
            if (std::find(dependencies.begin(), dependencies.end(), dependency->identity.key()) == dependencies.end())
            {
                dependencies.push_back(dependency->identity.key());
            }
        }
    };

    if (type.kind == NamedTypeDeclarationKind::Struct)
    {
        std::function<void(std::size_t)> considerFields;
        considerFields = [&](const std::size_t ownerOrdinal)
        {
            for (const std::size_t ordinal : context.members.membersOfType(ownerOrdinal))
            {
                const ClassMemberSymbol& field = context.members.members()[ordinal];
                if (field.kind != ClassMemberKind::Field || field.isStatic) continue;
                if (!isAnonymousMemberCarrier(field))
                {
                    consider(field.declaredTypeKey);
                    continue;
                }
                const NamedTypeDeclaration* nested =
                    namedTypeForCoreKey(field.declaredTypeKey, context.names);
                const auto types = context.names.namedTypes();
                if (nested != nullptr)
                    considerFields(static_cast<std::size_t>(nested - types.data()));
            }
        };
        considerFields(typeOrdinal);
    }
    else if (type.kind == NamedTypeDeclarationKind::Delegate && type.syntax != nullptr)
    {
        consider(context.members.canonicalTypeKey(type.syntax->typeSyntax(), context.names));
        for (const ast::AstNode* parameter : type.syntax->children())
        {
            if (parameter != nullptr && parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
            {
                consider(context.members.canonicalTypeKey(parameter->typeSyntax(), context.names));
            }
        }
    }
    return dependencies;
}

bool emitOrderedDefinitions(
    std::ostringstream& out,
    const HostContext& context,
    const std::set<std::string>& reachable,
    std::string& error)
{
    const auto types = context.names.namedTypes();
    std::unordered_map<std::string, std::size_t> ordinalByKey;
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        ordinalByKey.emplace(types[ordinal].identity.key(), ordinal);
    }

    enum class State { None, Visiting, Done };
    std::unordered_map<std::string, State> states;
    std::function<bool(const std::string&)> visit = [&](const std::string& key) -> bool
    {
        const auto foundOrdinal = ordinalByKey.find(key);
        if (foundOrdinal == ordinalByKey.end()) return true;
        const NamedTypeDeclaration& type = types[foundOrdinal->second];
        if (type.kind != NamedTypeDeclarationKind::Struct && type.kind != NamedTypeDeclarationKind::Delegate) return true;
        if (!reachable.contains(key)) return true;

        State& state = states[key];
        if (state == State::Done) return true;
        if (state == State::Visiting)
        {
            error = "cyclic by-value C header dependency involving '" + type.identity.fullyQualifiedName() + "'";
            return false;
        }
        state = State::Visiting;
        for (const std::string& dependency : definitionDependencies(context, type, foundOrdinal->second))
        {
            if (!visit(dependency)) return false;
        }

        if (isPromotedAnonymousLayoutType(type))
        {
            state = State::Done;
            return true;
        }

        if (type.kind == NamedTypeDeclarationKind::Struct)
        {
            if (!emitStruct(out, context, foundOrdinal->second, type, error)) return false;
        }
        else
        {
            if (!emitDelegate(out, context, type, error)) return false;
        }
        out << '\n';
        state = State::Done;
        return true;
    };

    for (const NamedTypeDeclaration& type : types)
    {
        if (reachable.contains(type.identity.key()) &&
            (type.kind == NamedTypeDeclarationKind::Struct || type.kind == NamedTypeDeclarationKind::Delegate))
        {
            if (!visit(type.identity.key())) return false;
        }
    }
    return true;
}
}

bool CHeaderGenerator::writeHeader(
    const ProjectCompilation& compilation,
    const std::filesystem::path& destination,
    std::string& error) const
{
    error.clear();
    const std::optional<HostContext> maybeContext = makeContext(compilation, error);
    if (!maybeContext.has_value()) return false;
    const HostContext& context = *maybeContext;
    const compilation::Project* root = compilation.projectGraph()->rootProject();
    if (root == nullptr)
    {
        error = "missing root project";
        return false;
    }

    std::ostringstream out;
    const std::string guard = "NATIVESHARP_" + upperIdentifier(root->moduleName()) + "_HOST_H";
    out << "/* Generated by nsc " << kCompilerVersion << ". Do not edit. */\n";
    out << "#ifndef " << guard << "\n#define " << guard << "\n\n";
    out << "#if !defined(_WIN32) && !defined(__CYGWIN__)\n"
           "#error \"This NativeSharp 1.0.1 host contract targets Windows only.\"\n"
           "#endif\n"
           "#if !defined(_M_X64) && !defined(__x86_64__)\n"
           "#error \"This NativeSharp 1.0.1 host contract targets Windows x64 only.\"\n"
           "#endif\n\n";
    out << "#include <stddef.h>\n#include <stdint.h>\n\n";
    out << "#if defined(__cplusplus)\n"
           "#define NS_STATIC_ASSERT(condition, message) static_assert((condition), message)\n"
           "#define NS_ALIGNOF(type) alignof(type)\n"
           "#define NS_ALIGNAS(bytes) alignas(bytes)\n"
           "#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L\n"
           "#define NS_STATIC_ASSERT(condition, message) _Static_assert((condition), message)\n"
           "#define NS_ALIGNOF(type) _Alignof(type)\n"
           "#define NS_ALIGNAS(bytes) _Alignas(bytes)\n"
           "#else\n"
           "#define NS_STATIC_ASSERT(condition, message)\n"
           "#define NS_ALIGNOF(type) 0\n"
           "#define NS_ALIGNAS(bytes)\n"
           "#endif\n\n";
    out << "#define NS_CALL\n";
    if (root->outputKind() == compilation::ProjectOutputKind::SharedLibrary)
    {
        out << "#if defined(" << upperIdentifier(root->moduleName()) << "_BUILDING_DLL)\n"
               "#define NS_API __declspec(dllexport)\n"
               "#else\n"
               "#define NS_API __declspec(dllimport)\n"
               "#endif\n\n";
    }
    else
    {
        out << "#define NS_API\n\n";
    }
    out << "/* ABI: Windows x64 C. bool/byte/char use one-byte uint8_t carriers. */\n"
           "/* Classes/interfaces are opaque pointer-sized handles. Struct layout is asserted below. */\n"
           "/* No hidden marshaling, retention or GC occurs at this boundary. Ownership is API-defined. */\n\n";

    const std::set<std::string> reachable = collectReachableHostTypes(context);
    const auto namedTypes = context.names.namedTypes();

    for (const NamedTypeDeclaration& type : namedTypes)
    {
        if (!reachable.contains(type.identity.key())) continue;
        if (isPromotedAnonymousLayoutType(type)) continue;
        if (type.kind == NamedTypeDeclarationKind::Class || type.kind == NamedTypeDeclarationKind::Interface ||
            type.kind == NamedTypeDeclarationKind::Struct)
        {
            const frontend::StructValueLayout* forwardLayout =
                type.kind == NamedTypeDeclarationKind::Struct
                    ? context.layouts.structLayoutForType(
                          static_cast<std::size_t>(&type - namedTypes.data()))
                    : nullptr;
            emitOpaque(out, context, type, forwardLayout != nullptr && forwardLayout->isUnion);
        }
    }
    if (!reachable.empty()) out << '\n';

    for (const NamedTypeDeclaration& type : namedTypes)
    {
        if (!reachable.contains(type.identity.key()) || type.kind != NamedTypeDeclarationKind::Enum) continue;
        // EL TYPEDEF LLEVA EL TIPO SUBYACENTE, no `int32_t` siempre.
        //
        // Esta linea es la cara de C del contrato: un `enum E : byte` que se declarara aqui como
        // `int32_t` haria que el lado de C leyera cuatro bytes de un campo de uno, y desplazara
        // todo lo que fuera detras dentro de una struct. Es exactamente el defecto que el tipo
        // subyacente vino a cerrar, pero en la direccion contraria -- y el mas dificil de ver,
        // porque el fichero que miente lo genera el propio compilador.
        //
        // La traduccion la hace hostType(), que ya sabe pasar cada clave de entero a su tipo de
        // <stdint.h>; no se escribe aqui una segunda tabla.
        std::string enumError;
        const std::string carrier =
            hostType(frontend::enumUnderlyingTypeKey(type.syntax), context, enumError);
        if (carrier.empty())
        {
            error = enumError;
            return false;
        }
        out << "typedef " << carrier << " " << hostNamedTypeName(context, type) << ";\n";
    }
    if (!reachable.empty()) out << '\n';

    if (!emitOrderedDefinitions(out, context, reachable, error)) return false;

    out << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    std::size_t exportCount = 0;
    for (const ClassMemberSymbol& member : context.members.members())
    {
        if (!member.isExportedC || member.declaringTypeOrdinal >= namedTypes.size()) continue;
        if (namedTypes[member.declaringTypeOrdinal].projectOrdinal != context.rootProjectOrdinal) continue;
        if (!emitExport(out, context, member, error)) return false;
        ++exportCount;
    }
    if (exportCount == 0) out << "/* This library has no export extern C host functions. */\n";
    out << "\n#ifdef __cplusplus\n}\n#endif\n\n";
    out << "#undef NS_API\n#undef NS_CALL\n#undef NS_ALIGNOF\n#undef NS_ALIGNAS\n#undef NS_STATIC_ASSERT\n\n";
    out << "#endif /* " << guard << " */\n";
    return writeText(destination, out.str(), error);
}
}
