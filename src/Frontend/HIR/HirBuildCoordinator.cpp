#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectCompilationIndexes.h"
#include "HirLoweringSupport.h"
#include "Internal/RegularFunctionResult.h"
#include "Internal/StackClassLocalUseFrame.h"

#include "nativesharp/Sema/EffectContext.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"

#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
template <typename Index, typename Values>
void indexValuesBySyntax(Index& index, const Values& values)
{
    index.reserve(values.size());
    for (const auto& value : values) index[value.syntax] = &value;
}

std::string hirSpecializationKey(const ast::AstNode* syntax)
{
    constexpr std::string_view kSpecializationPrefix = "stage44-specialization:";
    if (syntax == nullptr || !syntax->auxiliaryText().starts_with(kSpecializationPrefix))
        return {};
    std::string key = syntax->auxiliaryText().substr(kSpecializationPrefix.size());
    if (const std::size_t metadata = key.find(";nsc-meta:"); metadata != std::string::npos)
        key.resize(metadata);
    return key;
}

void appendUniqueOrdinal(std::vector<std::size_t>& values, std::size_t value)
{
    if (std::ranges::find(values, value) == values.end()) values.push_back(value);
}

// LA UNICA TABLA de las operaciones nucleo que se bajan por el bloque compartido de "llamada
// tipo string" de lowerExpression. Contesta LAS TRES preguntas que hay que hacer sobre una de
// ellas: si entra en el bloque, cuantos operandos toma, y en que nodo HIR se convierte.
//
// POR QUE EXISTE, y es la correccion del 2026-09-02 (arity) ampliada el 2026-09-03 (nodo HIR).
// Antes esto eran TRES cosas separadas dentro de la misma funcion, a cientos de lineas unas de
// otras:
//
//   1) una cadena de `core->kind == ... ||` que decidia si la operacion entraba en el bloque;
//   2) una cadena de if/else-if que le asignaba la aridad, con un `expected = 2U` por omision;
//   3) una cadena de if/else-if que le asignaba el nodo HIR, con `StringConcat` por omision.
//
// Anadir una operacion y olvidar cualquiera de las tres NO daba ningun error, y cada olvido
// fallaba de una forma distinta -- ninguna decia el nombre de la operacion:
//
//   * solo en (1): la aridad caia al 2 por omision. Si la operacion no tomaba dos operandos, el
//     resultado era "Stage 51 string intrinsic received an invalid operand count" -- un mensaje
//     que no nombra ni la operacion ni la lista.
//   * solo en (2): la operacion nunca entraba al bloque, asi que la llamada se quedaba siendo el
//     metodo NativeSharp normal -- el que devuelve -1 o "" como marcador de posicion. Compila,
//     enlaza, ejecuta, y da una respuesta plausible y FALSA. Medido al implementar File.GetSize:
//     el gancho aparecia declarado en el IR y sin una sola llamada.
//   * olvidar (3) era el peor de los tres: la operacion no se quedaba sin nodo, se convertia en
//     `StringConcat`. Es decir, `File.GetSize(ruta)` se habria bajado como una CONCATENACION DE
//     CADENAS. Ningun mensaje, ningun aviso; codigo generado que hace otra cosa.
//
// Con una sola funcion las tres respuestas salen del mismo `case`, asi que no pueden discrepar.
//
// Y EL SWITCH NO TIENE `default` A PROPOSITO. Con -Wswitch (y /WX, que este proyecto usa),
// anadir un enumerador a MethodCoreOperationKind y no tocar esta funcion es un ERROR DE
// COMPILACION que nombra el enumerador que falta. Ese es todo el punto: la comprobacion la hace
// el compilador y no la memoria de quien anade la operacion numero 105. Si la operacion nueva no
// va por este bloque, se pone en el grupo de `std::nullopt` y ya esta -- pero hay que decirlo.
//
// COMPROBADO ANTES DE FUSIONAR, para no fusionar dos cosas que ya discreparan: las 50 operaciones
// autorizadas por (1) y las 49 mapeadas explicitamente en (3) --mas `StringConcat`, que era el
// valor por omision-- eran exactamente el mismo conjunto. Cero codigo muerto, cero operacion sin
// mapeo. La tabla de abajo se derivo de las dos cadenas con un script, no a mano.
//
// LOS COMENTARIOS POR OPERACION son los de las cadenas viejas, movidos aqui tal cual: son la
// historia de por que cada operacion cae donde cae, y se pierden si se quedan en un fichero que ya
// no existe.
std::string sanitize(std::string value)
{
    for (char& ch : value)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) == 0 && ch != '_')
        {
            ch = '_';
        }
    }
    return value;
}

std::string stableSymbolSuffix(std::string_view value)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char raw : value)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(raw));
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << hash;
    return stream.str();
}

// C1.2 "import identity": the INTERNAL name of an import whose C symbol name is shared by two
// libraries. It encodes the identity's library and symbol so the two never collide again, and it
// is never seen by the linker as an external symbol -- the backend gives this name a real body
// that binds the external symbol at first call. The `__nsc_` prefix is already reserved (see
// isReservedStage47CAbiExportName), so no user declaration can land on it.
std::string importIdentityInternalName(std::string_view dllName, std::string_view symbolName)
{
    return "__nsc_import_" + sanitize(std::string(dllName)) + "__" + sanitize(std::string(symbolName));
}

std::string typeSuffix(const ClassMemberSymbol& member)
{
    std::string result;
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        // C4 "language references": `F(int* p)` and `F(ref int p)` are different members of the
        // language that lower to the same machine signature, so the mangled name has to keep them
        // apart or the two would become one LLVM function.
        if (parameter.passKind != ParameterPassKind::Value)
        {
            result += "__";
            result += parameterPassKindKeyword(parameter.passKind);
        }
        // NSC-LIMIT-009: mismo motivo que en mangleCallable. Un parametro de tipo anidado
        // colapsaba con uno del tipo homonimo de namespace, y dos sobrecargas distintas se
        // convertian en una sola funcion LLVM.
        result += "__" + compilation::symbolFragmentForIdentityKey(parameter.typeKey);
    }
    // Conversion overload identity includes the target type. Without this suffix,
    // `implicit operator A(Source)` and `implicit operator B(Source)` become the same
    // LLVM/COFF symbol even though the language resolves them by the requested target.
    if (member.isUserConversion)
    {
        result += "__to__" + compilation::symbolFragmentForIdentityKey(member.declaredTypeKey);
    }
    return result;
}
}

// Item 323 deliberately proves only uses that cannot expose the address of the containing class.
// Reading a field copies its value. A FieldAddress is equally safe only while it is the direct
// destination of a store; every other appearance of the local is conservatively an escape.
bool ProjectOopHirModel::stackClassLocalUsesAreSafe(
    const ProjectOopHirNode* node,
    const ProjectOopHirNode* declaration,
    std::size_t localOrdinal,
    const ProjectOopHirNode* parent,
    const ProjectOopHirNode* grandparent,
    std::size_t childIndex,
    std::size_t parentChildIndex)
{
    using internal::StackClassLocalUseFrame;
    std::vector<StackClassLocalUseFrame> stack;
    stack.push_back(StackClassLocalUseFrame{
        node, parent, grandparent, childIndex, parentChildIndex});
    while (!stack.empty())
    {
        const StackClassLocalUseFrame frame = stack.back();
        stack.pop_back();
        if (frame.node == nullptr) continue;
        if (frame.node != declaration && frame.node->localOrdinal == localOrdinal)
        {
            const bool copiedField = frame.node->kind == ProjectOopHirNodeKind::LocalValue &&
                frame.parent != nullptr &&
                frame.parent->kind == ProjectOopHirNodeKind::FieldValue &&
                frame.childIndex == 0;
            const bool directFieldStore =
                frame.node->kind == ProjectOopHirNodeKind::LocalValue &&
                frame.parent != nullptr &&
                frame.parent->kind == ProjectOopHirNodeKind::FieldAddress &&
                frame.childIndex == 0 && frame.grandparent != nullptr &&
                frame.parentChildIndex == 0 &&
                (frame.grandparent->kind == ProjectOopHirNodeKind::Assignment ||
                 frame.grandparent->kind == ProjectOopHirNodeKind::CompoundStore);
            if (!copiedField && !directFieldStore) return false;
        }
        for (std::size_t index = frame.node->children.size(); index > 0; --index)
        {
            stack.push_back(StackClassLocalUseFrame{
                frame.node->children[index - 1U].get(),
                frame.node,
                frame.parent,
                index - 1U,
                frame.childIndex});
        }
    }
    return true;
}

void ProjectOopHirModel::collectLocalDeclarations(
    ProjectOopHirNode* node,
    std::vector<ProjectOopHirNode*>& declarations)
{
    std::vector<ProjectOopHirNode*> stack;
    stack.push_back(node);
    while (!stack.empty())
    {
        ProjectOopHirNode* current = stack.back();
        stack.pop_back();
        if (current == nullptr) continue;
        if (current->kind == ProjectOopHirNodeKind::LocalDeclaration)
            declarations.push_back(current);
        for (auto child = current->children.rbegin(); child != current->children.rend(); ++child)
            stack.push_back(child->get());
    }
}

const ProjectOopHirNode* ProjectOopHirModel::firstUnpromotedObjectConstruction(
    const ProjectOopHirNode* node)
{
    std::vector<const ProjectOopHirNode*> stack;
    stack.push_back(node);
    while (!stack.empty())
    {
        const ProjectOopHirNode* current = stack.back();
        stack.pop_back();
        if (current == nullptr) continue;
        if (current->kind == ProjectOopHirNodeKind::ObjectConstruction &&
            !current->stackPromotedObject)
        {
            return current;
        }
        for (auto child = current->children.rbegin(); child != current->children.rend(); ++child)
            stack.push_back(child->get());
    }
    return nullptr;
}

void ProjectOopHirModel::scanParallelOrdinals(
    const ProjectOopHirNode* node,
    std::size_t localFloor,
    std::size_t cleanupFloor,
    std::size_t& localCount,
    std::size_t& cleanupCount,
    std::size_t& regionCount)
{
    std::vector<const ProjectOopHirNode*> stack;
    stack.push_back(node);
    while (!stack.empty())
    {
        const ProjectOopHirNode* current = stack.back();
        stack.pop_back();
        if (current == nullptr) continue;
        if (current->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow)
        {
            if (current->localOrdinal.has_value() && *current->localOrdinal >= localFloor)
                localCount = std::max(
                    localCount, *current->localOrdinal - localFloor + 1U);
            if (current->cleanupRegionOrdinal.has_value() &&
                *current->cleanupRegionOrdinal >= cleanupFloor)
            {
                cleanupCount = std::max(
                    cleanupCount, *current->cleanupRegionOrdinal - cleanupFloor + 1U);
            }
        }
        if (current->hirRegionOrdinal.has_value())
            regionCount = std::max(regionCount, *current->hirRegionOrdinal + 1U);
        for (auto child = current->children.rbegin(); child != current->children.rend(); ++child)
            stack.push_back(child->get());
    }
}

