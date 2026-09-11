#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string_view>
#include <span>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
// NSC-LIMIT-009. Solo se llama sobre claves de identidad, de modo que delega en el codificador
// que conserva el `+`. Antes aplastaba `+` y `.` al mismo `_` y el descriptor, la vtable y el mapa
// de interfaces de `N.Outer+Item` chocaban con los de `N.Outer.Item`.
std::string sanitizeDispatchName(std::string_view value)
{
    return compilation::symbolFragmentForIdentityKey(value);
}

std::uint64_t stableTypeId(std::string_view value) noexcept
{
    // FNV-1a 64-bit. This is an ABI-visible deterministic identity seed, not reflection.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        const auto byte = static_cast<unsigned char>(character);
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string genericSpecializationKeyForType(
    const ProjectNameLookup& nameLookup,
    std::size_t typeOrdinal)
{
    if (typeOrdinal >= nameLookup.namedTypes().size()) return {};
    constexpr std::string_view kSpecializationPrefix = "stage44-specialization:";
    const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
    const auto keyForSyntax = [&](const ast::AstNode* syntax) -> std::string {
        return syntax != nullptr && syntax->auxiliaryText().starts_with(kSpecializationPrefix)
            ? syntax->auxiliaryText().substr(kSpecializationPrefix.size())
            : std::string{};
    };
    if (std::string key = keyForSyntax(type.syntax); !key.empty()) return key;
    for (const NamedTypeDeclarationPart& part : type.parts)
    {
        if (std::string key = keyForSyntax(part.syntax); !key.empty()) return key;
    }
    // Name aggregation can select the open template as the declaration's primary syntax even
    // though this ordinal names the generated closed type.  The generated name is itself a
    // stable compiler contract: __g<arity>__<arguments>__h<16 hex>.  Treat it as reusable when
    // the AST marker is no longer reachable from the aggregate declaration.
    const std::string& name = type.identity.fullyQualifiedName();
    const std::size_t generic = name.rfind("__g");
    const std::size_t hash = name.rfind("__h");
    if (generic != std::string::npos && hash != std::string::npos && hash > generic + 3U &&
        hash + 3U + 16U == name.size())
    {
        bool validHash = true;
        for (const char character : std::string_view(name).substr(hash + 3U))
            validHash = validHash && std::isxdigit(static_cast<unsigned char>(character));
        if (validHash) return "type:" + type.identity.key();
    }
    return {};
}

const ProjectOopMirStaticField* knownStaticField(
    std::size_t memberOrdinal,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    for (const ProjectOopMirStaticField& field : knownFields)
    {
        if (field.memberOrdinal == memberOrdinal && field.isConst && field.hasConstantInitializer)
        {
            return &field;
        }
    }
    return nullptr;
}

bool isSignedIntegralType(std::string_view typeKey) noexcept
{
    // ID 89. Dice lo mismo que la tabla del modelo semantico y que la del backend, a proposito:
    // cada capa contesta sin depender de las otras. Las tres se fijan entre si en la fixture c19.
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
        typeKey == "builtin:int" || typeKey == "builtin:long" || typeKey == "builtin:nint";
}

unsigned integerBitWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
        typeKey == "builtin:char") return 8;
    if (typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32;
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint") return 64;
    return 64;
}

std::uint64_t integerMask(unsigned width) noexcept
{
    return width >= 64 ? std::numeric_limits<std::uint64_t>::max()
                       : ((std::uint64_t{1} << width) - 1U);
}

// ID 127. ESTA FUNCION HACIA MEDIA OPERACION, Y ESO ES UN DEFECTO SILENCIOSO.
//
// Decia:
//
//     return value & integerMask(integerBitWidth(typeKey));
//
// Truncaba al ancho del tipo y NO extendia el signo. La operacion canonica del compilador --
// `ProjectIntegerEvaluator::normalize`, cuyo propio comentario la llama "LA OPERACION CENTRAL" --
// hace las dos cosas: trunca, y si el tipo tiene signo rellena hacia arriba desde el bit mas alto
// del ancho real.
//
// QUE PRODUCIA. `integerConstantValue` (aqui abajo) es el plegado que decide el valor de TODO
// campo `const`, y lo hace normalizando cada operando al tipo del RESULTADO. Para `(-1) + 1L`:
//
//     -1 como int   -> 0xFFFFFFFFFFFFFFFF & 0xFFFFFFFF  =  0x00000000FFFFFFFF   (signo perdido)
//     ensanchado a long                                  =  4294967295
//     + 1                                                =  4294967296
//
// El campo quedaba con 4294967296 en vez de 0, sin un diagnostico: la constante se plegaba, el
// programa compilaba y enlazaba, y devolvia el numero equivocado. La fixture c17 lo destapo con
// codigo de salida 3.
//
// POR QUE NUNCA SE VIO. Era inalcanzable. El defecto solo aparece cuando un valor negativo de un
// tipo estrecho se ENSANCHA a uno mas ancho, y hasta el ID 121 `int + long` con constantes no
// compilaba -- el tipo comun era `double` y el evaluador entero se rendia. Los `const` negativos
// que existian eran de un solo ancho (`const int x = -5;`), donde truncar dos veces a 32 bits da
// el mismo patron y `LLVMConstInt(..., signExtend=1)` lo interpreta bien. **El ID 121 no
// introdujo este defecto: lo hizo alcanzable.**
//
// POR QUE SE DELEGA EN VEZ DE PARCHEAR. Esto era un TERCER evaluador de constantes enteras, con
// tabla de anchos propia, tabla de signo propia y normalizacion propia -- exactamente lo que
// CC1-105 elimino del modelo semantico despues de que produjera los defectos C-001..C-004 de una
// sola vez. La copia se quedo sin la mitad de la operacion, que es la forma en que las
// duplicaciones fallan: no divergen de golpe, se quedan cortas en una rama. Delegando, las dos
// capas no pueden volver a discrepar.
//
// Los otros dos consumidores de esta funcion (el plegado a `double`) pasan su resultado por
// `signedInteger()`, que vuelve a enmascarar antes de extender: para ellos el cambio es inocuo.
std::uint64_t normalizeInteger(std::uint64_t value, std::string_view typeKey) noexcept
{
    return ProjectIntegerEvaluator::normalize(value, typeKey);
}

std::int64_t signedInteger(std::uint64_t bits, std::string_view typeKey) noexcept
{
    const unsigned width = integerBitWidth(typeKey);
    bits &= integerMask(width);
    if (width >= 64)
    {
        return static_cast<std::int64_t>(bits);
    }
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((bits & sign) == 0)
    {
        return static_cast<std::int64_t>(bits);
    }
    return static_cast<std::int64_t>(bits | ~integerMask(width));
}

double normalizeFloating(double value, std::string_view typeKey) noexcept
{
    return typeKey == "builtin:float"
        ? static_cast<double>(static_cast<float>(value))
        : value;
}

std::optional<std::uint64_t> integerConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept;
std::optional<double> floatingConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept;
std::optional<bool> booleanConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept;
std::optional<std::string> stringConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields);

std::optional<std::string> stringConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields)
{
    if (node == nullptr) return std::nullopt;
    if (node->kind == ProjectOopHirNodeKind::StringLiteral)
    {
        return node->stringValue;
    }
    if (node->kind == ProjectOopHirNodeKind::StaticFieldValue && node->memberOrdinal.has_value())
    {
        const ProjectOopMirStaticField* field = knownStaticField(*node->memberOrdinal, knownFields);
        if (field != nullptr && field->typeKey == "builtin:string")
        {
            return field->constantStringValue;
        }
        return std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Conversion && node->typeKey == "builtin:string" &&
        !node->children.empty())
    {
        return stringConstantValue(node->children.front().get(), knownFields);
    }
    return std::nullopt;
}

std::optional<std::uint64_t> integerConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    if (node == nullptr) return std::nullopt;
    if (node->kind == ProjectOopHirNodeKind::IntegerConstant)
    {
        return normalizeInteger(node->integerValue, node->typeKey);
    }
    if (node->kind == ProjectOopHirNodeKind::StaticFieldValue && node->memberOrdinal.has_value())
    {
        const ProjectOopMirStaticField* field = knownStaticField(*node->memberOrdinal, knownFields);
        if (field != nullptr && ProjectTypeConversion::isIntegral(field->typeKey))
        {
            return normalizeInteger(field->constantIntegerValue, field->typeKey);
        }
        return std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Conversion && !node->children.empty())
    {
        if (!ProjectTypeConversion::isIntegral(node->typeKey)) return std::nullopt;
        const ProjectOopHirNode* child = node->children.front().get();
        if (ProjectTypeConversion::isIntegral(child->typeKey))
        {
            const std::optional<std::uint64_t> value = integerConstantValue(child, knownFields);
            return value.has_value() ? std::optional<std::uint64_t>(normalizeInteger(*value, node->typeKey))
                                     : std::nullopt;
        }
        // Stage 37 deliberately does not treat floating-to-integral casts as native const
        // expressions yet; rejecting them here keeps compile-time evaluation deterministic.
        return std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Unary && !node->children.empty())
    {
        const std::optional<std::uint64_t> value = integerConstantValue(node->children.front().get(), knownFields);
        if (!value.has_value()) return std::nullopt;
        switch (node->operation)
        {
            case lexer::TokenKind::Plus: return normalizeInteger(*value, node->typeKey);
            case lexer::TokenKind::Minus: return normalizeInteger(std::uint64_t{0} - *value, node->typeKey);
            case lexer::TokenKind::Tilde: return normalizeInteger(~*value, node->typeKey);
            default: return std::nullopt;
        }
    }
    if (node->kind == ProjectOopHirNodeKind::Binary && node->children.size() == 2)
    {
        const std::optional<std::uint64_t> leftValue = integerConstantValue(node->children[0].get(), knownFields);
        const std::optional<std::uint64_t> rightValue = integerConstantValue(node->children[1].get(), knownFields);
        if (!leftValue.has_value() || !rightValue.has_value()) return std::nullopt;
        const unsigned width = integerBitWidth(node->typeKey);
        const std::uint64_t left = normalizeInteger(*leftValue, node->typeKey);
        const std::uint64_t right = normalizeInteger(*rightValue, node->typeKey);
        std::uint64_t result = 0;
        switch (node->operation)
        {
            case lexer::TokenKind::Plus: result = left + right; break;
            case lexer::TokenKind::Minus: result = left - right; break;
            case lexer::TokenKind::Star: result = left * right; break;
            case lexer::TokenKind::Amp: result = left & right; break;
            case lexer::TokenKind::Pipe: result = left | right; break;
            case lexer::TokenKind::Caret: result = left ^ right; break;
            case lexer::TokenKind::LessLess:
                if (right >= width) return std::nullopt;
                result = left << static_cast<unsigned>(right);
                break;
            case lexer::TokenKind::GreaterGreater:
                if (right >= width) return std::nullopt;
                if (isSignedIntegralType(node->typeKey))
                {
                    result = static_cast<std::uint64_t>(
                        signedInteger(left, node->typeKey) >> static_cast<unsigned>(right));
                }
                else
                {
                    result = left >> static_cast<unsigned>(right);
                }
                break;
            case lexer::TokenKind::Slash:
            case lexer::TokenKind::Percent:
                if (right == 0) return std::nullopt;
                if (isSignedIntegralType(node->typeKey))
                {
                    const std::int64_t lhs = signedInteger(left, node->typeKey);
                    const std::int64_t rhs = signedInteger(right, node->typeKey);
                    const std::int64_t minimum = width == 64
                        ? std::numeric_limits<std::int64_t>::min()
                        : -(std::int64_t{1} << (width - 1U));
                    if (lhs == minimum && rhs == -1) return std::nullopt;
                    const std::int64_t computed = node->operation == lexer::TokenKind::Slash
                        ? lhs / rhs
                        : lhs % rhs;
                    result = static_cast<std::uint64_t>(computed);
                }
                else
                {
                    result = node->operation == lexer::TokenKind::Slash ? left / right : left % right;
                }
                break;
            default: return std::nullopt;
        }
        return normalizeInteger(result, node->typeKey);
    }
    return std::nullopt;
}

