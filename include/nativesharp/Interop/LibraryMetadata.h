#pragma once

#include "nativesharp/Runtime/RuntimeAbiContract.h"

#include "nativesharp/Interop/LibraryMetadataAssociatedItem.h"
#include "nativesharp/Interop/LibraryMetadataAssociatedItemKind.h"
#include "nativesharp/Interop/LibraryMetadataBinaryDependency.h"
#include "nativesharp/Interop/LibraryMetadataDefaultValueKind.h"
#include "nativesharp/Interop/LibraryMetadataDispatch.h"
#include "nativesharp/Interop/LibraryMetadataDispatchInterface.h"
#include "nativesharp/Interop/LibraryMetadataField.h"
#include "nativesharp/Interop/LibraryMetadataEscapeClass.h"
#include "nativesharp/Interop/LibraryMetadataGenericTemplate.h"
#include "nativesharp/Interop/LibraryMetadataGenericSpecialization.h"
#include "nativesharp/Interop/LibraryMetadataMember.h"
#include "nativesharp/Interop/LibraryMetadataNamespaceFunction.h"
#include "nativesharp/Interop/LibraryMetadataNamespaceState.h"
#include "nativesharp/Interop/LibraryMetadataNamespaceStateInitializerKind.h"
#include "nativesharp/Interop/LibraryMetadataOwnershipKind.h"
#include "nativesharp/Interop/LibraryMetadataParameter.h"
#include "nativesharp/Interop/LibraryMetadataParameterPassKind.h"
#include "nativesharp/Interop/LibraryMetadataPlatformImport.h"
#include "nativesharp/Interop/LibraryMetadataExport.h"
#include "nativesharp/Interop/LibraryMetadataTraitImpl.h"
#include "nativesharp/Interop/LibraryMetadataType.h"
#include "nativesharp/Interop/LibraryMetadataTypeKind.h"
#include "nativesharp/Interop/LibraryMetadataVariantCase.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::interop
{
// Library metadata describes the public surface and the physical contracts required by consumers.
//
// trait and implementing type records preserve associated type/constant
// requirements, defaults, definitions, and normalized integral values across LibraryReference.
// interface-kind type records can identify NativeSharp traits so a
// consumer reconstructs default methods and static property/operator requirements faithfully.
// tagged variants preserve nominal identity plus their ordered case and
// payload catalogue. Case order is observable through CaseTag(), so reconstructing it from the
// alphabetically sorted public member table would be an ABI defect.
// struct type flags preserve `record` and its four explicit derives so a
// LibraryReference consumer reconstructs the same NativeSharp synthesized-member surface.
// static-library metadata carries its binary LibraryReference closure and
// private platform imports so a consumer can link the archive without repeating implementation
// dependencies or losing DLL symbols.
// el consumidor reconstruye la superficie desde los contratos publicados,
// sin seleccionar una edición de lenguaje.
// las declaraciones C variadicas conservan el marcador `...` para que
// LibraryReference reconstruya la misma firma importada.
// los bitfields conservan unidad, offset de bit y ancho para reproducir
// el layout MSVC en headers y LibraryReference.
// los campos portadores de agregados anonimos conservan su marca para
// que una LibraryReference reconstruya la promocion de miembros sin aplanar el layout fisico.
// metodos y constructores publicos conservan `noheap`.
// los metodos publicos con retorno ref conservan su categoria de resultado
// y su portador ABI de direccion al reconstruirse desde una LibraryReference.
// los campos publicos volatile conservan el contrato de acceso observable
// para que un consumidor LibraryReference no los degrade a accesos ordinarios.
// los metodos inline ordinarios conservan politica y cuerpo fuente para
// que un consumidor precompilado pueda analizarlos, emitirlos e inlinearlos localmente.
// delegateFlags bit 1 conserva `extern vectorcall delegate`.
// el modulo enumera identidades nominales propiedad de otras
// LibraryReference para validar la clausura completa antes de reconstruir su API.
// las plantillas reutilizables conservan su grafo de dependencias y la
// clausura privada transitiva necesaria para materializarlas sin ampliar la API publica.
// una declaracion publica `extern C from` conserva la DLL que posee el
// simbolo para que el consumidor reconstruya el import real sin atribuirselo al productor.
// las constantes string conservan sus bytes para que el consumidor pueda
// materializar su valor en vez de perder el miembro publico.
// The file carries the ABI contract and the representation of ordinary delegates.
// cada plantilla generica publica conserva su implementacion fuente
// canonica para que otro modulo pueda monomorfizarla con el frontend del consumidor.
// cada clase publicable conserva el accessor funcional de su descriptor.
// un delegate conserva su firma y si es extern C.
// cada parametro conserva Value/ref/out/in.
// cada parametro conserva su valor predeterminado literal.
// el registro de tipo lleva `enumUnderlyingTypeKey`.
// los campos, miembros y parametros llevan `ownership`.
// public namespace functions preserve signature and physical identity.
// public namespace state preserves shared storage and initializers.
// identity payloads reserve one historical source-revision byte.
// parameters preserve the params ReadOnlySpan<T> call-site contract.
// public borrowed types and callable signatures preserve erased lifetime
// parameters, parameter/result bindings, and normalized outlives edges across LibraryReference.
// value types preserve their explicit Copy capability across
// LibraryReference boundaries; an absent bit keeps the move-only default.
// public extension methods preserve their receiver contract and extension
// lookup identity across LibraryReference boundaries.
// traits publish the descriptor-backed one-word dyn-object ABI and public
// callable signatures retain the explicit `dyn` marker across LibraryReference boundaries.
// declaration-scoped opaque result identities publish their one-field
// in-place representation and implemented trait as a closed native contract.
// named lifetime binders and outlives edges cross module boundaries.
// per-parameter interprocedural escape summaries are public contracts.
// value types that own storage publish producer-owned drop-glue thunks.
// delegates, members and namespace functions publish their typed-throws
// error key. Empty remains the canonical non-throwing contract.
// reusable generic templates publish parameter kinds, constant types,
// move/copy/drop capability constraints, nominal/equality constraints, and erased lifetime
// binders/outlives edges as an explicit semantic catalogue in addition to materializable source.
// extern-C parameters publish buffer-length and callback-lifetime
// contracts; members publish the explicit unwind-boundary policy.
// authored lexical using directives for reusable NativeSharp templates.
// possible same-module nominal receiver types used by template expressions.
// producer-proven root classes with no instance cleanup.

// These bits describe producer language capabilities affecting private lowering and debug behavior
// without changing the public NativeSharp ABI projection.
inline constexpr std::uint32_t kMetadataLanguageStaticConstructors = 1U << 0U;
inline constexpr std::uint32_t kMetadataLanguageStaticLocals = 1U << 1U;
inline constexpr std::uint32_t kMetadataLanguageConditionDeclarations = 1U << 2U;
inline constexpr std::uint32_t kMetadataLanguageFeatureMask =
    kMetadataLanguageStaticConstructors |
    kMetadataLanguageStaticLocals |
    kMetadataLanguageConditionDeclarations;

// NSMETA. These bits belong to LibraryMetadataType::flags. A record is still physically a
// struct; the flags publish only its compile-time member-generation policy. They therefore do not
// change layout or the native ABI, but they must survive source materialization or a consumer would
// silently gain the legacy struct members and lose the record's explicit opt-ins.
inline constexpr std::uint16_t kMetadataTypeRecord = 1U << 7U;
inline constexpr std::uint16_t kMetadataTypeDeriveEquality = 1U << 8U;
inline constexpr std::uint16_t kMetadataTypeDeriveHash = 1U << 9U;
inline constexpr std::uint16_t kMetadataTypeDerivePrint = 1U << 10U;
inline constexpr std::uint16_t kMetadataTypeDeriveClone = 1U << 11U;
// NSMETA. A variant is physically a struct, but its ordered closed case domain is part of the
// public language and ABI contract. The payload catalogue below carries the rest of that identity.
inline constexpr std::uint16_t kMetadataTypeVariant = 1U << 12U;
// NSMETA. Traits intentionally share the interface ABI; this bit preserves their source and
// semantic contract without inventing a sixth physical type kind.
inline constexpr std::uint16_t kMetadataTypeTrait = 1U << 13U;
// NSMETA. An opaque result is physically a one-field struct. This bit distinguishes that
// compiler-owned nominal identity from a user struct so source/API/ABI tools cannot erase the
// `some Trait` abstraction while preserving the same bytes accidentally.
inline constexpr std::uint16_t kMetadataTypeOpaqueResult = 1U << 14U;
// NSMETA. This is a semantic capability, not a layout promise: validation still requires
// every instance field to be Copy before the producer may publish it.
inline constexpr std::uint16_t kMetadataTypeCopy = 1U << 15U;
inline constexpr std::uint16_t kMetadataTypeRecordDeriveMask =
    kMetadataTypeDeriveEquality | kMetadataTypeDeriveHash |
    kMetadataTypeDerivePrint | kMetadataTypeDeriveClone;

// Bits 21-22 of LibraryMetadataMember::flags carry the four-valued inlining contract. Keeping
// this layout public prevents the producer, source materializer, and host tooling from growing
// separate magic shifts for the same metadata field.
inline constexpr std::uint32_t kMetadataMemberInliningShift = 21U;
inline constexpr std::uint32_t kMetadataMemberInliningMask = 3U << kMetadataMemberInliningShift;
// Item 308. Bit 23 is preserved in both member and layout-field flags so public volatile storage
// reconstructs with the same observable-access contract in a LibraryReference consumer.
inline constexpr std::uint32_t kMetadataMemberVolatile = 1U << 23U;
// Item 313. Bit 24 belongs only to callable member records. The semantic type key stays T while
// this bit says that the NativeSharp ABI physically returns a T* alias.
inline constexpr std::uint32_t kMetadataMemberReferenceReturn = 1U << 24U;
// Item 322. Bit 25 is a callable effect contract: callers in a noheap context may dispatch only
// to methods and constructors that preserve this bit across module boundaries.
inline constexpr std::uint32_t kMetadataMemberNoHeap = 1U << 25U;
// Item 326. Bit 26 belongs to layout-field records. It distinguishes the hidden physical field
// that carries an unnamed struct/union from an ordinary compiler-generated field, allowing the
// metadata source materializer to recreate promotion while retaining the producer's exact layout.
inline constexpr std::uint32_t kMetadataFieldAnonymousCarrier = 1U << 26U;
// On operator method records, bit 26 certifies the producer-checked non-escape
// of every `in` operand. Field records retain their separate meaning above.
inline constexpr std::uint32_t kMetadataOperatorInOperandsNoEscape = 1U << 26U;
// These bits belong to layout-field records and preserve exact bit coordinates. The flags
// distinguish a named bit-field from an unnamed padding declaration.
inline constexpr std::uint32_t kMetadataFieldBitField = 1U << 27U;
// Method records certify a non-escaping receiver without tracked reference-field effects.
inline constexpr std::uint32_t kMetadataMemberReceiverNoReferenceFieldEffects = 1U << 27U;
inline constexpr std::uint32_t kMetadataFieldUnnamedBitField = 1U << 28U;
// Item 331. Bit 29 belongs only to imported-C method records and preserves the terminal `...`.
inline constexpr std::uint32_t kMetadataMemberVariadic = 1U << 29U;
// NSMETA. The canonical key intentionally remains the trait/interface key because dyn uses
// that ABI. This semantic bit preserves the explicit dynamic-dispatch spelling for consumers.
inline constexpr std::uint32_t kMetadataMemberDynamicTrait = 1U << 30U;
// EN: Freeze P3. Bit 31 accompanies bit 24 on a callable declared `ref readonly T`: the alias it
//     returns may be read but not written. Consumers rebuild the `ref readonly` spelling.
// ES: Congelamiento P3. El bit 31 acompana al bit 24 en un callable declarado `ref readonly T`:
//     el alias que devuelve se puede leer pero no escribir. Los consumidores reconstruyen la forma
//     `ref readonly`.
inline constexpr std::uint32_t kMetadataMemberReadonlyReferenceReturn = 1U << 31U;

// NSMETA. LibraryMetadataNamespaceFunction::flags bit 5 marks the declaration as an
// extension method. Its first parameter is the implicit receiver and retains Value/ref/in in the
// normal parameter record; consumers must reconstruct an extension block rather than publishing
// the synthetic receiver as an ordinary free-function parameter.
inline constexpr std::uint8_t kMetadataNamespaceFunctionExtension = 1U << 5U;
// NS304-T08-F07 (NSMETA addendum): bit 6 marks a STATIC extension (`nsc-extension-static`),
// which has NO implicit receiver. Consumers reconstruct `extension T { static ... }` instead of
// treating the first parameter as the receiver.
inline constexpr std::uint8_t kMetadataNamespaceFunctionStaticExtension = 1U << 6U;

inline constexpr std::uint8_t kMetadataTraitObjectObjectSafe = 1U << 0U;
inline constexpr std::uint8_t kMetadataTraitObjectDescriptorMap = 1U << 1U;
inline constexpr std::uint8_t kMetadataTraitObjectNullIsZero = 1U << 2U;
inline constexpr std::uint8_t kMetadataTraitObjectNoConversionAllocation = 1U << 3U;

// Contrato binario de NativeSharp 1.0. Son numeros, no nombres decorativos: entran en el
// fingerprint ABI, se verifican antes de compilar al consumidor y sólo cambian cuando cambia una
// decisión física. NSMETA puede evolucionar de forma aditiva manteniendo estos valores.
inline constexpr std::uint16_t kNativeAbiMajor = 1;
inline constexpr std::uint16_t kNativeAbiMinor = 0;
inline constexpr std::uint16_t kRuntimeAbiMajor = runtime::kRuntimeAbiMajor;
inline constexpr std::uint16_t kRuntimeAbiMinor = runtime::kRuntimeAbiMinor;
inline constexpr std::uint16_t kManglingAbiVersion = 1;
// Item 319 appends the object-only destruction entry to every class descriptor.
// Existing field offsets remain stable, but a producer without field 7 cannot satisfy `destroy`.
// EN: Freeze P8 raises it to 3. An array whose element needs a value drop carries a liveness map
//     after its data (DEC-11), and an array-backed Span/ReadOnlySpan carries its source array and
//     offset (P4, 16 -> 32 bytes). A producer with object ABI 2 passes or receives the old layout.
// ES: Congelamiento P8 lo sube a 3. Un arreglo cuyo elemento requiere drop lleva un mapa de vida
//     tras sus datos (DEC-11), y un Span/ReadOnlySpan sobre un arreglo lleva el arreglo y su
//     desplazamiento (P4, de 16 a 32 bytes). Un productor con object ABI 2 usa el layout anterior.
inline constexpr std::uint16_t kObjectAbiVersion = 3;
inline constexpr std::uint16_t kDelegateAbiVersion = 1;
inline constexpr std::uint16_t kExceptionAbiVersion = 1;
inline constexpr std::uint32_t kManagedDelegateCellSize = 32;
inline constexpr std::uint16_t kMetadataGenericRequiresClass = 1U << 0U;
inline constexpr std::uint16_t kMetadataGenericRequiresValueType = 1U << 1U;
inline constexpr std::uint16_t kMetadataGenericRequiresDefaultConstructor = 1U << 2U;
inline constexpr std::uint16_t kMetadataGenericRequiresCopy = 1U << 3U;
inline constexpr std::uint16_t kMetadataGenericRequiresMove = 1U << 4U;
inline constexpr std::uint16_t kMetadataGenericRequiresDrop = 1U << 5U;
inline constexpr std::uint16_t kMetadataGenericRequiresUnmanaged = 1U << 6U;
inline constexpr std::uint16_t kMetadataGenericRequiresNotNull = 1U << 7U;
inline constexpr std::uint16_t kMetadataGenericConstraintMask =
    kMetadataGenericRequiresClass | kMetadataGenericRequiresValueType |
    kMetadataGenericRequiresDefaultConstructor | kMetadataGenericRequiresCopy |
    kMetadataGenericRequiresMove | kMetadataGenericRequiresDrop |
    kMetadataGenericRequiresUnmanaged | kMetadataGenericRequiresNotNull;

struct LibraryMetadata final
{
    // Transient provenance set after the reader has decoded and authenticated the payload.
    bool payloadDecodedByCurrentReader = false;
    std::uint32_t targetId = 1;
    std::uint32_t pointerSize = 8;
    std::uint32_t pointerAlignment = 8;
    std::uint32_t languageFeatureFlags = 0;
    std::string producerVersion;
    std::string moduleName;
    std::string targetTriple;
    // Physical ABI contract carried by the library metadata.
    std::uint16_t nativeAbiMajor = kNativeAbiMajor;
    std::uint16_t nativeAbiMinor = kNativeAbiMinor;
    std::uint16_t runtimeAbiMajor = kRuntimeAbiMajor;
    std::uint16_t runtimeAbiMinor = kRuntimeAbiMinor;
    std::uint16_t manglingAbiVersion = kManglingAbiVersion;
    std::uint16_t objectAbiVersion = kObjectAbiVersion;
    std::uint16_t delegateAbiVersion = kDelegateAbiVersion;
    std::uint16_t exceptionAbiVersion = kExceptionAbiVersion;
    std::uint32_t managedDelegateCellSize = kManagedDelegateCellSize;
    std::array<std::uint8_t, 32> abiFingerprint{};
    std::vector<LibraryMetadataType> types;
    std::vector<LibraryMetadataMember> members;
    std::vector<LibraryMetadataTraitImpl> traitImpls;
    std::vector<LibraryMetadataNamespaceFunction> namespaceFunctions;
    std::vector<LibraryMetadataNamespaceState> namespaceStates;
    std::vector<LibraryMetadataGenericTemplate> genericTemplates;
    std::vector<LibraryMetadataGenericSpecialization> genericSpecializations;
    // NSMETA: exact nominal types used by this module but owned by another
    // LibraryReference.  A consumer validates these keys against the complete
    // metadata set before parsing any reconstructed API.  Closed generic types
    // already materialized by their provider belong here too; they must not be
    // republished as if this module owned their specialization.
    std::vector<std::string> externalTypeKeys;
    // NSMETA: namespaces actually declared in the producer's own source.
    // Empty namespace shells preserve valid imports without exposing internal types.
    std::vector<std::string> sourceNamespaces;
    // Complete producer evidence for root classes without instance fields or destructors.
    std::vector<std::string> trivialCleanupRootTypeKeys;
    // Absence means unknown; it never grants a receiver non-escape guarantee.
    std::vector<std::string> nonEscapingLifecycleTypeKeys;
    std::vector<LibraryMetadataDispatch> dispatchTables;
    std::vector<LibraryMetadataExport> exports;
    // NSMETA. These are implementation closure edges of a static archive, not public API.
    // Artifact names are leaf names resolved beside the referring metadata or under the explicit
    // LibraryReferenceRoot; the ABI fingerprint prevents a same-name incompatible substitution.
    std::vector<LibraryMetadataBinaryDependency> binaryDependencies;
    // Private `extern C from` callees referenced by this archive. The final consumer materializes
    // their import libraries even though the declarations themselves are not public metadata.
    std::vector<LibraryMetadataPlatformImport> platformImports;
};

// NS304-T16-F01: the consolidated metadata invariants — reserved feature bits must be zero and the
// frozen ABI contract must hold exactly. Rejects inconsistent data without bumping for documentation
// or reusing bits.
bool verifyLibraryMetadataInvariants(const LibraryMetadata& metadata, std::string& error) noexcept;

}

#include "nativesharp/Interop/LibraryMetadataCodec.h"