void ProjectOopHirModel::applyParallelOrdinalOffsets(
    ProjectOopHirNode* node,
    std::size_t localFloor,
    std::size_t cleanupFloor,
    std::size_t localOffset,
    std::size_t cleanupOffset,
    std::size_t regionOffset)
{
    std::vector<ProjectOopHirNode*> stack;
    stack.push_back(node);
    while (!stack.empty())
    {
        ProjectOopHirNode* current = stack.back();
        stack.pop_back();
        if (current == nullptr) continue;
        if (current->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow)
        {
            if (current->localOrdinal.has_value() && *current->localOrdinal >= localFloor)
                *current->localOrdinal += localOffset;
            if (current->cleanupRegionOrdinal.has_value() &&
                *current->cleanupRegionOrdinal >= cleanupFloor)
            {
                *current->cleanupRegionOrdinal += cleanupOffset;
            }
        }
        if (current->hirRegionOrdinal.has_value())
            *current->hirRegionOrdinal += regionOffset;
        if (current->exceptionTargetRegionOrdinal.has_value())
            *current->exceptionTargetRegionOrdinal += regionOffset;
        for (auto child = current->children.rbegin(); child != current->children.rend(); ++child)
            stack.push_back(child->get());
    }
}
#define NSC_HIR_NODE_KIND_NAMES_01 \
    case ProjectOopHirNodeKind::Block: return "block"; \
    case ProjectOopHirNodeKind::BlockExpression: return "block-expression"; \
    case ProjectOopHirNodeKind::LocalDeclaration: return "local-declaration"; \
    case ProjectOopHirNodeKind::ExpressionStatement: return "expression-statement"; \
    case ProjectOopHirNodeKind::InlineAssembly: return "inline-assembly"; \
    case ProjectOopHirNodeKind::InlineAssemblyLine: return "inline-assembly-line"; \
    case ProjectOopHirNodeKind::InlineAssemblyInput: return "inline-assembly-input"; \
    case ProjectOopHirNodeKind::InlineAssemblyOutput: return "inline-assembly-output"; \
    case ProjectOopHirNodeKind::InlineAssemblyClobber: return "inline-assembly-clobber"; \
    case ProjectOopHirNodeKind::If: return "if"; \
    case ProjectOopHirNodeKind::While: return "while"; \
    case ProjectOopHirNodeKind::DoWhile: return "do-while"; \
    case ProjectOopHirNodeKind::For: return "for"; \
    case ProjectOopHirNodeKind::Switch: return "switch"; \
    case ProjectOopHirNodeKind::SwitchSection: return "switch-section"; \
    case ProjectOopHirNodeKind::Break: return "break"; \
    case ProjectOopHirNodeKind::Continue: return "continue"; \
    case ProjectOopHirNodeKind::Return: return "return"; \
    case ProjectOopHirNodeKind::TryRegion: return "try-region"; \
    case ProjectOopHirNodeKind::CatchRegion: return "catch-region"; \
    case ProjectOopHirNodeKind::FinallyRegion: return "finally-region"; \
    case ProjectOopHirNodeKind::UsingRegion: return "using-region"; \
    case ProjectOopHirNodeKind::Throw: return "throw"; \
    case ProjectOopHirNodeKind::Rethrow: return "rethrow"; \
    case ProjectOopHirNodeKind::CleanupContinuation: return "cleanup-continuation"; \
    case ProjectOopHirNodeKind::OwnedParameterCleanup: return "owned-parameter-cleanup"; \
    case ProjectOopHirNodeKind::MoveValue: return "move-value"; \
    case ProjectOopHirNodeKind::MoveArrayElement: return "move-array-element"; \
    case ProjectOopHirNodeKind::MoveOwnedField: return "move-owned-field"; \
    case ProjectOopHirNodeKind::MoveIteratorCurrent: return "move-iterator-current"; \
    case ProjectOopHirNodeKind::OwnedTemporaryBorrow: return "owned-temporary-borrow"; \
    case ProjectOopHirNodeKind::FullExpressionCleanup: return "full-expression-cleanup"; \
    case ProjectOopHirNodeKind::PointerLoad: return "pointer-load"; \
    case ProjectOopHirNodeKind::RawElementAddress: return "raw-element-address"; \
    case ProjectOopHirNodeKind::StackAllocation: return "stack-allocation"; \
    case ProjectOopHirNodeKind::RawAllocation: return "raw-allocation"; \
    case ProjectOopHirNodeKind::PointerArithmetic: return "pointer-arithmetic"; \
    case ProjectOopHirNodeKind::ValueConstruction: return "value-construction"; \
    case ProjectOopHirNodeKind::PlacementConstruction: return "placement-construction"; \
    case ProjectOopHirNodeKind::AggregateInitializer: return "aggregate-initializer";

#define NSC_HIR_NODE_KIND_NAMES_02 \
    case ProjectOopHirNodeKind::AggregateSpread: return "aggregate-spread"; \
    case ProjectOopHirNodeKind::AggregateElement: return "aggregate-element"; \
    case ProjectOopHirNodeKind::TupleDeconstruct: return "tuple-deconstruct"; \
    case ProjectOopHirNodeKind::TupleDeconstructElement: return "tuple-deconstruct-element"; \
    case ProjectOopHirNodeKind::ParameterPack: return "parameter-pack"; \
    case ProjectOopHirNodeKind::TemporaryAddress: return "temporary-address"; \
    case ProjectOopHirNodeKind::AddressValue: return "address-value"; \
    case ProjectOopHirNodeKind::BorrowValue: return "borrow-value"; \
    case ProjectOopHirNodeKind::ThisValue: return "this"; \
    case ProjectOopHirNodeKind::ParameterValue: return "parameter-value"; \
    case ProjectOopHirNodeKind::LocalValue: return "local-value"; \
    case ProjectOopHirNodeKind::FieldValue: return "field-value"; \
    case ProjectOopHirNodeKind::NullConditionalFieldValue: return "null-conditional-field-value"; \
    case ProjectOopHirNodeKind::NullConditionalFieldStore: return "null-conditional-field-store"; \
    case ProjectOopHirNodeKind::NullConditionalArrayLoad: return "null-conditional-array-load"; \
    case ProjectOopHirNodeKind::NullConditionalArrayStore: return "null-conditional-array-store"; \
    case ProjectOopHirNodeKind::StaticFieldValue: return "static-field-value"; \
    case ProjectOopHirNodeKind::NamespaceStateValue: return "namespace-state-value"; \
    case ProjectOopHirNodeKind::ParameterAddress: return "parameter-address"; \
    case ProjectOopHirNodeKind::LocalAddress: return "local-address"; \
    case ProjectOopHirNodeKind::FieldAddress: return "field-address"; \
    case ProjectOopHirNodeKind::FutureFieldAddress: return "future-field-address"; \
    case ProjectOopHirNodeKind::AwaitFuture: return "await-future"; \
    case ProjectOopHirNodeKind::SuspendPoint: return "suspend-point"; \
    case ProjectOopHirNodeKind::PollAddress: return "poll-address"; \
    case ProjectOopHirNodeKind::YieldValue: return "yield-value"; \
    case ProjectOopHirNodeKind::YieldBreak: return "yield-break"; \
    case ProjectOopHirNodeKind::IteratorCurrent: return "iterator-current"; \
    case ProjectOopHirNodeKind::IteratorMoveNext: return "iterator-move-next"; \
    case ProjectOopHirNodeKind::IteratorGetEnumerator: return "iterator-get-enumerator"; \
    case ProjectOopHirNodeKind::IteratorDrop: return "iterator-drop"; \
    case ProjectOopHirNodeKind::IteratorMoveNextAsync: return "iterator-move-next-async"; \
    case ProjectOopHirNodeKind::IteratorCancel: return "iterator-cancel"; \
    case ProjectOopHirNodeKind::AsyncYieldValue: return "async-yield-value"; \
    case ProjectOopHirNodeKind::DropFrame: return "drop-frame"; \
    case ProjectOopHirNodeKind::StaticFieldAddress: return "static-field-address"; \
    case ProjectOopHirNodeKind::NamespaceStateAddress: return "namespace-state-address"; \
    case ProjectOopHirNodeKind::IntegerConstant: return "integer-constant"; \
    case ProjectOopHirNodeKind::FloatingConstant: return "floating-constant"; \
    case ProjectOopHirNodeKind::BooleanConstant: return "boolean-constant";

#define NSC_HIR_NODE_KIND_NAMES_03 \
    case ProjectOopHirNodeKind::NullConstant: return "null-constant"; \
    case ProjectOopHirNodeKind::DefaultValue: return "default-value"; \
    case ProjectOopHirNodeKind::StringLiteral: return "string-literal"; \
    case ProjectOopHirNodeKind::Utf8StringLiteral: return "utf8-string-literal"; \
    case ProjectOopHirNodeKind::ArrayAllocation: return "array-allocation"; \
    case ProjectOopHirNodeKind::RangeArrayAllocation: return "range-array-allocation"; \
    case ProjectOopHirNodeKind::ArrayLength: return "array-length"; \
    case ProjectOopHirNodeKind::ArrayElementValue: return "array-element-value"; \
    case ProjectOopHirNodeKind::ArrayElementAddress: return "array-element-address"; \
    case ProjectOopHirNodeKind::RangeArrayElementValue: return "range-array-element-value"; \
    case ProjectOopHirNodeKind::RangeArrayElementAddress: return "range-array-element-address"; \
    case ProjectOopHirNodeKind::RangeSlice: return "range-slice"; \
    case ProjectOopHirNodeKind::StringLength: return "string-length"; \
    case ProjectOopHirNodeKind::StringElementValue: return "string-element-value"; \
    case ProjectOopHirNodeKind::EnumName: return "enum-name"; \
    case ProjectOopHirNodeKind::StringConcat: return "string-concat"; \
    case ProjectOopHirNodeKind::StringEquals: return "string-equals"; \
    case ProjectOopHirNodeKind::StringSubstring: return "string-substring"; \
    case ProjectOopHirNodeKind::StringCharacterPointer: return "string-character-pointer"; \
    case ProjectOopHirNodeKind::StringFromCharArray: return "string-from-char-array"; \
    case ProjectOopHirNodeKind::StringViewFromString: return "string-view-from-string"; \
    case ProjectOopHirNodeKind::SpanFromArray: return "span-from-array"; \
    case ProjectOopHirNodeKind::ReadOnlySpanFromArray: return "readonly-span-from-array"; \
    case ProjectOopHirNodeKind::SpanFromFixedArray: return "span-from-fixed-array"; \
    case ProjectOopHirNodeKind::ReadOnlySpanFromFixedArray: return "readonly-span-from-fixed-array"; \
    case ProjectOopHirNodeKind::CoreConsoleWriteLine: return "core-console-writeline"; \
    case ProjectOopHirNodeKind::CoreConsoleReadLine: return "core-console-readline"; \
    case ProjectOopHirNodeKind::AggregateEquals: return "aggregate-equals"; \
    case ProjectOopHirNodeKind::AggregateHash: return "aggregate-hash"; \
    case ProjectOopHirNodeKind::CoreObjectTypeName: return "core-object-type-name"; \
    case ProjectOopHirNodeKind::CoreObjectReferenceHash: return "core-object-reference-hash"; \
    case ProjectOopHirNodeKind::CoreClockQueryCounter: return "core-clock-query-counter"; \
    case ProjectOopHirNodeKind::CoreClockQueryFrequency: return "core-clock-query-frequency"; \
    case ProjectOopHirNodeKind::CoreClockQuerySystemFileTime: return "core-clock-query-system-filetime"; \
    case ProjectOopHirNodeKind::CoreEnvironmentTickCount64: return "core-environment-tick-count64"; \
    case ProjectOopHirNodeKind::CoreEnvironmentProcessorCount: return "core-environment-processor-count"; \
    case ProjectOopHirNodeKind::CoreEnvironmentExit: return "core-environment-exit"; \
    case ProjectOopHirNodeKind::CoreEnvironmentGetVariable: return "core-environment-get-variable"; \
    case ProjectOopHirNodeKind::CoreEnvironmentSetVariable: return "core-environment-set-variable"; \
    case ProjectOopHirNodeKind::CoreFileExists: return "core-file-exists";

#define NSC_HIR_NODE_KIND_NAMES_04 \
    case ProjectOopHirNodeKind::CoreFileDelete: return "core-file-delete"; \
    case ProjectOopHirNodeKind::CoreDirectoryExists: return "core-directory-exists"; \
    case ProjectOopHirNodeKind::CoreDirectoryCreate: return "core-directory-create"; \
    case ProjectOopHirNodeKind::CoreFileReadAllText: return "core-file-read-all-text"; \
    case ProjectOopHirNodeKind::CoreFileGetSize: return "core-file-get-size"; \
    case ProjectOopHirNodeKind::CoreFileReadAllBytes: return "core-file-read-all-bytes"; \
    case ProjectOopHirNodeKind::CoreFileReadBytes: return "core-file-read-bytes"; \
    case ProjectOopHirNodeKind::CoreFileWriteAllBytes: return "core-file-write-all-bytes"; \
    case ProjectOopHirNodeKind::CoreEnvironmentCurrentDirectory: return "core-environment-current-directory"; \
    case ProjectOopHirNodeKind::CoreProcessRun: return "core-process-run"; \
    case ProjectOopHirNodeKind::CoreFileWriteAllText: return "core-file-write-all-text"; \
    case ProjectOopHirNodeKind::CoreFileAppendAllText: return "core-file-append-all-text"; \
    case ProjectOopHirNodeKind::CoreDirectoryDelete: return "core-directory-delete"; \
    case ProjectOopHirNodeKind::CorePathGetFullPath: return "core-path-get-full-path"; \
    case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgCount: \
        return "core-environment-get-command-line-arg-count"; \
    case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgs: return "core-environment-get-command-line-args"; \
    case ProjectOopHirNodeKind::CoreDirectoryGetFileCount: return "core-directory-get-file-count"; \
    case ProjectOopHirNodeKind::CoreDirectoryGetFiles: return "core-directory-get-files"; \
    case ProjectOopHirNodeKind::CoreDirectoryGetDirectoryCount: return "core-directory-get-directory-count"; \
    case ProjectOopHirNodeKind::CoreDirectoryGetDirectories: return "core-directory-get-directories"; \
    case ProjectOopHirNodeKind::CoreThreadCreate: return "core-thread-create"; \
    case ProjectOopHirNodeKind::CoreThreadJoin: return "core-thread-join"; \
    case ProjectOopHirNodeKind::CoreThreadDetach: return "core-thread-detach"; \
    case ProjectOopHirNodeKind::CoreAtomicLoadInt32: return "core-atomic-load-int32"; \
    case ProjectOopHirNodeKind::CoreAtomicStoreInt32: return "core-atomic-store-int32"; \
    case ProjectOopHirNodeKind::CoreInterlockedIncrementInt32: return "core-interlocked-increment-int32"; \
    case ProjectOopHirNodeKind::CoreInterlockedDecrementInt32: return "core-interlocked-decrement-int32"; \
    case ProjectOopHirNodeKind::CoreInterlockedAddInt32: return "core-interlocked-add-int32"; \
    case ProjectOopHirNodeKind::CoreInterlockedExchangeInt32: return "core-interlocked-exchange-int32"; \
    case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt32: \
        return "core-interlocked-compare-exchange-int32"; \
    case ProjectOopHirNodeKind::CoreAtomicLoadInt64: return "core-atomic-load-int64"; \
    case ProjectOopHirNodeKind::CoreAtomicStoreInt64: return "core-atomic-store-int64"; \
    case ProjectOopHirNodeKind::CoreInterlockedIncrementInt64: return "core-interlocked-increment-int64"; \
    case ProjectOopHirNodeKind::CoreInterlockedDecrementInt64: return "core-interlocked-decrement-int64"; \
    case ProjectOopHirNodeKind::CoreInterlockedAddInt64: return "core-interlocked-add-int64"; \
    case ProjectOopHirNodeKind::CoreInterlockedExchangeInt64: return "core-interlocked-exchange-int64"; \
    case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt64: \
        return "core-interlocked-compare-exchange-int64"; \
    case ProjectOopHirNodeKind::CoreAtomicLoadPointer: return "core-atomic-load-pointer"; \
    case ProjectOopHirNodeKind::CoreAtomicStorePointer: return "core-atomic-store-pointer"; \
    case ProjectOopHirNodeKind::CoreInterlockedExchangePointer: return "core-interlocked-exchange-pointer";