std::optional<double> floatingConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    if (node == nullptr) return std::nullopt;
    if (node->kind == ProjectOopHirNodeKind::FloatingConstant)
    {
        return normalizeFloating(node->floatingValue, node->typeKey);
    }
    if (node->kind == ProjectOopHirNodeKind::IntegerConstant)
    {
        const std::uint64_t bits = normalizeInteger(node->integerValue, node->typeKey);
        const double value = isSignedIntegralType(node->typeKey)
            ? static_cast<double>(signedInteger(bits, node->typeKey))
            : static_cast<double>(bits);
        return value;
    }
    if (node->kind == ProjectOopHirNodeKind::StaticFieldValue && node->memberOrdinal.has_value())
    {
        const ProjectOopMirStaticField* field = knownStaticField(*node->memberOrdinal, knownFields);
        if (field == nullptr) return std::nullopt;
        if (field->typeKey == "builtin:float" || field->typeKey == "builtin:double")
        {
            return field->constantFloatingValue;
        }
        if (ProjectTypeConversion::isIntegral(field->typeKey))
        {
            const std::uint64_t bits = normalizeInteger(field->constantIntegerValue, field->typeKey);
            return isSignedIntegralType(field->typeKey)
                ? static_cast<double>(signedInteger(bits, field->typeKey))
                : static_cast<double>(bits);
        }
        return std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Conversion && !node->children.empty())
    {
        const ProjectOopHirNode* child = node->children.front().get();
        if (const std::optional<double> floating = floatingConstantValue(child, knownFields); floating.has_value())
        {
            return normalizeFloating(*floating, node->typeKey);
        }
        return std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Unary && !node->children.empty())
    {
        const std::optional<double> value = floatingConstantValue(node->children.front().get(), knownFields);
        if (!value.has_value()) return std::nullopt;
        if (node->operation == lexer::TokenKind::Plus) return normalizeFloating(*value, node->typeKey);
        if (node->operation == lexer::TokenKind::Minus) return normalizeFloating(-*value, node->typeKey);
        return std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Binary && node->children.size() == 2)
    {
        const std::optional<double> left = floatingConstantValue(node->children[0].get(), knownFields);
        const std::optional<double> right = floatingConstantValue(node->children[1].get(), knownFields);
        if (!left.has_value() || !right.has_value()) return std::nullopt;
        double result = 0.0;
        switch (node->operation)
        {
            case lexer::TokenKind::Plus: result = *left + *right; break;
            case lexer::TokenKind::Minus: result = *left - *right; break;
            case lexer::TokenKind::Star: result = *left * *right; break;
            case lexer::TokenKind::Slash: result = *left / *right; break;
            case lexer::TokenKind::Percent: result = std::fmod(*left, *right); break;
            default: return std::nullopt;
        }
        return normalizeFloating(result, node->typeKey);
    }
    return std::nullopt;
}

std::optional<bool> booleanConstantValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    if (node == nullptr) return std::nullopt;
    if (node->kind == ProjectOopHirNodeKind::BooleanConstant) return node->booleanValue;
    if (node->kind == ProjectOopHirNodeKind::StaticFieldValue && node->memberOrdinal.has_value())
    {
        const ProjectOopMirStaticField* field = knownStaticField(*node->memberOrdinal, knownFields);
        return field != nullptr && field->typeKey == "builtin:bool"
            ? std::optional<bool>(field->constantBooleanValue)
            : std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Conversion && !node->children.empty())
    {
        return booleanConstantValue(node->children.front().get(), knownFields);
    }
    if (node->kind == ProjectOopHirNodeKind::Unary && node->operation == lexer::TokenKind::Bang &&
        !node->children.empty())
    {
        const std::optional<bool> value = booleanConstantValue(node->children.front().get(), knownFields);
        return value.has_value() ? std::optional<bool>(!*value) : std::nullopt;
    }
    if (node->kind != ProjectOopHirNodeKind::Binary || node->children.size() != 2)
    {
        return std::nullopt;
    }
    if (node->operation == lexer::TokenKind::AmpAmp || node->operation == lexer::TokenKind::PipePipe)
    {
        const std::optional<bool> left = booleanConstantValue(node->children[0].get(), knownFields);
        const std::optional<bool> right = booleanConstantValue(node->children[1].get(), knownFields);
        if (!left.has_value() || !right.has_value()) return std::nullopt;
        return node->operation == lexer::TokenKind::AmpAmp ? (*left && *right) : (*left || *right);
    }
    if (node->operation == lexer::TokenKind::EqualEqual || node->operation == lexer::TokenKind::BangEqual)
    {
        const std::optional<bool> leftBool = booleanConstantValue(node->children[0].get(), knownFields);
        const std::optional<bool> rightBool = booleanConstantValue(node->children[1].get(), knownFields);
        if (leftBool.has_value() && rightBool.has_value())
        {
            return node->operation == lexer::TokenKind::EqualEqual
                ? *leftBool == *rightBool
                : *leftBool != *rightBool;
        }
    }
    const bool floating = node->children[0]->typeKey == "builtin:float" ||
        node->children[0]->typeKey == "builtin:double";
    if (floating)
    {
        const std::optional<double> left = floatingConstantValue(node->children[0].get(), knownFields);
        const std::optional<double> right = floatingConstantValue(node->children[1].get(), knownFields);
        if (!left.has_value() || !right.has_value()) return std::nullopt;
        switch (node->operation)
        {
            case lexer::TokenKind::EqualEqual: return *left == *right;
            case lexer::TokenKind::BangEqual: return *left != *right;
            case lexer::TokenKind::Less: return *left < *right;
            case lexer::TokenKind::LessEqual: return *left <= *right;
            case lexer::TokenKind::Greater: return *left > *right;
            case lexer::TokenKind::GreaterEqual: return *left >= *right;
            default: return std::nullopt;
        }
    }
    const std::optional<std::uint64_t> left = integerConstantValue(node->children[0].get(), knownFields);
    const std::optional<std::uint64_t> right = integerConstantValue(node->children[1].get(), knownFields);
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    if (isSignedIntegralType(node->children[0]->typeKey))
    {
        const auto lhs = static_cast<std::int64_t>(*left);
        const auto rhs = static_cast<std::int64_t>(*right);
        switch (node->operation)
        {
            case lexer::TokenKind::EqualEqual: return lhs == rhs;
            case lexer::TokenKind::BangEqual: return lhs != rhs;
            case lexer::TokenKind::Less: return lhs < rhs;
            case lexer::TokenKind::LessEqual: return lhs <= rhs;
            case lexer::TokenKind::Greater: return lhs > rhs;
            case lexer::TokenKind::GreaterEqual: return lhs >= rhs;
            default: return std::nullopt;
        }
    }
    switch (node->operation)
    {
        case lexer::TokenKind::EqualEqual: return *left == *right;
        case lexer::TokenKind::BangEqual: return *left != *right;
        case lexer::TokenKind::Less: return *left < *right;
        case lexer::TokenKind::LessEqual: return *left <= *right;
        case lexer::TokenKind::Greater: return *left > *right;
        case lexer::TokenKind::GreaterEqual: return *left >= *right;
        default: return std::nullopt;
    }
}

}
const char* projectOopMirNodeKindName(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::Block: return "block";
        case ProjectOopMirNodeKind::LocalDeclaration: return "local-declaration";
        case ProjectOopMirNodeKind::AggregateInitialize: return "aggregate-initialize";
        case ProjectOopMirNodeKind::AggregateElementStore: return "aggregate-element-store";
        case ProjectOopMirNodeKind::ExpressionStatement: return "expression-statement";
        case ProjectOopMirNodeKind::If: return "if";
        case ProjectOopMirNodeKind::While: return "while";
        case ProjectOopMirNodeKind::DoWhile: return "do-while";
        case ProjectOopMirNodeKind::For: return "for";
        case ProjectOopMirNodeKind::Switch: return "switch";
        case ProjectOopMirNodeKind::SwitchSection: return "switch-section";
        case ProjectOopMirNodeKind::Break: return "break";
        case ProjectOopMirNodeKind::Continue: return "continue";
        case ProjectOopMirNodeKind::Return: return "return";
        case ProjectOopMirNodeKind::TryRegion: return "try-region";
        case ProjectOopMirNodeKind::CatchRegion: return "catch-region";
        case ProjectOopMirNodeKind::FinallyRegion: return "finally-region";
        case ProjectOopMirNodeKind::UsingRegion: return "using-region";
        case ProjectOopMirNodeKind::Throw: return "throw";
        case ProjectOopMirNodeKind::Rethrow: return "rethrow";
        case ProjectOopMirNodeKind::CleanupContinuation: return "cleanup-continuation";
        case ProjectOopMirNodeKind::OwnedParameterCleanup: return "owned-parameter-cleanup";
        case ProjectOopMirNodeKind::MoveValue: return "move-value";
        case ProjectOopMirNodeKind::MoveArrayElement: return "move-array-element";
        case ProjectOopMirNodeKind::MoveOwnedField: return "move-owned-field";
        case ProjectOopMirNodeKind::OwnedTemporaryBorrow: return "owned-temporary-borrow";
        case ProjectOopMirNodeKind::FullExpressionCleanup: return "full-expression-cleanup";
        case ProjectOopMirNodeKind::PointerLoad: return "pointer-load";
        case ProjectOopMirNodeKind::RawElementAddress: return "raw-element-address";
        case ProjectOopMirNodeKind::StackAllocation: return "stack-allocation";
        case ProjectOopMirNodeKind::RawAllocation: return "raw-allocation";
        case ProjectOopMirNodeKind::PointerArithmetic: return "pointer-arithmetic";
        case ProjectOopMirNodeKind::CompoundStore: return "compound-store";
        case ProjectOopMirNodeKind::ValueConstruct: return "value-construct";
        case ProjectOopMirNodeKind::TemporaryAddress: return "temporary-address";
        case ProjectOopMirNodeKind::AddressValue: return "address-value";
        case ProjectOopMirNodeKind::BorrowValue: return "borrow-value";
        case ProjectOopMirNodeKind::ThisValue: return "this";
        case ProjectOopMirNodeKind::ParameterValue: return "parameter-value";
        case ProjectOopMirNodeKind::LocalValue: return "local-value";
        case ProjectOopMirNodeKind::FieldLoad: return "field-load";
        case ProjectOopMirNodeKind::StaticFieldLoad: return "static-field-load";
        case ProjectOopMirNodeKind::ParameterAddress: return "parameter-address";
        case ProjectOopMirNodeKind::LocalAddress: return "local-address";
        case ProjectOopMirNodeKind::FieldAddress: return "field-address";
        case ProjectOopMirNodeKind::StaticFieldAddress: return "static-field-address";
        case ProjectOopMirNodeKind::IntegerConstant: return "integer-constant";
        case ProjectOopMirNodeKind::FloatingConstant: return "floating-constant";
        case ProjectOopMirNodeKind::BooleanConstant: return "boolean-constant";
        case ProjectOopMirNodeKind::NullConstant: return "null-constant";
        case ProjectOopMirNodeKind::DefaultValue: return "default-value";
        case ProjectOopMirNodeKind::StringLiteral: return "string-literal";
        case ProjectOopMirNodeKind::ArrayAllocate: return "array-allocate";
        case ProjectOopMirNodeKind::ArrayLength: return "array-length";
        case ProjectOopMirNodeKind::ArrayElementLoad: return "array-element-load";
        case ProjectOopMirNodeKind::ArrayElementAddress: return "array-element-address";
        case ProjectOopMirNodeKind::StringLength: return "string-length";
        case ProjectOopMirNodeKind::StringElementLoad: return "string-element-load";
        case ProjectOopMirNodeKind::EnumName: return "enum-name";
        case ProjectOopMirNodeKind::StringConcat: return "string-concat";
        case ProjectOopMirNodeKind::StringEquals: return "string-equals";
        case ProjectOopMirNodeKind::StringSubstring: return "string-substring";
        case ProjectOopMirNodeKind::StringCharacterPointer: return "string-character-pointer";
        case ProjectOopMirNodeKind::StringFromCharArray: return "string-from-char-array";
        case ProjectOopMirNodeKind::CoreConsoleWriteLine: return "core-console-writeline";
        case ProjectOopMirNodeKind::AggregateEquals: return "aggregate-equals";
        case ProjectOopMirNodeKind::AggregateHash: return "aggregate-hash";
        case ProjectOopMirNodeKind::CoreObjectTypeName: return "core-object-type-name";
        case ProjectOopMirNodeKind::CoreObjectReferenceHash: return "core-object-reference-hash";
        case ProjectOopMirNodeKind::CoreClockQueryCounter: return "core-clock-query-counter";
        case ProjectOopMirNodeKind::CoreClockQueryFrequency: return "core-clock-query-frequency";
        case ProjectOopMirNodeKind::CoreClockQuerySystemFileTime: return "core-clock-query-system-filetime";
        case ProjectOopMirNodeKind::CoreEnvironmentTickCount64: return "core-environment-tick-count64";
        case ProjectOopMirNodeKind::CoreEnvironmentProcessorCount: return "core-environment-processor-count";
        case ProjectOopMirNodeKind::CoreEnvironmentExit: return "core-environment-exit";
        case ProjectOopMirNodeKind::CoreEnvironmentGetVariable: return "core-environment-get-variable";
        case ProjectOopMirNodeKind::CoreEnvironmentSetVariable: return "core-environment-set-variable";
        case ProjectOopMirNodeKind::CoreFileExists: return "core-file-exists";
        case ProjectOopMirNodeKind::CoreFileDelete: return "core-file-delete";
        case ProjectOopMirNodeKind::CoreDirectoryExists: return "core-directory-exists";
        case ProjectOopMirNodeKind::CoreDirectoryCreate: return "core-directory-create";
        case ProjectOopMirNodeKind::CoreFileReadAllText: return "core-file-read-all-text";
        case ProjectOopMirNodeKind::CoreFileGetSize: return "core-file-get-size";
        case ProjectOopMirNodeKind::CoreFileReadAllBytes: return "core-file-read-all-bytes";
        case ProjectOopMirNodeKind::CoreFileReadBytes: return "core-file-read-bytes";
        case ProjectOopMirNodeKind::CoreFileWriteAllBytes: return "core-file-write-all-bytes";
        case ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory: return "core-environment-current-directory";
        case ProjectOopMirNodeKind::CoreProcessRun: return "core-process-run";
        case ProjectOopMirNodeKind::CoreFileWriteAllText: return "core-file-write-all-text";
        case ProjectOopMirNodeKind::CoreFileAppendAllText: return "core-file-append-all-text";
        case ProjectOopMirNodeKind::CoreDirectoryDelete: return "core-directory-delete";
        case ProjectOopMirNodeKind::CorePathGetFullPath: return "core-path-get-full-path";
        case ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount: return "core-environment-get-command-line-arg-count";
        case ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs: return "core-environment-get-command-line-args";
        case ProjectOopMirNodeKind::CoreDirectoryGetFileCount: return "core-directory-get-file-count";
        case ProjectOopMirNodeKind::CoreDirectoryGetFiles: return "core-directory-get-files";
        case ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount: return "core-directory-get-directory-count";
        case ProjectOopMirNodeKind::CoreDirectoryGetDirectories: return "core-directory-get-directories";
        case ProjectOopMirNodeKind::CoreThreadCreate: return "core-thread-create";
        case ProjectOopMirNodeKind::CoreThreadJoin: return "core-thread-join";
        case ProjectOopMirNodeKind::CoreThreadDetach: return "core-thread-detach";
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt32: return "core-interlocked-increment-int32";
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt32: return "core-interlocked-decrement-int32";
        case ProjectOopMirNodeKind::CoreInterlockedAddInt32: return "core-interlocked-add-int32";
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt32: return "core-interlocked-exchange-int32";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32: return "core-interlocked-compare-exchange-int32";
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt64: return "core-interlocked-increment-int64";
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt64: return "core-interlocked-decrement-int64";
        case ProjectOopMirNodeKind::CoreInterlockedAddInt64: return "core-interlocked-add-int64";
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt64: return "core-interlocked-exchange-int64";
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64: return "core-interlocked-compare-exchange-int64";
        case ProjectOopMirNodeKind::CoreMutexCreate: return "core-mutex-create";
        case ProjectOopMirNodeKind::CoreMutexLock: return "core-mutex-lock";
        case ProjectOopMirNodeKind::CoreMutexUnlock: return "core-mutex-unlock";
        case ProjectOopMirNodeKind::CoreMutexClose: return "core-mutex-close";
        case ProjectOopMirNodeKind::CoreMonitorEnter: return "core-monitor-enter";
        case ProjectOopMirNodeKind::CoreMonitorExit: return "core-monitor-exit";
        case ProjectOopMirNodeKind::CoreMonitorWait: return "core-monitor-wait";
        case ProjectOopMirNodeKind::CoreMonitorPulse: return "core-monitor-pulse";
        case ProjectOopMirNodeKind::CoreMonitorPulseAll: return "core-monitor-pulse-all";
        case ProjectOopMirNodeKind::CoreSemaphoreCreate: return "core-semaphore-create";
        case ProjectOopMirNodeKind::CoreSemaphoreWait: return "core-semaphore-wait";
        case ProjectOopMirNodeKind::CoreSemaphoreRelease: return "core-semaphore-release";
        case ProjectOopMirNodeKind::CoreSemaphoreClose: return "core-semaphore-close";
        case ProjectOopMirNodeKind::CoreEventCreate: return "core-event-create";
        case ProjectOopMirNodeKind::CoreEventSet: return "core-event-set";
        case ProjectOopMirNodeKind::CoreEventReset: return "core-event-reset";
        case ProjectOopMirNodeKind::CoreEventWait: return "core-event-wait";
        case ProjectOopMirNodeKind::CoreEventClose: return "core-event-close";
        case ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId: return "core-process-get-current-process-id";
        case ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize: return "core-process-get-working-set-size";
        case ProjectOopMirNodeKind::CoreProcessGetKernelTime: return "core-process-get-kernel-time";
        case ProjectOopMirNodeKind::CoreProcessGetUserTime: return "core-process-get-user-time";
        case ProjectOopMirNodeKind::CoreProcessGetPriorityClass: return "core-process-get-priority-class";
        case ProjectOopMirNodeKind::CoreProcessSetPriorityClass: return "core-process-set-priority-class";
        case ProjectOopMirNodeKind::CoreSystemInfoGetPageSize: return "core-system-info-get-page-size";
        case ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture: return "core-system-info-get-processor-architecture";
        case ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity: return "core-system-info-get-allocation-granularity";
        case ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes: return "core-system-info-get-total-physical-memory-bytes";
        case ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes: return "core-system-info-get-available-physical-memory-bytes";
        case ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent: return "core-system-info-get-memory-load-percent";
        case ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent: return "core-debug-is-debugger-present";
        case ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount: return "core-debug-capture-stack-frame-count";
        case ProjectOopMirNodeKind::CoreDebugOutputString: return "core-debug-output-string";
        case ProjectOopMirNodeKind::CoreThreadSleep: return "core-thread-sleep";
        case ProjectOopMirNodeKind::CoreFileCopy: return "core-file-copy";
        case ProjectOopMirNodeKind::CoreFileMove: return "core-file-move";
        case ProjectOopMirNodeKind::CoreDirectoryMove: return "core-directory-move";
        case ProjectOopMirNodeKind::DelegateCreate: return "delegate-create";
        case ProjectOopMirNodeKind::LambdaCreate: return "lambda-create";
        case ProjectOopMirNodeKind::ClosureCaptureLoad: return "closure-capture-load";
        case ProjectOopMirNodeKind::ClosureCaptureAddress: return "closure-capture-address";
        case ProjectOopMirNodeKind::CaptureCellReference: return "capture-cell-reference";
        case ProjectOopMirNodeKind::ClosureCaptureCellReference: return "closure-capture-cell-reference";
        case ProjectOopMirNodeKind::DelegateInvoke: return "delegate-invoke";
        case ProjectOopMirNodeKind::EventAdd: return "event-add";
        case ProjectOopMirNodeKind::EventRemove: return "event-remove";
        case ProjectOopMirNodeKind::EventInvoke: return "event-invoke";
        case ProjectOopMirNodeKind::Unary: return "unary";
        case ProjectOopMirNodeKind::Binary: return "binary";
        case ProjectOopMirNodeKind::Conditional: return "conditional";
        case ProjectOopMirNodeKind::Store: return "store";
        case ProjectOopMirNodeKind::Convert: return "convert";
        case ProjectOopMirNodeKind::RuntimeTypeTest: return "runtime-type-test";
        case ProjectOopMirNodeKind::SafeReferenceCast: return "safe-reference-cast";
        case ProjectOopMirNodeKind::CheckedReferenceCast: return "checked-reference-cast";
        case ProjectOopMirNodeKind::DirectInstanceCall: return "direct-instance-call";
        case ProjectOopMirNodeKind::DirectStaticCall: return "direct-static-call";
        case ProjectOopMirNodeKind::VirtualInstanceCall: return "virtual-instance-call";
        case ProjectOopMirNodeKind::InterfaceInstanceCall: return "interface-instance-call";
        case ProjectOopMirNodeKind::AllocateAndConstruct: return "allocate-and-construct";
        case ProjectOopMirNodeKind::Deallocate: return "deallocate";
    }
    return "unknown";
}