#define NSC_HIR_NODE_KIND_NAMES_05 \
    case ProjectOopHirNodeKind::CoreInterlockedCompareExchangePointer: \
        return "core-interlocked-compare-exchange-pointer"; \
    case ProjectOopHirNodeKind::CoreAtomicThreadFence: return "core-atomic-thread-fence"; \
    case ProjectOopHirNodeKind::CoreAtomicCompilerFence: return "core-atomic-compiler-fence"; \
    case ProjectOopHirNodeKind::CoreSimdFloat32x4Add: return "core-simd-float32x4-add"; \
    case ProjectOopHirNodeKind::CoreSimdFloat32x4Subtract: return "core-simd-float32x4-subtract"; \
    case ProjectOopHirNodeKind::CoreSimdFloat32x4Multiply: return "core-simd-float32x4-multiply"; \
    case ProjectOopHirNodeKind::CoreSimdInt128Add: return "core-simd-int128-add"; \
    case ProjectOopHirNodeKind::CoreSimdInt128Subtract: return "core-simd-int128-subtract"; \
    case ProjectOopHirNodeKind::CoreSimdInt128Multiply: return "core-simd-int128-multiply"; \
    case ProjectOopHirNodeKind::CoreSimdFloat256Add: return "core-simd-float256-add"; \
    case ProjectOopHirNodeKind::CoreSimdFloat256Subtract: return "core-simd-float256-subtract"; \
    case ProjectOopHirNodeKind::CoreSimdFloat256Multiply: return "core-simd-float256-multiply"; \
    case ProjectOopHirNodeKind::CoreSimdInt256Add: return "core-simd-int256-add"; \
    case ProjectOopHirNodeKind::CoreSimdInt256Subtract: return "core-simd-int256-subtract"; \
    case ProjectOopHirNodeKind::CoreSimdInt256Multiply: return "core-simd-int256-multiply"; \
    case ProjectOopHirNodeKind::CoreSimdLoadAligned128: return "core-simd-load-aligned-128"; \
    case ProjectOopHirNodeKind::CoreSimdStoreAligned128: return "core-simd-store-aligned-128"; \
    case ProjectOopHirNodeKind::CoreSimdLoadUnaligned128: return "core-simd-load-unaligned-128"; \
    case ProjectOopHirNodeKind::CoreSimdStoreUnaligned128: return "core-simd-store-unaligned-128"; \
    case ProjectOopHirNodeKind::CoreHalfFromFloat: return "core-half-from-float"; \
    case ProjectOopHirNodeKind::CoreHalfToFloat: return "core-half-to-float"; \
    case ProjectOopHirNodeKind::CoreSimdSwizzleWzyx: return "core-simd-swizzle-wzyx"; \
    case ProjectOopHirNodeKind::CoreSimdShuffleLowHigh: return "core-simd-shuffle-low-high"; \
    case ProjectOopHirNodeKind::CoreSimdSqrt: return "core-simd-sqrt"; \
    case ProjectOopHirNodeKind::CoreSimdReciprocalSqrt: return "core-simd-reciprocal-sqrt"; \
    case ProjectOopHirNodeKind::CoreSimdFusedMultiplyAdd: return "core-simd-fused-multiply-add"; \
    case ProjectOopHirNodeKind::CoreBitPopulationCount: return "core-bit-popcount"; \
    case ProjectOopHirNodeKind::CoreBitLeadingZeroCount: return "core-bit-clz"; \
    case ProjectOopHirNodeKind::CoreBitTrailingZeroCount: return "core-bit-ctz"; \
    case ProjectOopHirNodeKind::CoreBitRotateLeft: return "core-bit-rotate-left"; \
    case ProjectOopHirNodeKind::CoreBitRotateRight: return "core-bit-rotate-right"; \
    case ProjectOopHirNodeKind::CoreBitByteSwap: return "core-bit-byte-swap"; \
    case ProjectOopHirNodeKind::CoreCpuPrefetchRead: return "core-cpu-prefetch-read"; \
    case ProjectOopHirNodeKind::CoreCpuPause: return "core-cpu-pause"; \
    case ProjectOopHirNodeKind::CoreCpuReadTimestamp: return "core-cpu-read-timestamp"; \
    case ProjectOopHirNodeKind::CoreMutexCreate: return "core-mutex-create"; \
    case ProjectOopHirNodeKind::CoreMutexCreateNamed: return "core-mutex-create-named"; \
    case ProjectOopHirNodeKind::CoreMutexLock: return "core-mutex-lock"; \
    case ProjectOopHirNodeKind::CoreMutexUnlock: return "core-mutex-unlock"; \
    case ProjectOopHirNodeKind::CoreMutexClose: return "core-mutex-close";

#define NSC_HIR_NODE_KIND_NAMES_06 \
    case ProjectOopHirNodeKind::CoreMonitorEnter: return "core-monitor-enter"; \
    case ProjectOopHirNodeKind::CoreMonitorExit: return "core-monitor-exit"; \
    case ProjectOopHirNodeKind::CoreMonitorWait: return "core-monitor-wait"; \
    case ProjectOopHirNodeKind::CoreMonitorPulse: return "core-monitor-pulse"; \
    case ProjectOopHirNodeKind::CoreMonitorPulseAll: return "core-monitor-pulse-all"; \
    case ProjectOopHirNodeKind::CoreSemaphoreCreate: return "core-semaphore-create"; \
    case ProjectOopHirNodeKind::CoreSemaphoreCreateNamed: return "core-semaphore-create-named"; \
    case ProjectOopHirNodeKind::CoreSemaphoreWait: return "core-semaphore-wait"; \
    case ProjectOopHirNodeKind::CoreSemaphoreRelease: return "core-semaphore-release"; \
    case ProjectOopHirNodeKind::CoreSemaphoreClose: return "core-semaphore-close"; \
    case ProjectOopHirNodeKind::CoreEventCreate: return "core-event-create"; \
    case ProjectOopHirNodeKind::CoreEventCreateNamed: return "core-event-create-named"; \
    case ProjectOopHirNodeKind::CoreEventSet: return "core-event-set"; \
    case ProjectOopHirNodeKind::CoreEventReset: return "core-event-reset"; \
    case ProjectOopHirNodeKind::CoreEventWait: return "core-event-wait"; \
    case ProjectOopHirNodeKind::CoreEventClose: return "core-event-close"; \
    case ProjectOopHirNodeKind::CoreProcessGetCurrentProcessId: return "core-process-get-current-process-id"; \
    case ProjectOopHirNodeKind::CoreProcessGetWorkingSetSize: return "core-process-get-working-set-size"; \
    case ProjectOopHirNodeKind::CoreProcessGetKernelTime: return "core-process-get-kernel-time"; \
    case ProjectOopHirNodeKind::CoreProcessGetUserTime: return "core-process-get-user-time"; \
    case ProjectOopHirNodeKind::CoreProcessGetPriorityClass: return "core-process-get-priority-class"; \
    case ProjectOopHirNodeKind::CoreProcessSetPriorityClass: return "core-process-set-priority-class"; \
    case ProjectOopHirNodeKind::CoreSystemInfoGetPageSize: return "core-system-info-get-page-size"; \
    case ProjectOopHirNodeKind::CoreSystemInfoGetProcessorArchitecture: \
        return "core-system-info-get-processor-architecture"; \
    case ProjectOopHirNodeKind::CoreSystemInfoGetAllocationGranularity: \
        return "core-system-info-get-allocation-granularity"; \
    case ProjectOopHirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes: \
        return "core-system-info-get-total-physical-memory-bytes"; \
    case ProjectOopHirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes: \
        return "core-system-info-get-available-physical-memory-bytes"; \
    case ProjectOopHirNodeKind::CoreSystemInfoGetMemoryLoadPercent: \
        return "core-system-info-get-memory-load-percent"; \
    case ProjectOopHirNodeKind::CoreDebugIsDebuggerPresent: return "core-debug-is-debugger-present"; \
    case ProjectOopHirNodeKind::CoreDebugCaptureStackFrameCount: return "core-debug-capture-stack-frame-count"; \
    case ProjectOopHirNodeKind::CoreDebugOutputString: return "core-debug-output-string"; \
    case ProjectOopHirNodeKind::CoreThreadSleep: return "core-thread-sleep"; \
    case ProjectOopHirNodeKind::CoreFileCopy: return "core-file-copy"; \
    case ProjectOopHirNodeKind::CoreFileMove: return "core-file-move"; \
    case ProjectOopHirNodeKind::CoreDirectoryMove: return "core-directory-move"; \
    case ProjectOopHirNodeKind::DelegateCreate: return "delegate-create"; \
    case ProjectOopHirNodeKind::LambdaCreate: return "lambda-create"; \
    case ProjectOopHirNodeKind::ClosureCaptureValue: return "closure-capture-value"; \
    case ProjectOopHirNodeKind::ClosureCaptureAddress: return "closure-capture-address"; \
    case ProjectOopHirNodeKind::CaptureCellReference: return "capture-cell-reference";

#define NSC_HIR_NODE_KIND_NAMES_07 \
    case ProjectOopHirNodeKind::ClosureCaptureCellReference: return "closure-capture-cell-reference"; \
    case ProjectOopHirNodeKind::DelegateInvoke: return "delegate-invoke"; \
    case ProjectOopHirNodeKind::EventAdd: return "event-add"; \
    case ProjectOopHirNodeKind::EventRemove: return "event-remove"; \
    case ProjectOopHirNodeKind::EventInvoke: return "event-invoke"; \
    case ProjectOopHirNodeKind::Unary: return "unary"; \
    case ProjectOopHirNodeKind::Binary: return "binary"; \
    case ProjectOopHirNodeKind::Conditional: return "conditional"; \
    case ProjectOopHirNodeKind::NullCoalescing: return "null-coalescing"; \
    case ProjectOopHirNodeKind::OptionalCoalescing: return "optional-coalescing"; \
    case ProjectOopHirNodeKind::NullCoalescingAssignment: return "null-coalescing-assignment"; \
    case ProjectOopHirNodeKind::Assignment: return "assignment"; \
    case ProjectOopHirNodeKind::CompoundStore: return "compound-store"; \
    case ProjectOopHirNodeKind::Conversion: return "conversion"; \
    case ProjectOopHirNodeKind::RuntimeTypeTest: return "runtime-type-test"; \
    case ProjectOopHirNodeKind::SafeReferenceCast: return "safe-reference-cast"; \
    case ProjectOopHirNodeKind::CheckedReferenceCast: return "checked-reference-cast"; \
    case ProjectOopHirNodeKind::InstanceCall: return "instance-call"; \
    case ProjectOopHirNodeKind::StaticCall: return "static-call"; \
    case ProjectOopHirNodeKind::ConstructorInitializerCall: return "constructor-initializer-call"; \
    case ProjectOopHirNodeKind::ObjectConstruction: return "object-construction"; \
    case ProjectOopHirNodeKind::Deallocation: return "deallocation"; \
    case ProjectOopHirNodeKind::ObjectDestruction: return "object-destruction"; \
    case ProjectOopHirNodeKind::Assert: return "assert"; \
    case ProjectOopHirNodeKind::Unreachable: return "unreachable"; \
    case ProjectOopHirNodeKind::Assume: return "assume"; \
    case ProjectOopHirNodeKind::Panic: return "panic"; \
    case ProjectOopHirNodeKind::ResultPropagation: return "result-propagation"; \
    case ProjectOopHirNodeKind::LoopExpression: return "loop-expression"; \
    case ProjectOopHirNodeKind::MatchExpression: return "match-expression"; \
    case ProjectOopHirNodeKind::MatchArm: return "match-arm"; \
    case ProjectOopHirNodeKind::MatchValue: return "match-value"; \
    case ProjectOopHirNodeKind::OverflowPolicy: return "overflow-policy";