bool ProjectOopMirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    error.clear();
    if (built_)
    {
        error = "ProjectOopMirModel instances are single-use";
        return false;
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !objectLayoutModel.isBuilt() ||
        !virtualDispatchModel.isBuilt() || !interfaceModel.isBuilt() ||
        !destructionModel.isBuilt() || !hirModel.isBuilt())
    {
        error = "Stage 34 OOP MIR requires built OOP HIR and Stage 29 object layouts";
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    objectLayoutModel_ = &objectLayoutModel;
    virtualDispatchModel_ = &virtualDispatchModel;
    interfaceModel_ = &interfaceModel;
    destructionModel_ = &destructionModel;
    hirModel_ = &hirModel;
    for (const StructValueLayout& layout : objectLayoutModel.structLayouts())
    {
        structLayouts_.push_back(ProjectOopMirStructLayout{
            layout.typeOrdinal, layout.typeKey, layout.size, layout.alignment,
            layout.isUnion, layout.pack, layout.hasExplicitFieldOffsets, layout.fields});
    }
    for (const ClassObjectLayout& layout : objectLayoutModel.classLayouts())
    {
        classLayouts_.push_back(ProjectOopMirClassLayout{
            layout.typeOrdinal,
            layout.typeKey,
            layout.size,
            layout.alignment,
            layout.headerSize,
            layout.baseTypeOrdinal,
            layout.baseSubobjectSize,
            layout.fields});
    }
    functionByMember_.resize(memberModel.members().size());
    dispatchTableByType_.resize(nameLookup.namedTypes().size());

    for (const ClassVirtualTable& sourceTable : virtualDispatchModel.classTables())
    {
        if (sourceTable.typeOrdinal >= nameLookup.namedTypes().size())
        {
            error = "Macro A virtual table refers to an invalid class ordinal";
            return false;
        }
        ProjectOopMirDispatchTable table;
        table.isImported = nameLookup.namedTypes()[sourceTable.typeOrdinal].isPrecompiled;
        table.genericSpecializationKey = genericSpecializationKeyForType(nameLookup, sourceTable.typeOrdinal);
        table.typeOrdinal = sourceTable.typeOrdinal;
        table.baseTypeOrdinal = sourceTable.baseTypeOrdinal;
        table.typeKey = "type:" + nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key();
        const std::string suffix = sanitizeDispatchName(nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key());
        table.descriptorName = "__nsc_classdesc_" + suffix;
        table.descriptorAccessorName = "__nsc_get_classdesc_" + suffix;
        table.vtableName = "__nsc_vtable_" + suffix;
        table.interfaceMapName = "__nsc_ifmap_" + suffix;
        table.stableTypeId = stableTypeId(table.typeKey);
        // C8: the SHORT name, not the fully-qualified key -- `Dictionary`, not
        // `Proj::NativeSharp.Core.Collections.Generic.Dictionary`. That is what a ToString()
        // default is useful for, and it matches what real C#'s GetType().Name gives.
        {
            const std::string& identity = nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key();
            const std::size_t lastDot = identity.find_last_of(".:");
            table.typeDisplayName = lastDot == std::string::npos
                ? identity : identity.substr(lastDot + 1);
        }
        table.typeNameGlobalName = "__nsc_typename_" + suffix;
        table.slotMemberOrdinals = sourceTable.slotMemberOrdinals;
        for (std::size_t slot = 0; slot < sourceTable.slotMemberOrdinals.size(); ++slot)
        {
            const std::size_t slotMember = sourceTable.slotMemberOrdinals[slot];
            const bool abstractSlot = slot < sourceTable.slotAbstract.size() && sourceTable.slotAbstract[slot];
            if (abstractSlot)
            {
                table.slotCalleeNames.emplace_back();
                continue;
            }
            const ProjectOopHirFunction* target = hirModel.functionForMember(slotMember);
            if (target == nullptr)
            {
                error = "Stage 35 concrete vtable slot refers to a method without OOP HIR body";
                return false;
            }
            table.slotCalleeNames.push_back(target->mangledName);
        }
        const ClassDestructionPlan* destruction = destructionModel.planForType(table.typeOrdinal);
        if (destruction == nullptr)
        {
            error = "Stage 35 dispatch table has no class destruction plan";
            return false;
        }
        table.destructionEntryName = destruction->destructionEntryName;
        table.destructorMemberOrdinals = destruction->destructorMemberOrdinals;
        if (!table.isImported)
        {
            for (const std::size_t destructorMember : destruction->destructorMemberOrdinals)
            {
                if (destructorMember < memberModel.members().size() &&
                    memberModel.members()[destructorMember].isPrecompiled)
                {
                    continue;
                }
                const ProjectOopHirFunction* destructorBody = hirModel.functionForMember(destructorMember);
                if (destructorBody == nullptr || !destructorBody->destructor)
                {
                    error = "Stage 35 destruction plan refers to a destructor without OOP HIR body";
                    return false;
                }
                table.destructorCalleeNames.push_back(destructorBody->mangledName);
            }
        }

        for (const std::size_t interfaceOrdinal : interfaceModel.interfacesOfClass(table.typeOrdinal))
        {
            const ClassInterfaceTablePlan* plan = interfaceModel.tablePlan(table.typeOrdinal, interfaceOrdinal);
            if (plan == nullptr)
            {
                error = "Stage 36 class interface map has no semantic table plan";
                return false;
            }
            ProjectOopMirDispatchTable::InterfaceTable interfaceTable;
            interfaceTable.interfaceTypeOrdinal = interfaceOrdinal;
            interfaceTable.interfaceTypeKey = interfaceModel.typeKeyForOrdinal(interfaceOrdinal);
            interfaceTable.stableInterfaceTypeId = plan->stableInterfaceTypeId;
            interfaceTable.methodTableName = "__nsc_iftable_" + suffix + "__" +
                sanitizeDispatchName(nameLookup.namedTypes()[interfaceOrdinal].identity.key());
            interfaceTable.interfaceMemberOrdinals = plan->interfaceMethodMemberOrdinals;
            for (const std::optional<std::size_t> implementation : plan->implementationMemberOrdinals)
            {
                if (!implementation.has_value())
                {
                    interfaceTable.implementationMemberOrdinals.push_back(memberModel.members().size());
                    interfaceTable.calleeNames.emplace_back();
                    continue;
                }
                interfaceTable.implementationMemberOrdinals.push_back(*implementation);
                const ProjectOopHirFunction* implementationFunction = hirModel.functionForMember(*implementation);
                if (implementationFunction == nullptr)
                {
                    error = "Stage 36 concrete interface implementation has no OOP HIR function";
                    return false;
                }
                interfaceTable.calleeNames.push_back(implementationFunction->mangledName);
            }
            table.interfaceTables.push_back(std::move(interfaceTable));
        }
        dispatchTableByType_[table.typeOrdinal] = dispatchTables_.size();
        dispatchTables_.push_back(std::move(table));
    }

    // Bloque 1 item 1 (owning field storage) -- collect, per class, the instance fields it OWNS, so
    // the destruction entry can release them without the author writing a `delete` by hand. Done as
    // a pass of its own AFTER every dispatch table exists, because deciding whether a field's type
    // is destroyable means asking whether that type has a class descriptor at all -- and that is
    // exactly the set of types that got a table above.
    //
    // Only fields whose type genuinely owns a separate allocation are listed. A value-typed `owned`
    // field owns no heap block for the traversal to release, which is the same rule already applied
    // to `owned` PARAMETERS (requiresOwnedDestruction) and to `owned` LOCALS
    // (ProjectMethodBodyModel) -- the three spellings agree on purpose.
    {
        std::unordered_set<std::string> destroyableClassTypeKeys;
        for (const ProjectOopMirDispatchTable& built : dispatchTables_)
        {
            destroyableClassTypeKeys.insert(built.typeKey);
        }
        const auto fieldOwnsAllocation = [&](const ClassMemberSymbol& member) {
            if (member.declaredTypeKey == "builtin:string") return true;
            return destroyableClassTypeKeys.contains(member.declaredTypeKey);
        };
        for (ProjectOopMirDispatchTable& built : dispatchTables_)
        {
            // The producer already emitted this complete plan behind destructionEntryName.
            // Rebuilding it would require importing private destructor bodies and field layout.
            if (built.isImported) continue;
            // The chain is walked MOST-DERIVED FIRST, and within each class the owned fields are
            // taken in REVERSE declaration order -- destruction is the exact reverse of
            // construction. This is the frozen order contract; the full statement and its
            // rationale live on ProjectOopMirDispatchTable::destructionSteps.
            std::vector<std::size_t> chain;
            for (std::optional<std::size_t> current = built.typeOrdinal; current.has_value();)
            {
                chain.push_back(*current);
                const std::optional<std::size_t> tableIndex =
                    (*current < dispatchTableByType_.size())
                        ? dispatchTableByType_[*current]
                        : std::nullopt;
                current = tableIndex.has_value()
                    ? dispatchTables_[*tableIndex].baseTypeOrdinal
                    : std::nullopt;
            }
            for (const std::size_t chainTypeOrdinal : chain)
            {
                // The precompiled producer owns the complete remainder of this base chain,
                // including private destructor bodies and private owned-field layout. Calling its
                // descriptor's destruction entry is the only complete operation available to a
                // consumer, and it is terminal because that entry also releases the allocation.
                if (chainTypeOrdinal < nameLookup.namedTypes().size() &&
                    nameLookup.namedTypes()[chainTypeOrdinal].isPrecompiled)
                {
                    built.delegatedBaseDestructionTypeOrdinal = chainTypeOrdinal;
                    break;
                }
                // This class's destructor body first: it runs while every one of its own owned
                // fields is still alive, and the null-guarded releases right after it skip
                // anything it already discharged by hand.
                const ClassDestructionPlan* chainPlan = destructionModel.planForType(chainTypeOrdinal);
                if (chainPlan != nullptr && chainPlan->explicitDestructorMemberOrdinal.has_value())
                {
                    const ProjectOopHirFunction* body =
                        hirModel.functionForMember(*chainPlan->explicitDestructorMemberOrdinal);
                    if (body == nullptr || !body->destructor)
                    {
                        error = "Bloque 1 destruction order refers to a destructor without OOP HIR body";
                        return false;
                    }
                    ProjectOopMirDispatchTable::DestructionStep bodyStep;
                    bodyStep.destructorCalleeName = body->mangledName;
                    built.destructionSteps.push_back(std::move(bodyStep));
                }
                const std::span<const std::size_t> classMembers =
                    memberModel.membersOfType(chainTypeOrdinal);
                for (auto memberIt = classMembers.rbegin(); memberIt != classMembers.rend(); ++memberIt)
                {
                    const std::size_t memberOrdinal = *memberIt;
                    const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
                    if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
                    if (member.declaredOwnership != sema::OwnershipKind::Owned) continue;
                    if (!fieldOwnsAllocation(member)) continue;
                    const ObjectFieldLayout* layout = objectLayoutModel.fieldLayoutForMember(memberOrdinal);
                    if (layout == nullptr)
                    {
                        error = "Bloque 1 owned field has no object layout";
                        return false;
                    }
                    const ProjectOopMirDispatchTable::OwnedField ownedField{
                        memberOrdinal, layout->offset, member.declaredTypeKey == "builtin:string"};
                    built.ownedFields.push_back(ownedField);
                    ProjectOopMirDispatchTable::DestructionStep fieldStep;
                    fieldStep.ownedField = ownedField;
                    fieldStep.isFieldRelease = true;
                    built.destructionSteps.push_back(std::move(fieldStep));
                }
            }
        }
    }

    for (const ProjectOopHirFunction& source : hirModel.functions())
    {
        ProjectOopMirFunction function;
        function.memberOrdinal = source.memberOrdinal;
        function.declaringTypeOrdinal = source.declaringTypeOrdinal;
        function.constructor = source.constructor;
        function.destructor = source.destructor;
        function.isStatic = source.isStatic;
        function.isExportedC = source.isExportedC;
        function.isImportedC = source.isImportedC;
        function.isImportedNative = source.isImportedNative;
        function.isSyntheticImplicitConstructor = source.isSyntheticImplicitConstructor;
        function.importDllName = source.importDllName;
        function.importRequiresRuntimeBinding = source.importRequiresRuntimeBinding;
        function.cAbiName = source.cAbiName;
        function.isLambda = source.isLambda;
        function.lambdaOrdinal = source.lambdaOrdinal;
        function.hasClosureEnvironment = source.hasClosureEnvironment;
        function.captureTypeKeys = source.captureTypeKeys;
        function.captureNames = source.captureNames;
        function.capturedLocalOrdinals = source.capturedLocalOrdinals;
        function.capturedParameterOrdinals = source.capturedParameterOrdinals;
        function.capturesThisCell = source.capturesThisCell;
        function.capturedLocalOrdinals = source.capturedLocalOrdinals;
        function.capturedParameterOrdinals = source.capturedParameterOrdinals;
        function.capturesThisCell = source.capturesThisCell;
        function.genericSpecializationKey = source.genericSpecializationKey;
        function.mangledName = source.mangledName;
        function.returnTypeKey = source.returnTypeKey;
        function.returnOwnership = source.returnOwnership;
        function.parameterNames = source.parameterNames;
        function.parameterTypeKeys = source.parameterTypeKeys;
        function.parameterOwnerships = source.parameterOwnerships;
        function.localOrdinals = source.localOrdinals;
        if (source.isImportedC || source.isImportedNative)
        {
            // Interop Foundation: imported C functions have no body to lower -- the definition
            // lives in the external DLL. The record still flows through so call sites resolve
            // and the backend declares it.
            functionByMember_[function.memberOrdinal] = functions_.size();
            functions_.push_back(std::move(function));
            continue;
        }
        function.body = lowerNode(source.body.get(), error);
        if (function.body == nullptr)
        {
            // Say WHERE. An internal error from this pipeline used to arrive as a bare sentence --
            // "Stage 34 cannot find Stage 29 allocation plan" and nothing else: no file, no line,
            // no method. In a 300-line source file that turned a one-line bug into nine steps of
            // manual bisection, which is exactly what happened porting objloader, and the cost is
            // paid again by every internal error after it. The mangled name is the cheapest thing
            // here that actually narrows the search, and it is always available.
            error += "\n  while lowering '" + source.mangledName + "'";
            return false;
        }
        if (!function.isLambda && !function.isSyntheticImplicitConstructor)
            functionByMember_[function.memberOrdinal] = functions_.size();
        functions_.push_back(std::move(function));
    }

    for (const ProjectOopHirFieldInitializer& source : hirModel.fieldInitializers())
    {
        auto value = lowerNode(source.value.get(), error);
        if (value == nullptr)
        {
            // Same reason as the function body above: a field initializer that fails to lower
            // otherwise reports nothing at all about which field it was.
            error += "\n  while lowering the field initializer of member #" +
                std::to_string(source.memberOrdinal);
            return false;
        }
        fieldInitializers_.push_back(ProjectOopMirFieldInitializer{
            source.memberOrdinal,
            source.declaringTypeOrdinal,
            source.isStatic,
            source.isConst,
            std::move(value)});
    }

    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind == ClassMemberKind::Field && field.isStatic)
        {
            ProjectOopMirStaticField staticField{
                memberOrdinal,
                field.declaringTypeOrdinal,
                field.name,
                field.declaredTypeKey,
                field.isReadonly,
                field.isConst,
                field.isPrecompiled,
                genericSpecializationKeyForType(nameLookup, field.declaringTypeOrdinal),
                {},
                {}};
            if (field.declaringTypeOrdinal >= nameLookup.namedTypes().size())
            {
                error = "Stage 37 static field has an invalid declaring type ordinal";
                return false;
            }
            const std::string typeFragment = compilation::symbolFragmentForIdentityKey(
                nameLookup.namedTypes()[field.declaringTypeOrdinal].identity.key());
            const std::string fieldFragment = compilation::symbolFragmentForIdentityKey(field.name);
            staticField.storageSymbol = "__nsc_static_" + typeFragment + "__" + fieldFragment;
            staticField.accessorName = "__nsc_get_static_" + typeFragment + "__" + fieldFragment;
            staticField.isEnumConstant = field.isEnumConstant;
            if (field.isConst)
            {
                for (const ProjectOopHirFieldInitializer& initializer : hirModel.fieldInitializers())
                {
                    if (initializer.memberOrdinal != memberOrdinal || initializer.value == nullptr)
                    {
                        continue;
                    }
                    // Stage 51.x: an enum member's declaredTypeKey is its own enum type (see the
                    // comment in ProjectMemberModel::collectEnumMembers), so isIntegral() alone
                    // would miss it here -- it is still an int-sized compile-time constant.
                    if (ProjectTypeConversion::isIntegral(field.declaredTypeKey) || field.isEnumConstant)
                    {
                        const std::optional<std::uint64_t> value = integerConstantValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantIntegerValue = *value;
                        }
                    }
                    else if (field.declaredTypeKey == "builtin:bool")
                    {
                        const std::optional<bool> value = booleanConstantValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantBooleanValue = *value;
                        }
                    }
                    else if (field.declaredTypeKey == "builtin:float" || field.declaredTypeKey == "builtin:double")
                    {
                        const std::optional<double> value = floatingConstantValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantFloatingValue = *value;
                        }
                    }
                    else if (field.declaredTypeKey == "builtin:string")
                    {
                        const std::optional<std::string> value =
                            stringConstantValue(initializer.value.get(), staticFields_);
                        if (value.has_value())
                        {
                            staticField.hasConstantInitializer = true;
                            staticField.constantStringValue = *value;
                        }
                    }
                    break;
                }
                if (!staticField.hasConstantInitializer)
                {
                    error = "Stage 37 const initializer validated semantically but could not be materialized as a native constant";
                    return false;
                }
            }
            staticFields_.push_back(std::move(staticField));
        }
    }

    built_ = true;
    return true;
}