const char* projectOopHirNodeKindName(ProjectOopHirNodeKind kind) noexcept
{
    switch (kind)
    {
        NSC_HIR_NODE_KIND_NAMES_01
        NSC_HIR_NODE_KIND_NAMES_02
        NSC_HIR_NODE_KIND_NAMES_03
        NSC_HIR_NODE_KIND_NAMES_04
        NSC_HIR_NODE_KIND_NAMES_05
        NSC_HIR_NODE_KIND_NAMES_06
        NSC_HIR_NODE_KIND_NAMES_07
    }
    return "unknown";
}

#undef NSC_HIR_NODE_KIND_NAMES_01

#undef NSC_HIR_NODE_KIND_NAMES_02

#undef NSC_HIR_NODE_KIND_NAMES_03

#undef NSC_HIR_NODE_KIND_NAMES_04

#undef NSC_HIR_NODE_KIND_NAMES_05

#undef NSC_HIR_NODE_KIND_NAMES_06

#undef NSC_HIR_NODE_KIND_NAMES_07

std::string ProjectOopHirModel::mangleCallable(
    const NamedTypeDeclaration& type,
    const ClassMemberSymbol& member)
{
    const std::string prefix = member.kind == ClassMemberKind::Constructor
        ? (member.isStatic ? "__nsc_cctor_body_" : "__nsc_ctor_")
        : member.kind == ClassMemberKind::Destructor
            ? "__nsc_dtor_body_"
            : "__nsc_method_";
    // NSC-LIMIT-009: la identidad pasa por el codificador que conserva el `+`, no por `sanitize`.
    // El nombre del MIEMBRO si usa `sanitize`: no puede contener `+`, y cambiarlo moveria todos
    // los simbolos ya existentes.
    std::string result = prefix + compilation::symbolFragmentForIdentityKey(type.identity.key()) +
        "__" + sanitize(member.name) + typeSuffix(member);
    if (member.isControlledSpecialization)
        result += "__specialize_" + stableSymbolSuffix(member.genericTemplateKey);
    return result;
}

bool ProjectOopHirModel::initializeBuildState(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel,
    std::string& error)
{
    if (built_)
    {
        error = "ProjectOopHirModel instances are single-use";
        return false;
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() ||
        !interfaceModel.isBuilt() || !methodBodyModel.isBuilt() ||
        !constructionModel.isBuilt() || !objectLayoutModel.isBuilt())
    {
        error = "Stage 34 OOP HIR requires completed Stage 24-29 project models";
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;
    methodBodyModel_ = &methodBodyModel;
    namespaceFunctionModel_ = namespaceFunctionModel;
    constructionModel_ = &constructionModel;
    objectLayoutModel_ = &objectLayoutModel;
    nextSyntheticLocalOrdinal_ = methodBodyModel.locals().size();
    nextSyntheticCleanupRegionOrdinal_ = methodBodyModel.cleanupRegions().size();
    return true;
}

void ProjectOopHirModel::indexBuildBindings(
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel)
{
    if (lookupSource_ != nullptr) return;
    if (indexes_ != nullptr)
    {
        indexes_->rebuildHirBindings(
            methodBodyModel.locals(),
            methodBodyModel.lambdaBindings(),
            methodBodyModel.lambdaValueBindings(),
            methodBodyModel.cleanupRegions());
    }
    indexValuesBySyntax(nameBindingBySyntax_, methodBodyModel.nameBindings());
    indexValuesBySyntax(expressionTypeBySyntax_, methodBodyModel.expressionTypes());
    indexValuesBySyntax(callBySyntax_, methodBodyModel.calls());
    indexValuesBySyntax(namespaceFunctionCallBySyntax_, methodBodyModel.namespaceFunctionCalls());
    indexValuesBySyntax(switchBindingBySyntax_, methodBodyModel.switchBindings());
    indexValuesBySyntax(runtimeTypeBindingBySyntax_, methodBodyModel.runtimeTypeBindings());
    indexValuesBySyntax(delegateBindingBySyntax_, methodBodyModel.delegateBindings());
    if (indexes_ == nullptr)
    {
        indexValuesBySyntax(lambdaBindingBySyntax_, methodBodyModel.lambdaBindings());
        indexValuesBySyntax(lambdaValueBindingBySyntax_, methodBodyModel.lambdaValueBindings());
    }
    indexValuesBySyntax(eventBindingBySyntax_, methodBodyModel.eventBindings());
    indexValuesBySyntax(coreBindingBySyntax_, methodBodyModel.coreBindings());
    indexValuesBySyntax(optionalCarrierBindingBySyntax_, methodBodyModel.optionalCarrierBindings());
    indexValuesBySyntax(
        resultPropagationBindingBySyntax_, methodBodyModel.resultPropagationBindings());
    indexValuesBySyntax(rangeSliceBindingBySyntax_, methodBodyModel.rangeSliceBindings());
    indexValuesBySyntax(exceptionBindingBySyntax_, methodBodyModel.exceptionBindings());
    if (indexes_ == nullptr)
        indexValuesBySyntax(cleanupRegionBySyntax_, methodBodyModel.cleanupRegions());
    indexValuesBySyntax(usingBindingBySyntax_, methodBodyModel.usingBindings());
    indexValuesBySyntax(moveBindingBySyntax_, methodBodyModel.moveBindings());
    indexValuesBySyntax(borrowBindingBySyntax_, methodBodyModel.borrowBindings());
    indexValuesBySyntax(structuredExitBySyntax_, methodBodyModel.structuredExits());
    indexValuesBySyntax(constructionBySyntax_, constructionModel.constructions());
    if (indexes_ == nullptr) indexValuesBySyntax(localBySyntax_, methodBodyModel.locals());
}

bool ProjectOopHirModel::buildRegularFunctionsInParallel(
    std::size_t,
    std::string& error)
{
    using internal::RegularFunctionResult;
    std::vector<RegularFunctionResult> results(memberModel_->members().size());
    driver::CompilationWorkerPool& workers = *workerPool_;
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size();
         ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if ((member.kind != ClassMemberKind::Method &&
             member.kind != ClassMemberKind::Constructor &&
             member.kind != ClassMemberKind::Destructor) || member.isAbstract)
        {
            results[memberOrdinal].success = true;
            continue;
        }
        workers.submit([&, memberOrdinal](std::size_t) {
            ProjectOopHirModel worker;
            RegularFunctionResult& result = results[memberOrdinal];
            result.success = worker.buildImpl(
                *nameLookup_,
                *memberModel_,
                *inheritanceModel_,
                *interfaceModel_,
                *methodBodyModel_,
                *constructionModel_,
                *objectLayoutModel_,
                result.error,
                genericConstraintModel_,
                1,
                namespaceFunctionModel_,
                metaprogrammingModel_,
                memberOrdinal,
                this);
            result.errorRange = worker.lastErrorRange();
            if (result.success && !worker.functions_.empty())
                result.functions = std::move(worker.functions_);
        });
    }
    workers.wait();
    return mergeParallelRegularFunctions(results, error);
}

bool ProjectOopHirModel::mergeParallelRegularFunctions(
    std::vector<internal::RegularFunctionResult>& results,
    std::string& error)
{
    const std::size_t localFloor = methodBodyModel_->locals().size();
    const std::size_t cleanupFloor = methodBodyModel_->cleanupRegions().size();
    for (std::size_t memberOrdinal = 0; memberOrdinal < results.size(); ++memberOrdinal)
    {
        internal::RegularFunctionResult& result = results[memberOrdinal];
        if (!result.success)
        {
            error = std::move(result.error);
            lastErrorRange_ = result.errorRange;
            return false;
        }
        if (result.functions.empty()) continue;
        functionByMember_[memberOrdinal] = functions_.size();
        for (ProjectOopHirFunction& function : result.functions)
        {
            std::size_t localCount = 0;
            std::size_t cleanupCount = 0;
            std::size_t regionCount = 0;
            scanParallelOrdinals(
                function.body.get(),
                localFloor,
                cleanupFloor,
                localCount,
                cleanupCount,
                regionCount);
            applyParallelOrdinalOffsets(
                function.body.get(),
                localFloor,
                cleanupFloor,
                nextSyntheticLocalOrdinal_ - localFloor,
                nextSyntheticCleanupRegionOrdinal_ - cleanupFloor,
                nextHirRegionOrdinal_);
            nextSyntheticLocalOrdinal_ += localCount;
            nextSyntheticCleanupRegionOrdinal_ += cleanupCount;
            nextHirRegionOrdinal_ += regionCount;
            functions_.push_back(std::move(function));
        }
    }
    return true;
}

bool ProjectOopHirModel::regularFunctionShouldBeSkipped(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& member) const
{
    if ((member.kind != ClassMemberKind::Method &&
         member.kind != ClassMemberKind::Constructor &&
         member.kind != ClassMemberKind::Destructor) || member.isAbstract)
        return true;
    // NSC-LIMIT-018. Signatures of a closed generic type are eager. For executables, ordinary
    // template method bodies are analyzed only after a real call or delegate conversion
    // selects them. Library products additionally demand their complete public/protected
    // closed surface because an external consumer is itself the call site. In both cases this
    // exact query prevents HIR from lowering a body for which semantic bindings do not exist.
    constexpr std::string_view kDeferredSpecializationPrefix = "stage44-specialization:";
    const ast::AstNode* declaringSyntaxForDemand =
        member.declaringTypeOrdinal < nameLookup_->namedTypes().size()
            ? nameLookup_->namedTypes()[member.declaringTypeOrdinal].syntax
            : nullptr;
    const bool deferredGenericMethod = member.kind == ClassMemberKind::Method &&
        !member.isVirtual && !member.isOverride && !member.isAbstract &&
        ((member.syntax != nullptr &&
          member.syntax->auxiliaryText().starts_with(kDeferredSpecializationPrefix)) ||
         (declaringSyntaxForDemand != nullptr &&
          declaringSyntaxForDemand->auxiliaryText().starts_with(
              kDeferredSpecializationPrefix)));
    if (deferredGenericMethod && !methodBodyModel_->callableBodyAnalyzed(memberOrdinal))
        return true;
    // NSC-LIMIT-007. La misma pregunta que hace el modelo de cuerpos, y hace falta aqui
    // TAMBIEN: este bucle vuelve a recorrer todos los miembros y baja sus cuerpos, asi que
    // saltarla solo alla dejaria un miembro con cuerpo sin analizar del que HIR pediria
    // bindings que nadie escribio.
    //
    // Se preguntan DOS nodos, y hacen falta los dos. Para un METODO generico la invalidez
    // esta en su propio nodo; para un TIPO generico --`Caja<int>` con `where T : class`--
    // esta en el nodo del tipo, y sus metodos son hijos suyos, asi que preguntar solo por
    // `member.syntax` no la veria nunca.
    if (genericConstraintModel_ != nullptr &&
        (!genericConstraintModel_->specializationSatisfiesConstraints(member.syntax) ||
         (member.declaringTypeOrdinal < nameLookup_->namedTypes().size() &&
          !genericConstraintModel_->specializationSatisfiesConstraints(
              nameLookup_->namedTypes()[member.declaringTypeOrdinal].syntax))))
        return true;
    if (member.kind == ClassMemberKind::Destructor && member.isPrecompiled)
    {
        // The consumer needs the declaration to build the destruction plan, but never imports
        // or regenerates the destructor body. Destruction crosses the producer-owned entry
        // stored in the runtime class descriptor.
        return true;
    }
    return false;
}

bool ProjectOopHirModel::appendPrecompiledRegularFunction(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& member,
    std::string& error)
{
    // The declaration came from .nsmeta and participates in the ordinary NativeSharp
    // overload set, but its implementation is already in the referenced .lib/DLL. The
    // The semantic signature came from synthetic source, but the physical identity was
    // produced once by the library producer and transported through NSMETA.  Recomputing
    // it here is incorrect when a closed generic argument acquired a qualified spelling
    // during source reconstruction.
    ProjectOopHirFunction imported;
    imported.memberOrdinal = memberOrdinal;
    imported.declaringTypeOrdinal = member.declaringTypeOrdinal;
    imported.constructor = member.kind == ClassMemberKind::Constructor;
    imported.destructor = false;
    imported.isStatic = member.isStatic;
    imported.referenceReturn = member.isReferenceReturn;
    imported.isNoHeap = member.isNoHeap;
    imported.effects = member.effects;
    // NSMETA declarations carry optimization policy as part of their callable contract.
    // In particular, `noinline` has no transported body but must still constrain calls
    // emitted by this consumer.
    imported.inliningPolicy = member.inliningPolicy;
    imported.isImportedNative = true;
    if (member.precompiledMangledName.empty())
    {
        error = "precompiled callable '" + member.signatureKey +
            "' has no physical symbol bound from NSMETA";
        return false;
    }
    imported.mangledName = member.precompiledMangledName;
    imported.returnTypeKey = imported.constructor ? "builtin:void" : member.declaredTypeKey;
    imported.errorTypeKey = member.declaredErrorTypeKey;
    imported.returnOwnership = imported.constructor
        ? sema::OwnershipKind::None
        : member.declaredOwnership;
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        imported.parameterTypeKeys.push_back(parameter.typeKey);
        imported.parameterPassKinds.push_back(parameter.passKind);
        imported.parameterOwnerships.push_back(parameter.ownership);
    }
    functionByMember_[memberOrdinal] = functions_.size();
    functions_.push_back(std::move(imported));
    return true;
}

void ProjectOopHirModel::appendImportedCRegularFunction(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& member)
{
    // Interop Foundation: an imported C function is a DECLARATION by definition -- the
    // body lives in the named DLL. Record the function (C symbol as its mangled name, no
    // body) so ordinary call resolution and the backend's declaration pass see it, and
    // move on before the body requirement below.
    ProjectOopHirFunction imported;
    imported.memberOrdinal = memberOrdinal;
    imported.declaringTypeOrdinal = member.declaringTypeOrdinal;
    imported.isStatic = true;
    imported.isImportedC = true;
    imported.isVariadic = member.isVariadic;
    imported.importDllName = member.importDllName;
    imported.importRequiresRuntimeBinding = member.importRequiresRuntimeBinding;
    imported.cAbiName = member.name;
    // C1.2 "import identity": `cAbiName` is always the EXTERNAL symbol (what the DLL or
    // the archive exports); `mangledName` is the INTERNAL name the module and every call
    // site use. In the ordinary case the two coincide, which is exactly what makes the
    // link resolve the import statically by name -- unchanged, and what the certified
    // user32/kernel32 scenarios exercise. When one symbol name is claimed by two
    // libraries the two MUST diverge: a single LLVM module cannot hold two functions
    // called `Init`, and asking for it silently produced `Init.1`, which the linker then
    // could not resolve (measured). The internal name is derived from the whole identity,
    // so `a.dll!Init` and `b.dll!Init` are two distinct functions from here on.
    imported.mangledName = member.importRequiresRuntimeBinding
        ? importIdentityInternalName(member.importDllName, member.name)
        : member.name;
    imported.returnTypeKey = member.declaredTypeKey;
    imported.returnOwnership = sema::OwnershipKind::None;
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        imported.parameterTypeKeys.push_back(parameter.typeKey);
        imported.parameterPassKinds.push_back(parameter.passKind);
        imported.parameterOwnerships.push_back(parameter.ownership);
    }
    functionByMember_[memberOrdinal] = functions_.size();
    functions_.push_back(std::move(imported));
}

void ProjectOopHirModel::initializeRegularFunction(
    ProjectOopHirFunction& function,
    std::size_t memberOrdinal,
    const ClassMemberSymbol& member)
{
    function.memberOrdinal = memberOrdinal;
    function.declaringTypeOrdinal = member.declaringTypeOrdinal;
    function.constructor = member.kind == ClassMemberKind::Constructor;
    function.destructor = member.kind == ClassMemberKind::Destructor;
    function.isStatic = member.isStatic;
    function.referenceReturn = member.isReferenceReturn;
    function.isNoHeap = member.isNoHeap;
    function.effects = member.effects;
    function.inliningPolicy = member.inliningPolicy;
    function.isTransportedInlineBody = member.isTransportedInlineBody;
    function.isExportedC = member.isExportedC;
    function.cAbiName = member.cAbiName;
    constexpr std::string_view kSpecializationPrefix = "stage44-specialization:";
    if (member.syntax != nullptr &&
        member.syntax->auxiliaryText().starts_with(kSpecializationPrefix))
    {
        function.genericSpecializationKey = hirSpecializationKey(member.syntax);
    }
    else
    {
        const ast::AstNode* declaringSyntax =
            nameLookup_->namedTypes()[member.declaringTypeOrdinal].syntax;
        if (declaringSyntax != nullptr &&
            declaringSyntax->auxiliaryText().starts_with(kSpecializationPrefix))
            function.genericSpecializationKey = hirSpecializationKey(declaringSyntax);
    }
    function.mangledName = mangleCallable(
        nameLookup_->namedTypes()[member.declaringTypeOrdinal], member);
    function.returnTypeKey = (function.constructor || function.destructor)
        ? "builtin:void"
        : member.declaredTypeKey;
    function.errorTypeKey = member.declaredErrorTypeKey;
    function.frameStorage = member.frameStorage;
    function.returnOwnership = (function.constructor || function.destructor)
        ? sema::OwnershipKind::None
        : member.declaredOwnership;
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        function.parameterNames.push_back(parameter.name);
        function.parameterTypeKeys.push_back(parameter.typeKey);
        function.parameterPassKinds.push_back(parameter.passKind);
        function.parameterOwnerships.push_back(parameter.ownership);
    }
    for (std::size_t localOrdinal = 0; localOrdinal < methodBodyModel_->locals().size();
         ++localOrdinal)
    {
        const MethodLocalSymbol& local = methodBodyModel_->locals()[localOrdinal];
        if (local.methodMemberOrdinal == memberOrdinal && !local.lambdaOrdinal.has_value())
            function.localOrdinals.push_back(localOrdinal);
    }
}

bool ProjectOopHirModel::appendRegularFunctionParameterCleanups(
    ProjectOopHirFunction& function,
    const ClassMemberSymbol& member,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& cleanupNodes,
    std::string& error)
{
    activeHirCleanupRegions_.clear();
    for (std::size_t cleanupOrdinal = 0;
         cleanupOrdinal < methodBodyModel_->cleanupRegions().size();
         ++cleanupOrdinal)
    {
        const MethodCleanupRegion& region = methodBodyModel_->cleanupRegions()[cleanupOrdinal];
        if (region.methodMemberOrdinal != function.memberOrdinal ||
            (region.kind != MethodCleanupRegionKind::OwnedParameterDestroy &&
             region.kind != MethodCleanupRegionKind::ValueParameterDrop) ||
            !region.resourceParameterOrdinal.has_value())
        {
            continue;
        }
        const std::size_t parameterOrdinal = *region.resourceParameterOrdinal;
        if (parameterOrdinal >= member.parameters.size())
        {
            error = "Stage 50.8 owned-parameter cleanup has an invalid parameter ordinal";
            return false;
        }
        activeHirCleanupRegions_.push_back(cleanupOrdinal);
        auto cleanup = makeNode(
            ProjectOopHirNodeKind::OwnedParameterCleanup,
            member.syntax,
            member.parameters[parameterOrdinal].typeKey);
        cleanup->parameterOrdinal = parameterOrdinal;
        cleanup->cleanupRegionOrdinal = cleanupOrdinal;
        cleanup->automaticDestruction =
            region.kind == MethodCleanupRegionKind::ValueParameterDrop;
        cleanupNodes.push_back(std::move(cleanup));
    }
    return true;
}

bool ProjectOopHirModel::appendConstructorInitializerArguments(
    ProjectOopHirNode& initializerCall,
    const ConstructorInitializerBinding& binding,
    const ClassMemberSymbol& selectedConstructor,
    std::size_t memberOrdinal,
    bool& hasOwnedTemporary,
    std::string& error)
{
    ProjectOopHirNode* expandedPack = nullptr;
    for (std::size_t index = 0; index < binding.initializerSyntax->children().size();
         ++index)
    {
        const ast::AstNode* argumentSyntax = binding.initializerSyntax->children()[index];
        auto argument = lowerExpression(argumentSyntax, memberOrdinal, false, error);
        if (argument == nullptr) return false;
        if (index < binding.selectedConversions.size() &&
            binding.selectedConversions[index].kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), binding.selectedConversions[index], argumentSyntax);
        }
        if (index >= binding.selectedArgumentParameterOrdinals.size())
        {
            error = "named constructor initializer argument has no parameter ordinal";
            return false;
        }
        const std::size_t parameterIndex = binding.selectedArgumentParameterOrdinals[index];
        const bool packElement = index < binding.selectedParameterPackElements.size() &&
            binding.selectedParameterPackElements[index];
        if (packElement)
        {
            if (parameterIndex >= selectedConstructor.parameters.size())
            {
                error = "T01-F02 constructor initializer pack has no parameter";
                return false;
            }
            const std::string& elementType =
                selectedConstructor.parameters[parameterIndex].parameterPackElementTypeKey;
            if (elementType.empty())
            {
                error = "T01-F02 constructor initializer pack lost its element type";
                return false;
            }
            if (expandedPack == nullptr)
            {
                auto pack = makeNode(
                    ProjectOopHirNodeKind::ParameterPack,
                    argumentSyntax,
                    selectedConstructor.parameters[parameterIndex].typeKey);
                pack->elementTypeKey = elementType;
                expandedPack = pack.get();
                initializerCall.children.push_back(std::move(pack));
                initializerCall.argumentParameterOrdinals.push_back(parameterIndex);
            }
            expandedPack->children.push_back(std::move(argument));
            continue;
        }
        if (std::ranges::find(
                binding.selectedFullExpressionBorrowArgumentOrdinals,
                index) != binding.selectedFullExpressionBorrowArgumentOrdinals.end())
        {
            argument = wrapFreshOwnedTemporaryBorrow(
                std::move(argument), argumentSyntax, hasOwnedTemporary);
        }
        initializerCall.children.push_back(std::move(argument));
        initializerCall.argumentParameterOrdinals.push_back(parameterIndex);
    }
    return true;
}

bool ProjectOopHirModel::appendConstructorInitializerDefaults(
    ProjectOopHirNode& initializerCall,
    const ConstructorInitializerBinding& binding,
    const ClassMemberSymbol& selectedConstructor,
    std::string& error)
{
    for (std::size_t index = 0; index < binding.selectedDefaultArguments.size(); ++index)
    {
        if (index >= binding.selectedDefaultArgumentParameterOrdinals.size())
        {
            error = "selected constructor initializer default has no parameter ordinal";
            return false;
        }
        const std::size_t parameterIndex =
            binding.selectedDefaultArgumentParameterOrdinals[index];
        if (parameterIndex >= selectedConstructor.parameters.size())
        {
            error = "NSC-LIMIT-020 constructor initializer default has no parameter";
            return false;
        }
        auto argument = lowerDefaultArgument(
            binding.selectedDefaultArguments[index],
            selectedConstructor.parameters[parameterIndex].typeKey,
            error);
        if (argument == nullptr) return false;
        initializerCall.children.push_back(std::move(argument));
        initializerCall.argumentParameterOrdinals.push_back(parameterIndex);
    }
    if (!binding.selectedSynthesizesEmptyParameterPack) return true;
    if (!binding.selectedParameterPackOrdinal.has_value() ||
        *binding.selectedParameterPackOrdinal >= selectedConstructor.parameters.size())
    {
        error = "T01-F02 empty constructor initializer pack has no parameter";
        return false;
    }
    const std::size_t parameterIndex = *binding.selectedParameterPackOrdinal;
    const std::string& elementType =
        selectedConstructor.parameters[parameterIndex].parameterPackElementTypeKey;
    if (elementType.empty())
    {
        error = "T01-F02 empty constructor initializer pack lost its element type";
        return false;
    }
    auto pack = makeNode(
        ProjectOopHirNodeKind::ParameterPack,
        binding.initializerSyntax,
        selectedConstructor.parameters[parameterIndex].typeKey);
    pack->elementTypeKey = elementType;
    initializerCall.children.push_back(std::move(pack));
    initializerCall.argumentParameterOrdinals.push_back(parameterIndex);
    return true;
}

namespace
{
bool validateNoHeapConstructorInitializer(
    const ClassMemberSymbol& member,
    const ConstructorInitializerBinding* binding,
    const ProjectMemberModel* memberModel,
    std::string& error)
{
    if (!member.isNoHeap || binding == nullptr ||
        (binding->resolution != ConstructorInitializerResolutionKind::ResolvedExplicit &&
         binding->resolution !=
             ConstructorInitializerResolutionKind::ResolvedImplicitDefault))
        return true;
    if (!binding->selectedEntryConstructorMemberOrdinal.has_value() ||
        *binding->selectedEntryConstructorMemberOrdinal >= memberModel->members().size())
    {
        error = "NS3030: noheap constructor '" + member.signatureKey +
            "' cannot call an implicit constructor without a noheap contract";
        return false;
    }
    const ClassMemberSymbol& selectedConstructor = memberModel->members()[
        *binding->selectedEntryConstructorMemberOrdinal];
    if (selectedConstructor.isNoHeap) return true;
    error = "NS3030: noheap constructor '" + member.signatureKey +
        "' cannot call non-noheap constructor '" +
        selectedConstructor.signatureKey + "'";
    return false;
}
}