std::span<const ProjectOopMirFunction> ProjectOopMirModel::functions() const noexcept
{
    return functions_;
}

std::span<const ProjectOopMirFieldInitializer> ProjectOopMirModel::fieldInitializers() const noexcept
{
    return fieldInitializers_;
}

std::span<const ProjectOopMirStaticField> ProjectOopMirModel::staticFields() const noexcept
{
    return staticFields_;
}

std::span<const ProjectOopMirStructLayout> ProjectOopMirModel::structLayouts() const noexcept
{
    return structLayouts_;
}

std::span<const ProjectOopMirClassLayout> ProjectOopMirModel::classLayouts() const noexcept
{
    return classLayouts_;
}

std::span<const ProjectOopMirDispatchTable> ProjectOopMirModel::dispatchTables() const noexcept
{
    return dispatchTables_;
}

const ProjectOopMirDispatchTable* ProjectOopMirModel::dispatchTableForType(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= dispatchTableByType_.size() || !dispatchTableByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *dispatchTableByType_[typeOrdinal];
    return index < dispatchTables_.size() ? &dispatchTables_[index] : nullptr;
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForMember(std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= functionByMember_.size() || !functionByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByMember_[memberOrdinal]];
}

bool ProjectOopMirModel::isBuilt() const noexcept
{
    return built_;
}

std::unique_ptr<ProjectOopMirNode> ProjectOopMirModel::lowerNode(
    const ProjectOopHirNode* node,
    std::string& error)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    auto result = std::make_unique<ProjectOopMirNode>();
    result->syntax = node->syntax;
    result->typeKey = node->typeKey;
    result->elementTypeKey = node->elementTypeKey;
    result->stringValue = node->stringValue;
    result->operation = node->operation;
    result->memberOrdinal = node->memberOrdinal;
    result->secondaryMemberOrdinal = node->secondaryMemberOrdinal;
    result->localOrdinal = node->localOrdinal;
    result->parameterOrdinal = node->parameterOrdinal;
    result->constructionOrdinal = node->constructionOrdinal;
    result->targetTypeOrdinal = node->targetTypeOrdinal;
    result->exceptionTypeOrdinal = node->exceptionTypeOrdinal;
    result->cleanupRegionOrdinal = node->cleanupRegionOrdinal;
    result->mirRegionOrdinal = node->hirRegionOrdinal;
    result->exceptionTargetRegionOrdinal = node->exceptionTargetRegionOrdinal;
    result->targetScopeOrdinal = node->targetScopeOrdinal;
    result->disposeContractMemberOrdinal = node->disposeContractMemberOrdinal;
    result->disposeImplementationMemberOrdinal = node->disposeImplementationMemberOrdinal;
    result->cleanupRegionOrdinals = node->cleanupRegionOrdinals;
    result->cleanupContinuationKind = node->cleanupContinuationKind;
    result->borrowKind = node->borrowKind;
    result->localBorrowSourceKind = node->localBorrowSourceKind;
    if (node->conversion.has_value())
    {
        result->conversionKind = node->conversion->kind;
    }
    // SwitchSection only. Copied verbatim: the values were folded once, in the body model.
    result->switchLabels = node->switchLabels;
    result->switchHasDefault = node->switchHasDefault;
    result->integerValue = node->integerValue;
    result->floatingValue = node->floatingValue;
    result->booleanValue = node->booleanValue;
    result->baseReceiver = node->baseReceiver;
    result->targetIsInterface = node->targetIsInterface;
    result->dynamicDestruction = node->dynamicDestruction;
    result->valueTypeNoOpDeletion = node->valueTypeNoOpDeletion;
    result->catchAll = node->catchAll;
    result->interfaceCleanupDispatch = node->interfaceCleanupDispatch;
    result->argumentCount = node->argumentCount;
    result->argumentParameterOrdinals = node->argumentParameterOrdinals;
    result->argumentTypeKeys = node->argumentTypeKeys;

    switch (node->kind)
    {
        case ProjectOopHirNodeKind::Block: result->kind = ProjectOopMirNodeKind::Block; break;
        case ProjectOopHirNodeKind::LocalDeclaration: result->kind = ProjectOopMirNodeKind::LocalDeclaration; break;
        case ProjectOopHirNodeKind::ExpressionStatement: result->kind = ProjectOopMirNodeKind::ExpressionStatement; break;
        case ProjectOopHirNodeKind::If: result->kind = ProjectOopMirNodeKind::If; break;
        case ProjectOopHirNodeKind::While: result->kind = ProjectOopMirNodeKind::While; break;
        case ProjectOopHirNodeKind::DoWhile: result->kind = ProjectOopMirNodeKind::DoWhile; break;
        case ProjectOopHirNodeKind::Conditional: result->kind = ProjectOopMirNodeKind::Conditional; break;
        case ProjectOopHirNodeKind::For: result->kind = ProjectOopMirNodeKind::For; break;
        case ProjectOopHirNodeKind::Switch: result->kind = ProjectOopMirNodeKind::Switch; break;
        case ProjectOopHirNodeKind::SwitchSection:
            result->kind = ProjectOopMirNodeKind::SwitchSection;
            break;
        case ProjectOopHirNodeKind::Break: result->kind = ProjectOopMirNodeKind::Break; break;
        case ProjectOopHirNodeKind::Continue: result->kind = ProjectOopMirNodeKind::Continue; break;
        case ProjectOopHirNodeKind::Return: result->kind = ProjectOopMirNodeKind::Return; break;
        case ProjectOopHirNodeKind::TryRegion: result->kind = ProjectOopMirNodeKind::TryRegion; break;
        case ProjectOopHirNodeKind::CatchRegion: result->kind = ProjectOopMirNodeKind::CatchRegion; break;
        case ProjectOopHirNodeKind::FinallyRegion: result->kind = ProjectOopMirNodeKind::FinallyRegion; break;
        case ProjectOopHirNodeKind::UsingRegion: result->kind = ProjectOopMirNodeKind::UsingRegion; break;
        case ProjectOopHirNodeKind::Throw: result->kind = ProjectOopMirNodeKind::Throw; break;
        case ProjectOopHirNodeKind::Rethrow: result->kind = ProjectOopMirNodeKind::Rethrow; break;
        case ProjectOopHirNodeKind::CleanupContinuation:
            result->kind = ProjectOopMirNodeKind::CleanupContinuation;
            break;
        case ProjectOopHirNodeKind::OwnedParameterCleanup:
            result->kind = ProjectOopMirNodeKind::OwnedParameterCleanup;
            break;
        case ProjectOopHirNodeKind::MoveValue:
            result->kind = ProjectOopMirNodeKind::MoveValue;
            break;
        case ProjectOopHirNodeKind::MoveArrayElement:
            result->kind = ProjectOopMirNodeKind::MoveArrayElement;
            break;
        case ProjectOopHirNodeKind::MoveOwnedField:
            result->kind = ProjectOopMirNodeKind::MoveOwnedField;
            break;
        case ProjectOopHirNodeKind::OwnedTemporaryBorrow:
            result->kind = ProjectOopMirNodeKind::OwnedTemporaryBorrow;
            break;
        case ProjectOopHirNodeKind::FullExpressionCleanup:
            result->kind = ProjectOopMirNodeKind::FullExpressionCleanup;
            break;
        case ProjectOopHirNodeKind::RawElementAddress:
        {
            // C2.1 "fixed arrays": the element SIZE and ALIGNMENT the GEP needs. Same layout
            // question the managed-array path above already answers, asked through the same
            // helper, so a struct element gets its real size rather than a pointer's.
            if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr)
            {
                error = "C2.1 raw element lowering requires name lookup and value layouts";
                return nullptr;
            }
            const auto layout = elementLayout(node->elementTypeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "C2.1 cannot determine raw element layout for '" + node->elementTypeKey + "'";
                return nullptr;
            }
            result->immediate = layout->first;
            result->alignment = layout->second;
            result->kind = ProjectOopMirNodeKind::RawElementAddress;
            break;
        }
        // CC1 fases 4B, 4C y 4D. Las tres necesitan LO MISMO -- cuanto mide un elemento -- y las
        // tres lo piden por `elementLayout`, que es el mismo helper del que sale el stride de
        // `p[i]` y el offset de un campo. Ni una tabla de tamanos nueva: dos reglas de tamano que
        // pudieran divergir es exactamente el defecto que este milestone existe para quitar.
        case ProjectOopHirNodeKind::StackAllocation:
        case ProjectOopHirNodeKind::RawAllocation:
        case ProjectOopHirNodeKind::PointerArithmetic:
        {
            if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr)
            {
                error = "CC1 raw storage lowering requires name lookup and value layouts";
                return nullptr;
            }
            const auto layout = elementLayout(node->elementTypeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "CC1 cannot determine the element layout of '" + node->elementTypeKey + "'";
                return nullptr;
            }
            result->immediate = layout->first;
            result->alignment = layout->second;
            result->kind = node->kind == ProjectOopHirNodeKind::StackAllocation
                ? ProjectOopMirNodeKind::StackAllocation
                : (node->kind == ProjectOopHirNodeKind::RawAllocation
                       ? ProjectOopMirNodeKind::RawAllocation
                       : ProjectOopMirNodeKind::PointerArithmetic);
            break;
        }
        case ProjectOopHirNodeKind::PointerLoad:
            result->kind = ProjectOopMirNodeKind::PointerLoad;
            break;
        case ProjectOopHirNodeKind::AddressValue:
            result->kind = ProjectOopMirNodeKind::AddressValue;
            break;
        case ProjectOopHirNodeKind::BorrowValue:
            result->kind = ProjectOopMirNodeKind::BorrowValue;
            break;
        case ProjectOopHirNodeKind::ThisValue: result->kind = ProjectOopMirNodeKind::ThisValue; break;
        case ProjectOopHirNodeKind::ParameterValue: result->kind = ProjectOopMirNodeKind::ParameterValue; break;
        case ProjectOopHirNodeKind::LocalValue: result->kind = ProjectOopMirNodeKind::LocalValue; break;
        case ProjectOopHirNodeKind::ParameterAddress: result->kind = ProjectOopMirNodeKind::ParameterAddress; break;
        case ProjectOopHirNodeKind::LocalAddress: result->kind = ProjectOopMirNodeKind::LocalAddress; break;
        case ProjectOopHirNodeKind::IntegerConstant: result->kind = ProjectOopMirNodeKind::IntegerConstant; break;
        case ProjectOopHirNodeKind::FloatingConstant: result->kind = ProjectOopMirNodeKind::FloatingConstant; break;
        case ProjectOopHirNodeKind::BooleanConstant: result->kind = ProjectOopMirNodeKind::BooleanConstant; break;
        case ProjectOopHirNodeKind::NullConstant: result->kind = ProjectOopMirNodeKind::NullConstant; break;
        case ProjectOopHirNodeKind::DefaultValue: result->kind = ProjectOopMirNodeKind::DefaultValue; break;
        case ProjectOopHirNodeKind::StringLiteral:
            result->kind = ProjectOopMirNodeKind::StringLiteral;
            break;
        case ProjectOopHirNodeKind::ArrayLength:
            result->kind = ProjectOopMirNodeKind::ArrayLength;
            break;
        case ProjectOopHirNodeKind::StringLength:
            result->kind = ProjectOopMirNodeKind::StringLength;
            break;
        case ProjectOopHirNodeKind::StringElementValue:
            result->kind = ProjectOopMirNodeKind::StringElementLoad;
            result->immediate = 1;
            result->alignment = 1;
            break;
        case ProjectOopHirNodeKind::EnumName:
            result->kind = ProjectOopMirNodeKind::EnumName;
            break;
        case ProjectOopHirNodeKind::StringConcat:
            result->kind = ProjectOopMirNodeKind::StringConcat;
            break;
        case ProjectOopHirNodeKind::StringEquals:
            result->kind = ProjectOopMirNodeKind::StringEquals;
            break;
        case ProjectOopHirNodeKind::StringCharacterPointer:
            result->kind = ProjectOopMirNodeKind::StringCharacterPointer;
            break;
        case ProjectOopHirNodeKind::StringSubstring:
            result->kind = ProjectOopMirNodeKind::StringSubstring;
            break;
        case ProjectOopHirNodeKind::StringFromCharArray:
            result->kind = ProjectOopMirNodeKind::StringFromCharArray;
            break;
        // SHR-022. La mascara de dos bits que el HIR fijo pasa a `immediate`, que es de donde el
        // backend saca los operandos constantes. El prologo generico de esta funcion ya copia
        // `integerValue`, pero ESE campo no lo mira nadie aguas abajo para este nodo: sin esta
        // linea la mascara se queda en el HIR y la llamada al runtime sale con lo que hubiera en
        // `immediate` -- cero -- asi que las cuatro salidas escribirian por salida estandar y sin
        // salto de linea, en silencio.
        case ProjectOopHirNodeKind::CoreConsoleWriteLine:
            result->kind = ProjectOopMirNodeKind::CoreConsoleWriteLine;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::AggregateEquals:
        case ProjectOopHirNodeKind::AggregateHash:
        {
            // C8 struct half: the aggregate's size and alignment, so the backend can spill the
            // operands into slots it can walk. The leaves themselves already carry their offsets
            // from the HIR pass and are copied generically with the rest of the children.
            if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr)
            {
                error = "C8 struct root member requires name lookup and value layouts";
                return nullptr;
            }
            const auto layout = elementLayout(node->elementTypeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "C8 cannot determine the layout of struct '" + node->elementTypeKey + "'";
                return nullptr;
            }
            result->kind = node->kind == ProjectOopHirNodeKind::AggregateEquals
                ? ProjectOopMirNodeKind::AggregateEquals
                : ProjectOopMirNodeKind::AggregateHash;
            result->immediate = layout->first;
            result->alignment = layout->second;
            break;
        }
        case ProjectOopHirNodeKind::CoreObjectTypeName:
            result->kind = ProjectOopMirNodeKind::CoreObjectTypeName;
            break;
        case ProjectOopHirNodeKind::CoreObjectReferenceHash:
            result->kind = ProjectOopMirNodeKind::CoreObjectReferenceHash;
            break;
        case ProjectOopHirNodeKind::CoreClockQueryCounter:
            result->kind = ProjectOopMirNodeKind::CoreClockQueryCounter;
            break;
        case ProjectOopHirNodeKind::CoreClockQueryFrequency:
            result->kind = ProjectOopMirNodeKind::CoreClockQueryFrequency;
            break;
        case ProjectOopHirNodeKind::CoreClockQuerySystemFileTime:
            result->kind = ProjectOopMirNodeKind::CoreClockQuerySystemFileTime;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentTickCount64:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentTickCount64;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentProcessorCount:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentProcessorCount;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentExit:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentExit;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentGetVariable:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentGetVariable;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentSetVariable:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentSetVariable;
            break;
        case ProjectOopHirNodeKind::CoreFileExists:
            result->kind = ProjectOopMirNodeKind::CoreFileExists;
            break;
        case ProjectOopHirNodeKind::CoreFileDelete:
            result->kind = ProjectOopMirNodeKind::CoreFileDelete;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryExists:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryExists;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryCreate:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryCreate;
            break;
        case ProjectOopHirNodeKind::CoreFileReadAllText:
            result->kind = ProjectOopMirNodeKind::CoreFileReadAllText;
            break;
        case ProjectOopHirNodeKind::CoreFileWriteAllText:
            result->kind = ProjectOopMirNodeKind::CoreFileWriteAllText;
            break;
        case ProjectOopHirNodeKind::CoreFileAppendAllText:
            result->kind = ProjectOopMirNodeKind::CoreFileAppendAllText;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryDelete:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryDelete;
            break;
        case ProjectOopHirNodeKind::CorePathGetFullPath:
            result->kind = ProjectOopMirNodeKind::CorePathGetFullPath;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgCount:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgs:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs;
            break;
        case ProjectOopHirNodeKind::CoreFileGetSize:
            result->kind = ProjectOopMirNodeKind::CoreFileGetSize;
            break;
        case ProjectOopHirNodeKind::CoreFileReadAllBytes:
            result->kind = ProjectOopMirNodeKind::CoreFileReadAllBytes;
            break;
        case ProjectOopHirNodeKind::CoreFileReadBytes:
            result->kind = ProjectOopMirNodeKind::CoreFileReadBytes;
            break;
        case ProjectOopHirNodeKind::CoreFileWriteAllBytes:
            result->kind = ProjectOopMirNodeKind::CoreFileWriteAllBytes;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentCurrentDirectory:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory;
            break;
        case ProjectOopHirNodeKind::CoreProcessRun:
            result->kind = ProjectOopMirNodeKind::CoreProcessRun;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetFileCount:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetFileCount;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetFiles:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetFiles;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetDirectoryCount:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetDirectories:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetDirectories;
            break;
        case ProjectOopHirNodeKind::CoreThreadCreate:
            result->kind = ProjectOopMirNodeKind::CoreThreadCreate;
            break;
        case ProjectOopHirNodeKind::CoreThreadJoin:
            result->kind = ProjectOopMirNodeKind::CoreThreadJoin;
            break;
        case ProjectOopHirNodeKind::CoreThreadDetach:
            result->kind = ProjectOopMirNodeKind::CoreThreadDetach;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedIncrementInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedIncrementInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedDecrementInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedDecrementInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedAddInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedAddInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedExchangeInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedExchangeInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedIncrementInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedIncrementInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedDecrementInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedDecrementInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedAddInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedAddInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedExchangeInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedExchangeInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64;
            break;
        case ProjectOopHirNodeKind::CoreMutexCreate:
            result->kind = ProjectOopMirNodeKind::CoreMutexCreate;
            break;
        case ProjectOopHirNodeKind::CoreMutexLock:
            result->kind = ProjectOopMirNodeKind::CoreMutexLock;
            break;
        case ProjectOopHirNodeKind::CoreMutexUnlock:
            result->kind = ProjectOopMirNodeKind::CoreMutexUnlock;
            break;
        case ProjectOopHirNodeKind::CoreMutexClose:
            result->kind = ProjectOopMirNodeKind::CoreMutexClose;
            break;
        // Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll are trivial passthrough --
        // their single implicit field address is a real FieldAddress CHILD node (built in
        // ProjectOopHirModel.cpp), converted generically by the recursive children loop at the
        // bottom of this function, exactly like CoreInterlockedIncrementInt32 above.
        case ProjectOopHirNodeKind::CoreMonitorEnter:
            result->kind = ProjectOopMirNodeKind::CoreMonitorEnter;
            break;
        case ProjectOopHirNodeKind::CoreMonitorExit:
            result->kind = ProjectOopMirNodeKind::CoreMonitorExit;
            break;
        case ProjectOopHirNodeKind::CoreMonitorPulse:
            result->kind = ProjectOopMirNodeKind::CoreMonitorPulse;
            break;
        case ProjectOopHirNodeKind::CoreMonitorPulseAll:
            result->kind = ProjectOopMirNodeKind::CoreMonitorPulseAll;
            break;
        // CoreMonitorWait is the one case in this round that is NOT trivial passthrough: its
        // memberOrdinal/secondaryMemberOrdinal (copied generically above) hold RAW FIELD ORDINALS
        // (cv_, lock_), not yet resolved byte offsets -- resolve both here, exactly like the
        // ordinary FieldAddress case elsewhere in this function does for its single ordinal, and
        // store them as `immediate`/`secondaryImmediate` for ProjectOopLlvmLowerer.cpp to GEP
        // directly off the single lowered receiver child (see that file's CoreMonitorWait case).
        case ProjectOopHirNodeKind::CoreMonitorWait:
        {
            if (!node->memberOrdinal.has_value() || !node->secondaryMemberOrdinal.has_value())
            {
                error = "Stage 54.4 Monitor.Wait HIR node is missing one of its two field ordinals";
                return nullptr;
            }
            const ObjectFieldLayout* cvLayout = fieldLayout(*node->memberOrdinal);
            const ObjectFieldLayout* lockLayout = fieldLayout(*node->secondaryMemberOrdinal);
            if (cvLayout == nullptr || lockLayout == nullptr)
            {
                error = "Stage 54.4 cannot find Stage 29 field offset for Monitor.Wait's cv_/lock_ fields";
                return nullptr;
            }
            result->kind = ProjectOopMirNodeKind::CoreMonitorWait;
            result->immediate = cvLayout->offset;
            result->secondaryImmediate = lockLayout->offset;
            break;
        }
        // Stage 54.5 "Otras primitivas de sincronizacion". Todas trivial passthrough, mismo
        // shape que CoreMutexCreate/Lock/Unlock/Close arriba -- sin receptor, sin FieldAddress.
        case ProjectOopHirNodeKind::CoreSemaphoreCreate:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreCreate;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreWait:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreWait;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreRelease:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreRelease;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreClose:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreClose;
            break;
        case ProjectOopHirNodeKind::CoreEventCreate:
            result->kind = ProjectOopMirNodeKind::CoreEventCreate;
            break;
        case ProjectOopHirNodeKind::CoreEventSet:
            result->kind = ProjectOopMirNodeKind::CoreEventSet;
            break;
        case ProjectOopHirNodeKind::CoreEventReset:
            result->kind = ProjectOopMirNodeKind::CoreEventReset;
            break;
        case ProjectOopHirNodeKind::CoreEventWait:
            result->kind = ProjectOopMirNodeKind::CoreEventWait;
            break;
        case ProjectOopHirNodeKind::CoreEventClose:
            result->kind = ProjectOopMirNodeKind::CoreEventClose;
            break;
        // Stage 55.1/55.2/55.3 -- trivial passthrough, exactly like the Interlocked/Semaphore/
        // Event cases above: no memberOrdinal/immediate resolution needed, children (if any --
        // only CoreProcessSetPriorityClass/CoreDebugOutputString have one) are copied generically
        // by the loop that calls this function.
        case ProjectOopHirNodeKind::CoreProcessGetCurrentProcessId:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetWorkingSetSize:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetKernelTime:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetKernelTime;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetUserTime:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetUserTime;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetPriorityClass:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetPriorityClass;
            break;
        case ProjectOopHirNodeKind::CoreProcessSetPriorityClass:
            result->kind = ProjectOopMirNodeKind::CoreProcessSetPriorityClass;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetPageSize:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetPageSize;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetProcessorArchitecture:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetAllocationGranularity:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetMemoryLoadPercent:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent;
            break;
        case ProjectOopHirNodeKind::CoreDebugIsDebuggerPresent:
            result->kind = ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent;
            break;
        case ProjectOopHirNodeKind::CoreDebugCaptureStackFrameCount:
            result->kind = ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount;
            break;
        case ProjectOopHirNodeKind::CoreDebugOutputString:
            result->kind = ProjectOopMirNodeKind::CoreDebugOutputString;
            break;
        case ProjectOopHirNodeKind::CoreThreadSleep:
            result->kind = ProjectOopMirNodeKind::CoreThreadSleep;
            break;
        case ProjectOopHirNodeKind::CoreFileCopy:
            result->kind = ProjectOopMirNodeKind::CoreFileCopy;
            break;
        case ProjectOopHirNodeKind::CoreFileMove:
            result->kind = ProjectOopMirNodeKind::CoreFileMove;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryMove:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryMove;
            break;
        case ProjectOopHirNodeKind::LambdaCreate:
            result->kind = ProjectOopMirNodeKind::LambdaCreate;
            result->calleeName = node->stringValue;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::ClosureCaptureValue:
            result->kind = ProjectOopMirNodeKind::ClosureCaptureLoad;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::ClosureCaptureAddress:
            result->kind = ProjectOopMirNodeKind::ClosureCaptureAddress;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::CaptureCellReference:
            result->kind = ProjectOopMirNodeKind::CaptureCellReference;
            break;
        case ProjectOopHirNodeKind::ClosureCaptureCellReference:
            result->kind = ProjectOopMirNodeKind::ClosureCaptureCellReference;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::DelegateCreate:
        {
            if (!node->memberOrdinal.has_value() || hirModel_ == nullptr)
            {
                error = "Stage 42 delegate creation has no target callable";
                return nullptr;
            }
            const ProjectOopHirFunction* target = hirModel_->functionForMember(*node->memberOrdinal);
            if (target == nullptr)
            {
                error = "Stage 42 delegate cannot bind an abstract or body-less target";
                return nullptr;
            }
            result->kind = ProjectOopMirNodeKind::DelegateCreate;
            result->calleeName = target->mangledName;
            break;
        }
        case ProjectOopHirNodeKind::DelegateInvoke:
            result->kind = ProjectOopMirNodeKind::DelegateInvoke;
            break;
        case ProjectOopHirNodeKind::EventAdd:
            result->kind = ProjectOopMirNodeKind::EventAdd;
            break;
        case ProjectOopHirNodeKind::EventRemove:
            result->kind = ProjectOopMirNodeKind::EventRemove;
            break;
        case ProjectOopHirNodeKind::EventInvoke:
            result->kind = ProjectOopMirNodeKind::EventInvoke;
            break;
        case ProjectOopHirNodeKind::ArrayAllocation:
        case ProjectOopHirNodeKind::ArrayElementValue:
        case ProjectOopHirNodeKind::ArrayElementAddress:
        {
            if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr)
            {
                error = "Stage 41 array lowering requires name lookup and value layouts";
                return nullptr;
            }
            const auto layout = elementLayout(node->elementTypeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "Stage 41 cannot determine native array element layout for '" + node->elementTypeKey + "'";
                return nullptr;
            }
            result->immediate = layout->first;
            result->alignment = layout->second;
            result->kind = node->kind == ProjectOopHirNodeKind::ArrayAllocation
                ? ProjectOopMirNodeKind::ArrayAllocate
                : node->kind == ProjectOopHirNodeKind::ArrayElementValue
                    ? ProjectOopMirNodeKind::ArrayElementLoad
                    : ProjectOopMirNodeKind::ArrayElementAddress;
            break;
        }
        case ProjectOopHirNodeKind::Unary: result->kind = ProjectOopMirNodeKind::Unary; break;
        case ProjectOopHirNodeKind::Binary: result->kind = ProjectOopMirNodeKind::Binary; break;
        case ProjectOopHirNodeKind::Assignment: result->kind = ProjectOopMirNodeKind::Store; break;
        // IDs 79 y 81. Un destino numerico no necesita paso; un destino puntero si, y lo resuelve
        // el MISMO elementLayout que ya usan PointerArithmetic, el stride de un arreglo y sizeof.
        case ProjectOopHirNodeKind::CompoundStore:
        {
            result->kind = ProjectOopMirNodeKind::CompoundStore;
            if (!node->elementTypeKey.empty())
            {
                if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr)
                {
                    error = "compound pointer update requires name lookup and value layouts";
                    return nullptr;
                }
                const auto layout = elementLayout(node->elementTypeKey);
                if (!layout.has_value() || layout->first == 0)
                {
                    error = "compound pointer update cannot determine the element layout of '" +
                        node->elementTypeKey + "'";
                    return nullptr;
                }
                result->immediate = layout->first;
                result->alignment = layout->second;
            }
            break;
        }
        case ProjectOopHirNodeKind::Conversion: result->kind = ProjectOopMirNodeKind::Convert; break;
        case ProjectOopHirNodeKind::RuntimeTypeTest:
        case ProjectOopHirNodeKind::SafeReferenceCast:
        case ProjectOopHirNodeKind::CheckedReferenceCast:
        {
            if (!node->targetTypeOrdinal.has_value() || nameLookup_ == nullptr)
            {
                error = "Stage 39 RTTI HIR node has no target type";
                return nullptr;
            }
            result->kind = node->kind == ProjectOopHirNodeKind::RuntimeTypeTest
                ? ProjectOopMirNodeKind::RuntimeTypeTest
                : node->kind == ProjectOopHirNodeKind::SafeReferenceCast
                    ? ProjectOopMirNodeKind::SafeReferenceCast
                    : ProjectOopMirNodeKind::CheckedReferenceCast;
            result->targetIsInterface = node->targetIsInterface;
            result->immediate = stableTypeId(
                "type:" + nameLookup_->namedTypes()[*node->targetTypeOrdinal].identity.key());
            break;
        }
        case ProjectOopHirNodeKind::Deallocation: result->kind = ProjectOopMirNodeKind::Deallocate; break;

        case ProjectOopHirNodeKind::StaticFieldValue:
            result->kind = ProjectOopMirNodeKind::StaticFieldLoad;
            break;
        case ProjectOopHirNodeKind::StaticFieldAddress:
            result->kind = ProjectOopMirNodeKind::StaticFieldAddress;
            break;

        case ProjectOopHirNodeKind::FieldValue:
        case ProjectOopHirNodeKind::FieldAddress:
        {
            if (!node->memberOrdinal.has_value())
            {
                error = "Stage 34 field HIR node has no member ordinal";
                return nullptr;
            }
            const ObjectFieldLayout* layout = fieldLayout(*node->memberOrdinal);
            if (layout == nullptr)
            {
                error = "Stage 34 cannot find Stage 29 field offset for member #" +
                    std::to_string(*node->memberOrdinal);
                return nullptr;
            }
            result->kind = node->kind == ProjectOopHirNodeKind::FieldValue
                ? ProjectOopMirNodeKind::FieldLoad
                : ProjectOopMirNodeKind::FieldAddress;
            result->immediate = layout->offset;
            result->alignment = layout->alignment;
            break;
        }

        case ProjectOopHirNodeKind::InstanceCall:
        case ProjectOopHirNodeKind::ConstructorInitializerCall:
        {
            if (!node->memberOrdinal.has_value() || hirModel_ == nullptr)
            {
                error = "Macro A instance/base call has no selected target method";
                return nullptr;
            }
            if (node->interfaceDispatch)
            {
                if (!node->targetTypeOrdinal.has_value() || interfaceModel_ == nullptr)
                {
                    error = "Stage 36 interface call is missing its static interface type";
                    return nullptr;
                }
                const InterfaceTypeInfo* info = interfaceModel_->interfaceInfo(*node->targetTypeOrdinal);
                if (info == nullptr)
                {
                    error = "Stage 36 interface call target is not an interface";
                    return nullptr;
                }
                const auto found = std::find(
                    info->effectiveMethodMemberOrdinals.begin(),
                    info->effectiveMethodMemberOrdinals.end(),
                    *node->memberOrdinal);
                if (found == info->effectiveMethodMemberOrdinals.end())
                {
                    error = "Stage 36 interface call cannot find the selected contract slot";
                    return nullptr;
                }
                result->kind = ProjectOopMirNodeKind::InterfaceInstanceCall;
                result->targetTypeOrdinal = node->targetTypeOrdinal;
                result->interfaceSlot = static_cast<std::size_t>(
                    found - info->effectiveMethodMemberOrdinals.begin());
                result->immediate = stableTypeId(
                    interfaceModel_->typeKeyForOrdinal(*node->targetTypeOrdinal));
                break;
            }
            const std::optional<std::size_t> slot = virtualDispatchModel_ != nullptr
                ? virtualDispatchModel_->slotForMember(*node->memberOrdinal)
                : std::nullopt;
            const bool dynamicDispatch =
                node->kind == ProjectOopHirNodeKind::InstanceCall && !node->baseReceiver && slot.has_value();
            const ProjectOopHirFunction* target = hirModel_->functionForMember(*node->memberOrdinal);
            if (!dynamicDispatch && target == nullptr)
            {
                error = "Stage 35 direct/base call cannot target an abstract method";
                return nullptr;
            }
            result->kind = dynamicDispatch
                ? ProjectOopMirNodeKind::VirtualInstanceCall
                : ProjectOopMirNodeKind::DirectInstanceCall;
            if (target != nullptr)
            {
                result->calleeName = target->mangledName;
            }
            result->virtualSlot = dynamicDispatch ? slot : std::nullopt;
            break;
        }

        case ProjectOopHirNodeKind::StaticCall:
        {
            if (!node->memberOrdinal.has_value() || hirModel_ == nullptr)
            {
                error = "Stage 37 static call has no selected target method";
                return nullptr;
            }
            const ProjectOopHirFunction* target = hirModel_->functionForMember(*node->memberOrdinal);
            if (target == nullptr || !target->isStatic)
            {
                error = "Stage 37 static call target is missing or is not static";
                return nullptr;
            }
            result->kind = ProjectOopMirNodeKind::DirectStaticCall;
            result->calleeName = target->mangledName;
            break;
        }

        case ProjectOopHirNodeKind::TemporaryAddress:
            result->kind = ProjectOopMirNodeKind::TemporaryAddress;
            break;
        case ProjectOopHirNodeKind::AggregateInitializer:
        {
            // C7 "aggregate initializer": the storage the elements are written into. Its size and
            // alignment come from the same value layout every other consumer of this type reads,
            // so `V v = {1, 2};` occupies exactly the bytes a C host sees.
            if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr)
            {
                error = "C7 aggregate initializer requires name lookup and value layouts";
                return nullptr;
            }
            const auto layout = elementLayout(node->typeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "C7 cannot determine the layout of aggregate '" + node->typeKey + "'";
                return nullptr;
            }
            result->kind = ProjectOopMirNodeKind::AggregateInitialize;
            result->immediate = layout->first;
            result->alignment = layout->second;
            result->zeroInitialize = true;
            break;
        }
        case ProjectOopHirNodeKind::AggregateElement:
        {
            result->kind = ProjectOopMirNodeKind::AggregateElementStore;
            result->immediate = node->integerValue;
            // C8: the element's WIDTH IN BYTES, resolved here rather than in the backend, because
            // this is where the value layouts live. It is what lets the struct root members read
            // every leaf -- a float, a reference, an enum -- as a plain integer of the right size,
            // with no cast instructions at all. (C7's initializer elements carry it too and simply
            // do not use it: one rule for what an aggregate element is, not two.)
            if (nameLookup_ != nullptr && objectLayoutModel_ != nullptr)
            {
                const auto leafLayout = elementLayout(node->typeKey);
                if (leafLayout.has_value()) result->secondaryImmediate = leafLayout->first;
            }
            break;
        }
        case ProjectOopHirNodeKind::ValueConstruction:
        {
            // C6 "struct members": a struct has no allocation plan -- there is nothing to
            // allocate. Its size and alignment come straight from the value layout the object
            // layout model already computed for it, which is the same layout it crosses the C
            // boundary with.
            if (!node->targetTypeOrdinal.has_value() || objectLayoutModel_ == nullptr)
            {
                error = "C6 struct construction has no target type ordinal";
                return nullptr;
            }
            const StructValueLayout* layout =
                objectLayoutModel_->structLayoutForType(*node->targetTypeOrdinal);
            if (layout == nullptr)
            {
                error = "C6 struct construction has no value layout";
                return nullptr;
            }
            result->kind = ProjectOopMirNodeKind::ValueConstruct;
            result->immediate = layout->size;
            result->alignment = layout->alignment;
            result->zeroInitialize = true;
            if (node->memberOrdinal.has_value())
            {
                const ProjectOopHirFunction* constructor =
                    hirModel_->functionForMember(*node->memberOrdinal);
                if (constructor == nullptr)
                {
                    error = "C6 struct constructor has no OOP HIR function";
                    return nullptr;
                }
                result->calleeName = constructor->mangledName;
                result->memberOrdinal = node->memberOrdinal;
            }
            break;
        }
        case ProjectOopHirNodeKind::ObjectConstruction:
        {
            if (!node->constructionOrdinal.has_value())
            {
                error = "Stage 34 object construction has no construction ordinal";
                return nullptr;
            }
            const ObjectAllocationPlan* plan = allocationPlan(*node->constructionOrdinal);
            if (plan == nullptr)
            {
                error = "Stage 34 cannot find Stage 29 allocation plan";
                return nullptr;
            }
            result->kind = ProjectOopMirNodeKind::AllocateAndConstruct;
            result->immediate = plan->objectSize;
            result->alignment = plan->objectAlignment;
            result->allocationFailureReturnsNull = plan->allocationFailureReturnsNull;
            result->zeroInitialize = plan->zeroInitializesStorage;
            if (plan->constructorEntryMemberOrdinal.has_value())
            {
                const ProjectOopHirFunction* constructor = hirModel_->functionForMember(
                    *plan->constructorEntryMemberOrdinal);
                if (constructor == nullptr)
                {
                    error = "Macro A constructor entry has no OOP HIR function";
                    return nullptr;
                }
                result->calleeName = constructor->mangledName;
                result->memberOrdinal = plan->constructorEntryMemberOrdinal;
            }
            break;
        }
    }

    for (const auto& child : node->children)
    {
        if (child == nullptr)
        {
            result->children.push_back(nullptr);
            continue;
        }
        auto lowered = lowerNode(child.get(), error);
        if (lowered == nullptr)
        {
            return nullptr;
        }
        result->children.push_back(std::move(lowered));
    }
    return result;
}