bool ProjectOopHirModel::prependConstructorWork(
    ProjectOopHirFunction& function,
    std::size_t memberOrdinal,
    const ClassMemberSymbol& member,
    std::string& error)
{
    if (!function.constructor || function.isStatic) return true;
    std::vector<std::unique_ptr<ProjectOopHirNode>> prefix;
    const ConstructorInitializerBinding* binding =
        constructionModel_->constructorInitializerBindingForMember(memberOrdinal);
    if (!validateNoHeapConstructorInitializer(member, binding, memberModel_, error)) return false;
    if (!appendConstructorInitializerCall(prefix, memberOrdinal, binding, error)) return false;
    if (!appendConstructorFieldInitializerWork(
            prefix, memberOrdinal, member, binding, error))
        return false;
    function.body->children.insert(
        function.body->children.begin(),
        std::make_move_iterator(prefix.begin()),
        std::make_move_iterator(prefix.end()));
    return true;
}

bool ProjectOopHirModel::appendConstructorInitializerCall(
    std::vector<std::unique_ptr<ProjectOopHirNode>>& prefix,
    std::size_t memberOrdinal,
    const ConstructorInitializerBinding* binding,
    std::string& error)
{
    if (binding != nullptr &&
        (binding->resolution == ConstructorInitializerResolutionKind::ResolvedExplicit ||
         binding->resolution ==
             ConstructorInitializerResolutionKind::ResolvedImplicitDefault) &&
        binding->selectedEntryConstructorMemberOrdinal.has_value() &&
        binding->targetTypeOrdinal.has_value())
    {
        auto initializerCall = makeNode(
            ProjectOopHirNodeKind::ConstructorInitializerCall,
            binding->initializerSyntax,
            "builtin:void");
        initializerCall->memberOrdinal = binding->selectedEntryConstructorMemberOrdinal;
        initializerCall->baseReceiver =
            binding->kind != ConstructorInitializerKind::ExplicitThis;
        initializerCall->children.push_back(makeNode(
            ProjectOopHirNodeKind::ThisValue,
            binding->initializerSyntax,
            inheritanceModel_->typeKeyForOrdinal(*binding->targetTypeOrdinal)));
        bool hasOwnedTemporary = false;
        if (binding->resolution == ConstructorInitializerResolutionKind::ResolvedExplicit &&
            binding->initializerSyntax != nullptr)
        {
            if (!binding->selectedEntryConstructorMemberOrdinal.has_value() ||
                *binding->selectedEntryConstructorMemberOrdinal >= memberModel_->members().size())
            {
                error = "NSC-LIMIT-020 selected constructor initializer has no callable metadata";
                return false;
            }
            const ClassMemberSymbol& selectedConstructor = memberModel_->members()[
                *binding->selectedEntryConstructorMemberOrdinal];
            if (!appendConstructorInitializerArguments(
                    *initializerCall,
                    *binding,
                    selectedConstructor,
                    memberOrdinal,
                    hasOwnedTemporary,
                    error) ||
                !appendConstructorInitializerDefaults(
                    *initializerCall, *binding, selectedConstructor, error))
                return false;
        }
        prefix.push_back(wrapFullExpressionCleanup(
            std::move(initializerCall), binding->initializerSyntax, hasOwnedTemporary));
    }
    return true;
}

bool ProjectOopHirModel::appendConstructorFieldInitializerWork(
    std::vector<std::unique_ptr<ProjectOopHirNode>>& prefix,
    std::size_t memberOrdinal,
    const ClassMemberSymbol& member,
    const ConstructorInitializerBinding* binding,
    std::string& error)
{
    // A this(...) delegating constructor must not execute this type's field initializers:
    // the terminal constructor in the delegation chain executes them exactly once.
    const bool delegatesToThis = binding != nullptr &&
        binding->kind == ConstructorInitializerKind::ExplicitThis;
    if (!delegatesToThis)
    {
        // When the immediate base has only an implicit constructor, Macro A can elide the
        // physical callable and jump to the nearest explicit ancestor.  Stage 37 must still
        // execute field initializers for every skipped implicit base, in base-to-derived order.
        if (binding != nullptr &&
            binding->resolution ==
                ConstructorInitializerResolutionKind::ResolvedImplicitDefault &&
            binding->targetTypeOrdinal.has_value())
        {
            for (const std::size_t typeOrdinal : implicitInitializerTypes(
                     *binding->targetTypeOrdinal,
                     binding->selectedEntryConstructorMemberOrdinal))
            {
                appendInstanceFieldInitializers(typeOrdinal, prefix, error, memberOrdinal);
                if (!error.empty()) return false;
            }
        }
        appendInstanceFieldInitializers(
            member.declaringTypeOrdinal, prefix, error, memberOrdinal);
        if (!error.empty()) return false;
    }
    return true;
}

void ProjectOopHirModel::promoteEligibleStackClassLocals(
    ProjectOopHirFunction& function,
    const std::vector<ProjectOopHirNode*>& localDeclarations)
{
    // Item 323. Promotion is intentionally a post-HIR proof: every use of each local is now
    // visible, including nested control flow and synthesized work. Only a trivial, local class
    // construction whose identity never becomes observable may exchange owned heap storage
    // for frame storage. The one-mebibyte cap matches explicit stackalloc's fixed budget.
    constexpr std::uint64_t kMaximumPromotedObjectBytes = std::uint64_t{1} << 20;
    for (ProjectOopHirNode* declaration : localDeclarations)
    {
        if (declaration == nullptr || !declaration->localOrdinal.has_value() ||
            declaration->children.size() != 1 || declaration->children.front() == nullptr)
            continue;
        ProjectOopHirNode* construction = declaration->children.front().get();
        if (construction->kind != ProjectOopHirNodeKind::ObjectConstruction ||
            construction->argumentCount != 0 || !construction->children.empty() ||
            !construction->constructionOrdinal.has_value() ||
            !construction->targetTypeOrdinal.has_value())
            continue;
        const MethodLocalSymbol* local = localSymbol(declaration->syntax);
        const ObjectConstructionBinding* binding = constructionBinding(construction->syntax);
        const std::size_t targetTypeOrdinal = *construction->targetTypeOrdinal;
        const ClassObjectLayout* layout = objectLayoutModel_ != nullptr
            ? objectLayoutModel_->layoutForType(targetTypeOrdinal) : nullptr;
        if (local == nullptr || local->isStatic || !local->isRaii || local->isReference ||
            local->objectLifetimeOwnership != sema::OwnershipKind::Owned ||
            local->storageOwnership != sema::OwnershipKind::Owned || binding == nullptr ||
            !binding->selectedImplicitDefault ||
            binding->selectedConstructorMemberOrdinal.has_value() ||
            binding->implicitDefaultBaseConstructorMemberOrdinal.has_value() ||
            layout == nullptr || layout->baseTypeOrdinal.has_value() || layout->size == 0 ||
            layout->size > kMaximumPromotedObjectBytes ||
            targetTypeOrdinal >= nameLookup_->namedTypes().size() ||
            nameLookup_->namedTypes()[targetTypeOrdinal].isPrecompiled)
            continue;
        bool hasNontrivialLifetimeMember = false;
        std::vector<std::size_t> fallbackMemberOrdinals;
        std::span<const std::size_t> memberOrdinals =
            memberModel_->membersDeclaredByType(targetTypeOrdinal);
        if (!memberModel_->hasMembersDeclaredByTypeIndex())
        {
            for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
            {
                if (memberModel_->members()[ordinal].declaringTypeOrdinal == targetTypeOrdinal)
                    fallbackMemberOrdinals.push_back(ordinal);
            }
            memberOrdinals = fallbackMemberOrdinals;
        }
        for (const std::size_t memberOrdinal : memberOrdinals)
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& targetMember = memberModel_->members()[memberOrdinal];
            if (targetMember.kind == ClassMemberKind::Destructor ||
                (targetMember.kind == ClassMemberKind::Field && !targetMember.isStatic &&
                 targetMember.declaredOwnership == sema::OwnershipKind::Owned))
            {
                hasNontrivialLifetimeMember = true;
                break;
            }
        }
        if (hasNontrivialLifetimeMember ||
            !stackClassLocalUsesAreSafe(
                function.body.get(), declaration, *declaration->localOrdinal))
            continue;
        declaration->stackPromotedObject = true;
        declaration->storageOwnership = sema::OwnershipKind::None;
        construction->stackPromotedObject = true;
        construction->localOrdinal = declaration->localOrdinal;
        construction->storageOwnership = sema::OwnershipKind::None;
    }
}

void ProjectOopHirModel::promoteScopeClassLocals(
    const std::vector<ProjectOopHirNode*>& localDeclarations)
{
    // NS304-T13-F03: `scope T x = new T(args)` forces frame storage even with constructor
    // arguments. Sema has already rejected escape, so the alloca cannot dangle.
    constexpr std::uint64_t kMaximumPromotedObjectBytes = std::uint64_t{1} << 20;
    for (ProjectOopHirNode* declaration : localDeclarations)
    {
        if (declaration == nullptr || !declaration->localOrdinal.has_value() ||
            declaration->children.size() != 1 || declaration->children.front() == nullptr)
            continue;
        ProjectOopHirNode* construction = declaration->children.front().get();
        if (construction->kind != ProjectOopHirNodeKind::ObjectConstruction ||
            !construction->constructionOrdinal.has_value() ||
            !construction->targetTypeOrdinal.has_value())
            continue;
        const MethodLocalSymbol* local = localSymbol(declaration->syntax);
        const std::size_t targetTypeOrdinal = *construction->targetTypeOrdinal;
        const ClassObjectLayout* layout = objectLayoutModel_ != nullptr
            ? objectLayoutModel_->layoutForType(targetTypeOrdinal) : nullptr;
        if (local == nullptr || !local->isScope || local->isStatic || local->isReference ||
            layout == nullptr || layout->baseTypeOrdinal.has_value() || layout->size == 0 ||
            layout->size > kMaximumPromotedObjectBytes ||
            targetTypeOrdinal >= nameLookup_->namedTypes().size() ||
            nameLookup_->namedTypes()[targetTypeOrdinal].isPrecompiled)
            continue;
        declaration->stackPromotedObject = true;
        declaration->storageOwnership = sema::OwnershipKind::None;
        construction->stackPromotedObject = true;
        construction->localOrdinal = declaration->localOrdinal;
        construction->storageOwnership = sema::OwnershipKind::None;
    }
}

bool ProjectOopHirModel::validateNoHeapRegularFunction(
    const ProjectOopHirFunction& function,
    const ClassMemberSymbol& member,
    std::string& error) const
{
    // Object construction is deferred until after the proof so a noheap callable can use a
    // proven frame object. Every construction that remains heap-backed keeps item 321's exact
    // diagnostic and contract.
    if (!function.isNoHeap) return true;
    const ProjectOopHirNode* allocation =
        firstUnpromotedObjectConstruction(function.body.get());
    if (allocation == nullptr) return true;
    error = "NS3030: noheap callable '" + member.name +
        "' cannot contain direct heap allocation '" +
        std::string(projectOopHirNodeKindName(allocation->kind)) + "'";
    return false;
}

bool ProjectOopHirModel::appendRegularCoroutineFunctions(
    ProjectOopHirFunction function,
    std::string& error)
{
    std::unique_ptr<ProjectOopHirFunction> poll;
    std::unique_ptr<ProjectOopHirFunction> moveNext;
    std::unique_ptr<ProjectOopHirFunction> drop;
    // NS304-T07-F02: an async generator is ONE machine (await + yield) — the async-generator
    // split produces MoveNext (future) + poll + drop. A precompiled declaration (no lowered
    // body) is NOT re-split: its facade symbols live in the referenced .lib, so the consumer
    // calls them instead of synthesizing empty duplicates.
    const bool hasBody = function.body != nullptr && !function.isImportedNative;
    const bool asyncGenerator = hasBody && !function.constructor && !function.destructor &&
        function.effects.contains(sema::Effect::Generator) &&
        function.effects.contains(sema::Effect::Async);
    if (asyncGenerator)
    {
        auto split = splitAsyncGeneratorFunction(function, error);
        if (split.size() != 3U) return false;
        moveNext = std::move(split[0]);
        poll = std::move(split[1]);
        drop = std::move(split[2]);
    }
    else
    {
        if (hasBody && !function.constructor && !function.destructor &&
            function.effects.contains(sema::Effect::Async))
        {
            poll = splitAsyncFunction(function, error);
            if (poll == nullptr) return false;
        }
        if (hasBody && !function.constructor && !function.destructor &&
            function.effects.contains(sema::Effect::Generator))
        {
            auto split = splitGeneratorFunction(function, error);
            if (split.first == nullptr) return false;
            moveNext = std::move(split.first);
            drop = std::move(split.second);
        }
    }
    functionByMember_[function.memberOrdinal] = functions_.size();
    functions_.push_back(std::move(function));
    if (poll) functions_.push_back(std::move(*poll));
    if (moveNext) functions_.push_back(std::move(*moveNext));
    if (drop) functions_.push_back(std::move(*drop));
    return true;
}