const ObjectFieldLayout* ProjectOopMirModel::fieldLayout(std::size_t memberOrdinal) const noexcept
{
    return objectLayoutModel_ != nullptr
        ? objectLayoutModel_->fieldLayoutForMember(memberOrdinal)
        : nullptr;
}

const ObjectAllocationPlan* ProjectOopMirModel::allocationPlan(std::size_t constructionOrdinal) const noexcept
{
    if (objectLayoutModel_ == nullptr)
    {
        return nullptr;
    }
    // The plan vector is NOT indexed by construction ordinal, and treating it as if it were was
    // the bug. buildAllocationPlans() walks every construction but APPENDS a plan only for the
    // ones that actually allocate: it skips an unresolved construction, and -- since C6 -- every
    // STRUCT construction, which has no class layout and needs no plan at all because there is
    // nothing to allocate.
    //
    // So `allocationPlans()[ordinal]` was only correct while every construction produced a plan.
    // One skipped construction shifted every later plan down by one, and from there the lookup
    // either ran off the end -- the reported "Stage 34 cannot find Stage 29 allocation plan", an
    // internal error with no file and no line, which cost nine bisection steps to isolate when it
    // showed up in a 300-line file -- or, in the cases that stayed in range, silently returned
    // SOMEBODY ELSE'S plan and would have allocated a class with the wrong size and alignment.
    // The loud half is what got reported; the silent half is the reason this is worth fixing at
    // the root instead of widening the bounds check.
    //
    // Each plan already records the ordinal it belongs to, so match on it.
    for (const ObjectAllocationPlan& plan : objectLayoutModel_->allocationPlans())
    {
        if (plan.constructionOrdinal == constructionOrdinal)
        {
            return &plan;
        }
    }
    return nullptr;
}

// C2.1 "fixed arrays": the size and alignment of one element, for every element type this
// compiler can put in an array -- managed (heap, length-prefixed) or raw (inline storage, a raw
// pointer). Extracted from an inline lambda in the managed-array case so the raw case asks the
// SAME question the same way; two answers that could disagree about a struct's size would produce
// a GEP stride that does not match the layout the field was given.
std::optional<std::pair<std::uint64_t, std::uint64_t>> ProjectOopMirModel::elementLayout(
    std::string_view typeKey) const
{
    if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr) return std::nullopt;
    // One rule for "how big is one of these", shared with the OOP HIR model's aggregate-element
    // offsets (C7). See ProjectObjectLayoutModel::valueLayoutForTypeKey.
    return objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, typeKey);
}

}