bool ProjectOopHirModel::buildRegularFunction(
    std::size_t memberOrdinal,
    std::string& error)
{
    const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
    if (regularFunctionShouldBeSkipped(memberOrdinal, member)) return true;
    if ((member.kind == ClassMemberKind::Method ||
         member.kind == ClassMemberKind::Constructor) &&
        member.isPrecompiled && !member.isImportedC)
        return appendPrecompiledRegularFunction(memberOrdinal, member, error);
    if (member.isImportedC)
    {
        appendImportedCRegularFunction(memberOrdinal, member);
        return true;
    }
    if (member.syntax == nullptr || member.syntax->first() == nullptr)
    {
        error = "Stage 35 concrete OOP callable has no body: " + member.signatureKey;
        return false;
    }
    if (member.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        error = "Stage 34 OOP callable refers to invalid declaring type ordinal";
        return false;
    }

    ProjectOopHirFunction function;
    initializeRegularFunction(function, memberOrdinal, member);
    std::vector<std::unique_ptr<ProjectOopHirNode>> cleanupNodes;
    if (!appendRegularFunctionParameterCleanups(function, member, cleanupNodes, error))
        return false;
    loweringAsyncBody_ = function.effects.contains(sema::Effect::Async);
    function.body = lowerBlock(member.syntax->first(), memberOrdinal, error);
    activeHirCleanupRegions_.clear();
    if (function.body == nullptr) return false;
    if (!cleanupNodes.empty())
    {
        function.body->children.insert(
            function.body->children.begin(),
            std::make_move_iterator(cleanupNodes.begin()),
            std::make_move_iterator(cleanupNodes.end()));
    }
    if (!prependConstructorWork(function, memberOrdinal, member, error)) return false;
    std::vector<ProjectOopHirNode*> localDeclarations;
    collectLocalDeclarations(function.body.get(), localDeclarations);
    promoteEligibleStackClassLocals(function, localDeclarations);
    promoteScopeClassLocals(localDeclarations);
    if (!validateNoHeapRegularFunction(function, member, error)) return false;
    return appendRegularCoroutineFunctions(std::move(function), error);
}

bool ProjectOopHirModel::buildRegularFunctionsSequentially(
    std::optional<std::size_t> memberFilter,
    std::string& error)
{
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size();
         ++memberOrdinal)
    {
        if (memberFilter.has_value() && *memberFilter != memberOrdinal) continue;
        if (!buildRegularFunction(memberOrdinal, error)) return false;
    }
    return true;
}

void ProjectOopHirModel::initializeNamespaceFunction(
    ProjectOopHirFunction& function,
    std::size_t functionOrdinal,
    const NamespaceFunctionSymbol& symbol) const
{
    function.memberOrdinal = memberModel_->members().size() + functionOrdinal;
    function.namespaceFunctionOrdinal = functionOrdinal;
    function.declaringTypeOrdinal = nameLookup_->namedTypes().size();
    function.isStatic = true;
    function.inliningPolicy = symbol.inliningPolicy;
    function.referenceReturn = symbol.isReferenceReturn;
    function.isVariadic = symbol.isVariadic;
    function.isExportedC = symbol.isExportedC;
    if (symbol.isExportedC) function.cAbiName = symbol.name;
    function.mangledName = symbol.mangledName;
    function.debugDisplayName = symbol.namespaceName.empty()
        ? symbol.name
        : symbol.namespaceName + "." + symbol.name;
    function.returnTypeKey = symbol.declaredTypeKey;
    function.errorTypeKey = symbol.declaredErrorTypeKey;
    function.effects = symbol.effects;
    function.returnOwnership = symbol.declaredOwnership;
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        function.parameterNames.push_back(parameter.name);
        function.parameterTypeKeys.push_back(parameter.typeKey);
        function.parameterPassKinds.push_back(parameter.passKind);
        function.parameterOwnerships.push_back(parameter.ownership);
    }
}

bool ProjectOopHirModel::appendNamespaceFunctionParameterCleanups(
    ProjectOopHirFunction&,
    std::size_t callableOrdinal,
    const NamespaceFunctionSymbol& symbol,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& cleanupNodes,
    std::string& error)
{
    activeHirCleanupRegions_.clear();
    for (std::size_t cleanupOrdinal = 0;
         cleanupOrdinal < methodBodyModel_->cleanupRegions().size();
         ++cleanupOrdinal)
    {
        const MethodCleanupRegion& region = methodBodyModel_->cleanupRegions()[cleanupOrdinal];
        if (region.methodMemberOrdinal != callableOrdinal ||
            (region.kind != MethodCleanupRegionKind::OwnedParameterDestroy &&
             region.kind != MethodCleanupRegionKind::ValueParameterDrop) ||
            !region.resourceParameterOrdinal.has_value())
        {
            continue;
        }
        const std::size_t parameterOrdinal = *region.resourceParameterOrdinal;
        if (parameterOrdinal >= symbol.parameters.size())
        {
            error = "namespace function owned-parameter cleanup has an invalid parameter ordinal";
            return false;
        }
        activeHirCleanupRegions_.push_back(cleanupOrdinal);
        auto cleanup = makeNode(
            ProjectOopHirNodeKind::OwnedParameterCleanup,
            symbol.definitionSyntax,
            symbol.parameters[parameterOrdinal].typeKey);
        cleanup->parameterOrdinal = parameterOrdinal;
        cleanup->cleanupRegionOrdinal = cleanupOrdinal;
        cleanup->automaticDestruction =
            region.kind == MethodCleanupRegionKind::ValueParameterDrop;
        cleanupNodes.push_back(std::move(cleanup));
    }
    return true;
}

bool ProjectOopHirModel::buildNamespaceFunction(
    std::size_t functionOrdinal,
    std::string& error)
{
    const NamespaceFunctionSymbol& symbol =
        namespaceFunctionModel_->functions()[functionOrdinal];
    ProjectOopHirFunction function;
    initializeNamespaceFunction(function, functionOrdinal, symbol);

    if (symbol.definitionSyntax == nullptr)
    {
        if (symbol.isPrecompiled && !symbol.isExternC)
        {
            function.isImportedNative = true;
            functionByNamespaceFunction_[functionOrdinal] = functions_.size();
            functions_.push_back(std::move(function));
            return true;
        }
        if (!symbol.isExternC)
        {
            // A prototype that is never selected emits no code. A selected call or
            // delegate conversion was already rejected by method-body binding.
            return true;
        }
        function.isImportedC = true;
        function.cAbiName = symbol.name;
        function.mangledName = symbol.name;
        functionByNamespaceFunction_[functionOrdinal] = functions_.size();
        functions_.push_back(std::move(function));
        return true;
    }
    if (!methodBodyModel_->namespaceFunctionBodyAnalyzed(functionOrdinal))
    {
        error = "namespace function '" + symbol.signatureKey +
            "' reached HIR without analyzed body bindings";
        return false;
    }
    if (symbol.definitionSyntax->first() == nullptr)
    {
        error = "namespace function '" + symbol.signatureKey + "' has no body syntax";
        return false;
    }

    const std::size_t callableOrdinal = memberModel_->members().size() + functionOrdinal;
    for (std::size_t localOrdinal = 0; localOrdinal < methodBodyModel_->locals().size();
         ++localOrdinal)
    {
        const MethodLocalSymbol& local = methodBodyModel_->locals()[localOrdinal];
        if (local.methodMemberOrdinal == callableOrdinal && !local.lambdaOrdinal.has_value())
            function.localOrdinals.push_back(localOrdinal);
    }
    std::vector<std::unique_ptr<ProjectOopHirNode>> cleanupNodes;
    if (!appendNamespaceFunctionParameterCleanups(
            function, callableOrdinal, symbol, cleanupNodes, error))
        return false;
    loweringAsyncBody_ = function.effects.contains(sema::Effect::Async);
    function.body = lowerBlock(symbol.definitionSyntax->first(), callableOrdinal, error);
    activeHirCleanupRegions_.clear();
    if (function.body == nullptr) return false;
    if (!cleanupNodes.empty())
    {
        function.body->children.insert(
            function.body->children.begin(),
            std::make_move_iterator(cleanupNodes.begin()),
            std::make_move_iterator(cleanupNodes.end()));
    }
    std::unique_ptr<ProjectOopHirFunction> poll;
    if (function.effects.contains(sema::Effect::Async))
    {
        poll = splitAsyncFunction(function, error);
        if (poll == nullptr) return false;
    }
    functionByNamespaceFunction_[functionOrdinal] = functions_.size();
    functions_.push_back(std::move(function));
    if (poll) functions_.push_back(std::move(*poll));
    return true;
}

bool ProjectOopHirModel::buildNamespaceFunctions(std::string& error)
{
    if (namespaceFunctionModel_ == nullptr) return true;
    for (std::size_t functionOrdinal = 0;
         functionOrdinal < namespaceFunctionModel_->functions().size();
         ++functionOrdinal)
    {
        if (!buildNamespaceFunction(functionOrdinal, error)) return false;
    }
    return true;
}

bool ProjectOopHirModel::lowerExpressionLambdaBody(
    ProjectOopHirFunction& function,
    const MethodLambdaBinding& lambda,
    std::string& error)
{
    auto block = makeNode(ProjectOopHirNodeKind::Block, lambda.syntax, "builtin:void");
    auto value = lowerExpression(lambda.syntax->first(), lambda.ownerMemberOrdinal, false, error);
    if (value != nullptr)
    {
        const MethodExpressionTypeBinding* sourceType = expressionType(lambda.syntax->first());
        if (lambda.returnTypeKey == "builtin:void")
        {
            auto statement = makeNode(
                ProjectOopHirNodeKind::ExpressionStatement,
                lambda.syntax->first(),
                "builtin:void");
            statement->children.push_back(std::move(value));
            block->children.push_back(std::move(statement));
        }
        else
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                value->typeKey,
                lambda.returnTypeKey,
                sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
            if (!conversion.exists())
            {
                error = "Stage 43 lambda body lost its validated return conversion";
            }
            else
            {
                if (conversion.kind != ProjectConversionKind::Identity)
                    value = wrapImplicitConversion(
                        std::move(value), conversion, lambda.syntax->first());
                auto statement = makeNode(
                    ProjectOopHirNodeKind::Return,
                    lambda.syntax->first(),
                    "builtin:void");
                statement->children.push_back(std::move(value));
                block->children.push_back(std::move(statement));
            }
        }
    }
    function.body = std::move(block);
    return error.empty();
}

bool ProjectOopHirModel::buildLambdaFunction(
    const MethodLambdaBinding& lambda,
    std::string& error)
{
    if (lambda.ownerMemberOrdinal >= memberModel_->members().size())
    {
        error = "Stage 43 lambda owner member ordinal is invalid";
        return false;
    }
    const ClassMemberSymbol& owner = memberModel_->members()[lambda.ownerMemberOrdinal];
    ProjectOopHirFunction function;
    function.memberOrdinal = lambda.ownerMemberOrdinal;
    function.declaringTypeOrdinal = owner.declaringTypeOrdinal;
    function.isLambda = true;
    function.lambdaOrdinal = lambda.lambdaOrdinal;
    function.hasClosureEnvironment = !lambda.captures.empty();
    function.callableTrait = lambda.callableTrait;
    function.closureLayoutSize = lambda.closureLayout.size;
    function.closureLayoutAlignment = lambda.closureLayout.alignment;
    function.isStatic = !function.hasClosureEnvironment;
    function.mangledName = lambda.generatedName;
    function.returnTypeKey = lambda.returnTypeKey;
    function.errorTypeKey = lambda.errorTypeKey;
    if (!lambda.errorTypeKey.empty())
        function.effects = function.effects.with(sema::Effect::Throws);
    function.parameterNames = lambda.parameterNames;
    function.parameterTypeKeys = lambda.parameterTypeKeys;
    function.parameterPassKinds.assign(
        lambda.parameterTypeKeys.size(), ParameterPassKind::Value);
    for (const MethodLambdaCapture& capture : lambda.captures)
    {
        function.captureTypeKeys.push_back(capture.typeKey);
        function.captureNames.push_back(capture.name);
        function.captureSourceKinds.push_back(capture.sourceKind);
        function.captureModes.push_back(capture.mode);
        function.captureOwnsValues.push_back(capture.ownsCapturedValue);
    }
    for (std::size_t localOrdinal = 0; localOrdinal < methodBodyModel_->locals().size();
         ++localOrdinal)
    {
        const MethodLocalSymbol& local = methodBodyModel_->locals()[localOrdinal];
        if (local.lambdaOrdinal == lambda.lambdaOrdinal &&
            !local.lambdaParameterOrdinal.has_value())
            function.localOrdinals.push_back(localOrdinal);
    }
    currentLambda_ = &lambda;
    if (lambda.blockBody)
        function.body = lowerBlock(lambda.syntax->first(), lambda.ownerMemberOrdinal, error);
    else
        lowerExpressionLambdaBody(function, lambda, error);
    currentLambda_ = nullptr;
    if (function.body == nullptr || !error.empty()) return false;
    functions_.push_back(std::move(function));
    return true;
}

bool ProjectOopHirModel::buildLambdaFunctions(std::string& error)
{
    // Stage 43 lowers every target-typed lambda as a synthetic native callable.  Capturing
    // lambdas use the Stage 42 receiver slot as a closure-environment pointer; non-capturing
    // lambdas remain ordinary static callables with a null delegate receiver.
    for (const MethodLambdaBinding& lambda : methodBodyModel_->lambdaBindings())
    {
        if (!buildLambdaFunction(lambda, error)) return false;
    }
    return true;
}

bool ProjectOopHirModel::markLambdaClosureCells(std::string& error)
{
    // E11 closure cells belong to the callable that creates the lambda.  Mark the creator's
    // locals/parameters here, after regular and synthetic functions both exist, so LLVM can
    // heap-home those variables once and every delegate can retain the same cell.
    for (const MethodLambdaBinding& lambda : methodBodyModel_->lambdaBindings())
    {
        if (lambda.captures.empty()) continue;
        ProjectOopHirFunction* creator = nullptr;
        if (lambda.parentLambdaOrdinal.has_value())
        {
            for (ProjectOopHirFunction& candidate : functions_)
            {
                if (candidate.isLambda && candidate.lambdaOrdinal == lambda.parentLambdaOrdinal)
                {
                    creator = &candidate;
                    break;
                }
            }
        }
        else if (lambda.ownerMemberOrdinal < functionByMember_.size() &&
                 functionByMember_[lambda.ownerMemberOrdinal].has_value())
        {
            creator = &functions_[*functionByMember_[lambda.ownerMemberOrdinal]];
        }
        if (creator == nullptr)
        {
            error = "E11 lambda creator function is unavailable";
            return false;
        }
        for (const MethodLambdaCapture& capture : lambda.captures)
        {
            switch (capture.sourceKind)
            {
                case MethodLambdaCaptureSourceKind::OriginalLocal:
                case MethodLambdaCaptureSourceKind::ParentLocal:
                    if (capture.sourceOrdinal.has_value())
                        appendUniqueOrdinal(
                            creator->capturedLocalOrdinals, *capture.sourceOrdinal);
                    break;
                case MethodLambdaCaptureSourceKind::OriginalParameter:
                case MethodLambdaCaptureSourceKind::ParentParameter:
                    if (capture.sourceOrdinal.has_value())
                        appendUniqueOrdinal(
                            creator->capturedParameterOrdinals, *capture.sourceOrdinal);
                    break;
                case MethodLambdaCaptureSourceKind::OriginalThis:
                    creator->capturesThisCell = true;
                    break;
                case MethodLambdaCaptureSourceKind::ParentCapture:
                    break;
            }
        }
    }
    return true;
}

bool ProjectOopHirModel::buildFieldInitializer(
    std::size_t memberOrdinal,
    std::string& error)
{
    const ClassMemberSymbol& field = memberModel_->members()[memberOrdinal];
    if (field.kind != ClassMemberKind::Field) return true;
    // Stage 51.x: an enum constant has no real initializer expression to lower for the
    // (common) auto-increment case -- its value was already computed by
    // ProjectMemberModel::collectEnumMembers. Synthesize the same IntegerConstant HIR node
    // `lowerExpression` would have produced for a literal, so every downstream consumer
    // (Stage 37 const-field materialization in the MIR builder, then LLVM constant-global
    // lowering) sees an ordinary `static const int` field and needs no enum-specific code.
    if (field.isEnumConstant)
    {
        // EL LITERAL LLEVA EL TIPO SUBYACENTE DEL ENUM, no `builtin:int`.
        //
        // Aguas abajo, `normalizeInteger` trunca al ancho de esta clave y extiende el signo. Con
        // `builtin:int` fijo, TODO enumerador se truncaba a 32 bits antes de llegar al IR: en un
        // `enum : ulong`, el valor 4294967295 salia valiendo -1, y el modelo de miembros --que
        // ya lo habia aceptado como valido-- no tenia forma de enterarse. La regla esta en
        // EnumUnderlyingType.h.
        const frontend::NamedTypeDeclaration* enumType =
            field.declaringTypeOrdinal < nameLookup_->namedTypes().size()
                ? &nameLookup_->namedTypes()[field.declaringTypeOrdinal]
                : nullptr;
        const std::string_view underlyingKey = enumType != nullptr
            ? enumUnderlyingTypeKey(enumType->syntax)
            : std::string_view("builtin:int");
        auto literal = makeNode(
            ProjectOopHirNodeKind::IntegerConstant, field.syntax, std::string(underlyingKey));
        literal->integerValue = static_cast<std::uint64_t>(field.enumConstantValue);
        fieldInitializers_.push_back(ProjectOopHirFieldInitializer{
            memberOrdinal,
            field.declaringTypeOrdinal,
            field.isStatic,
            field.isConst,
            std::move(literal)});
        return true;
    }
    if (field.syntax == nullptr || field.syntax->first() == nullptr) return true;
    auto value = lowerExpression(field.syntax->first(), memberOrdinal, false, error);
    if (value == nullptr) return false;
    const MethodExpressionTypeBinding* sourceType = expressionType(field.syntax->first());
    const ProjectImplicitConversion conversion = classifyImplicit(
        value->typeKey,
        field.declaredTypeKey,
        sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
        value = wrapImplicitConversion(std::move(value), conversion, field.syntax->first());
    fieldInitializers_.push_back(ProjectOopHirFieldInitializer{
        memberOrdinal,
        field.declaringTypeOrdinal,
        field.isStatic,
        field.isConst,
        std::move(value)});
    return true;
}

bool ProjectOopHirModel::buildFieldInitializers(std::string& error)
{
    // Keep a canonical lowered form for every explicit field initializer.  Static initializers are
    // consumed by the Stage 37 native type-initialization routine; instance initializers are also
    // useful to dumps/tests even though constructor lowering injects per-construction copies.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size();
         ++memberOrdinal)
    {
        if (!buildFieldInitializer(memberOrdinal, error)) return false;
    }
    return true;
}

bool ProjectOopHirModel::buildImplicitConstructor(
    std::size_t typeOrdinal,
    std::string& error)
{
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[typeOrdinal];
    if (type.kind != NamedTypeDeclarationKind::Class || type.isPrecompiled || type.isStatic)
        return true;
    const bool hasExplicitConstructor = std::ranges::any_of(
        memberModel_->membersOfType(typeOrdinal), [&](const std::size_t ordinal) {
            return ordinal < memberModel_->members().size() &&
                memberModel_->members()[ordinal].kind == ClassMemberKind::Constructor;
        });
    if (hasExplicitConstructor) return true;

    const ImplicitDefaultConstructorBinding* binding =
        constructionModel_->implicitDefaultConstructorBindingForType(typeOrdinal);
    if (binding == nullptr ||
        binding->resolution == ImplicitDefaultConstructorResolutionKind::Invalid ||
        binding->resolution ==
            ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor ||
        binding->resolution ==
            ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor)
        return true;

    ProjectOopHirFunction function;
    function.memberOrdinal = std::numeric_limits<std::size_t>::max();
    function.declaringTypeOrdinal = typeOrdinal;
    function.constructor = true;
    function.isSyntheticImplicitConstructor = true;
    const std::string fullyQualifiedName = type.identity.fullyQualifiedName();
    const std::size_t separator = fullyQualifiedName.find_last_of(".+");
    const std::string typeName = separator == std::string::npos
        ? fullyQualifiedName
        : fullyQualifiedName.substr(separator + 1U);
    function.mangledName = "__nsc_ctor_" +
        compilation::symbolFragmentForIdentityKey(type.identity.key()) + "__" +
        sanitize(typeName);
    function.returnTypeKey = "builtin:void";
    function.body = makeNode(ProjectOopHirNodeKind::Block, type.syntax, "builtin:void");
    if (binding->entryConstructorMemberOrdinal.has_value() &&
        binding->baseTypeOrdinal.has_value())
    {
        auto baseCall = makeNode(
            ProjectOopHirNodeKind::ConstructorInitializerCall, type.syntax, "builtin:void");
        baseCall->memberOrdinal = binding->entryConstructorMemberOrdinal;
        baseCall->baseReceiver = true;
        baseCall->children.push_back(makeNode(
            ProjectOopHirNodeKind::ThisValue,
            type.syntax,
            inheritanceModel_->typeKeyForOrdinal(*binding->baseTypeOrdinal)));
        function.body->children.push_back(std::move(baseCall));
    }
    if (binding->baseTypeOrdinal.has_value())
    {
        for (const std::size_t initializerType : implicitInitializerTypes(
                 *binding->baseTypeOrdinal, binding->entryConstructorMemberOrdinal))
        {
            appendInstanceFieldInitializers(initializerType, function.body->children, error);
            if (!error.empty()) return false;
        }
    }
    appendInstanceFieldInitializers(typeOrdinal, function.body->children, error);
    if (!error.empty()) return false;
    functions_.push_back(std::move(function));
    return true;
}

bool ProjectOopHirModel::buildImplicitConstructors(std::string& error)
{
    // A source class with no written constructor still owns executable construction semantics:
    // base chaining, zero initialization (performed by the allocator path), and its instance
    // field initializers. Publish those semantics through the same void(this) ABI as a written
    // constructor so a metadata consumer never has to recreate producer source expressions.
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup_->namedTypes().size();
         ++typeOrdinal)
    {
        if (!buildImplicitConstructor(typeOrdinal, error)) return false;
    }
    return true;
}

bool ProjectOopHirModel::buildImpl(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::string& error,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::size_t workerCount,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel,
    const ProjectMetaprogrammingModel* metaprogrammingModel,
    std::optional<std::size_t> memberFilter,
    const ProjectOopHirModel* lookupSource)
{
    error.clear();
    genericConstraintModel_ = genericConstraintModel;
    lookupSource_ = lookupSource;
    metaprogrammingModel_ = metaprogrammingModel;
    if (!initializeBuildState(
            nameLookup,
            memberModel,
            inheritanceModel,
            interfaceModel,
            methodBodyModel,
            constructionModel,
            objectLayoutModel,
            namespaceFunctionModel,
            error))
        return false;
    indexBuildBindings(methodBodyModel, constructionModel);
    functionByMember_.resize(memberModel.members().size());
    if (namespaceFunctionModel_ != nullptr)
        functionByNamespaceFunction_.resize(namespaceFunctionModel_->functions().size());
    const bool regularFunctionsBuiltInParallel = workerCount > 1 && !memberFilter.has_value();
    if (regularFunctionsBuiltInParallel &&
        !buildRegularFunctionsInParallel(workerCount, error))
        return false;

    if (!regularFunctionsBuiltInParallel &&
        !buildRegularFunctionsSequentially(memberFilter, error))
        return false;
    if (!memberFilter.has_value() && !buildNamespaceFunctions(error)) return false;
    if (memberFilter.has_value())
    {
        built_ = true;
        return true;
    }
    if (!buildLambdaFunctions(error) || !markLambdaClosureCells(error)) return false;

    if (!buildFieldInitializers(error)) return false;

    if (!buildImplicitConstructors(error)) return false;

    built_ = true;
    return true;
}

// C8 "type root", struct half: the SHORT name a struct's synthesized ToString() answers --
// `Vector3`, not `Proj::game.math.Vector3`. Same rule the class descriptor's runtime name uses,
// so the two spellings of "what is this type called" cannot drift apart.
std::string ProjectOopHirModel::shortTypeName(std::string_view typeKey) const
{
    std::string_view identity = typeKey;
    if (identity.starts_with("type:")) identity = identity.substr(5);
    const std::size_t lastSeparator = identity.find_last_of(".:");
    return std::string(
        lastSeparator == std::string_view::npos ? identity : identity.substr(lastSeparator + 1));
}

bool ProjectOopHirModel::isStructValueType(std::string_view typeKey) const noexcept
{
    if (ProjectTypeConversion::isTuple(typeKey)) return true;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() == key) return type.kind == NamedTypeDeclarationKind::Struct;
    }
    return false;
}

bool ProjectOopHirModel::isClassReferenceType(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return false;
    }
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() == identity)
        {
            return type.kind == NamedTypeDeclarationKind::Class;
        }
    }
    return false;
}

const ClassMemberSymbol* ProjectOopHirModel::callableSymbol(std::size_t ordinal) const noexcept
{
    if (methodBodyModel_ != nullptr) return methodBodyModel_->callableSymbol(ordinal);
    if (memberModel_ == nullptr) return nullptr;
    if (ordinal < memberModel_->members().size())
        return &memberModel_->members()[ordinal];
    return nullptr;
}

bool ProjectOopHirModel::isReferenceType(std::string_view typeKey) const noexcept
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Reference;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return false;
    }
    const std::string_view identity = typeKey.substr(5);
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.identity.key() == identity)
        {
            return type.kind == NamedTypeDeclarationKind::Class ||
                type.kind == NamedTypeDeclarationKind::Interface ||
                type.kind == NamedTypeDeclarationKind::Delegate;
        }
    }
    return false;
}
}
