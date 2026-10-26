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
// LA VERSION DEL FORMATO .nsmeta, EN UN SOLO SITIO Y PUBLICA.
//
// Publica porque las pruebas necesitan saber cual es la actual: escrito a mano en cada sitio, el
// dia que el minor sube de 1 a 2 la prueba de "un minor del futuro se rechaza" pasa a comprobar el
// minor ACTUAL y sale roja -- que es exactamente lo que ocurrio. Un numero de version repetido es
// la misma forma de defecto que este arbol lleva todo el dia persiguiendo.
//
// MINOR 29 (2026-10-03): trait and implementing type records preserve associated type/constant
// requirements, defaults, definitions, and normalized integral values across LibraryReference.
// MINOR 28 (2026-10-03): interface-kind type records can identify NativeSharp 3 traits so a
// consumer reconstructs default methods and static property/operator requirements faithfully.
// MINOR 27 (2026-10-03): tagged variants preserve nominal identity plus their ordered case and
// payload catalogue. Case order is observable through CaseTag(), so reconstructing it from the
// alphabetically sorted public member table would be an ABI defect.
// MINOR 26 (2026-10-03): struct type flags preserve `record` and its four explicit derives so a
// LibraryReference consumer reconstructs the same NativeSharp 3.0 synthesized-member surface.
// MINOR 25 (2026-10-01): static-library metadata carries its binary LibraryReference closure and
// private platform imports so a consumer can link the archive without repeating implementation
// dependencies or losing DLL symbols.
// MINOR 23 (2026-09-29): el productor publica la edición de lenguaje 2/3 para que un consumidor
// precompilado no mezcle silenciosamente reglas de frontend.
// MINOR 19 (2026-09-15): las declaraciones C variadicas conservan el marcador `...` para que
// LibraryReference reconstruya la misma firma importada.
// MINOR 18 (2026-09-15): los bitfields conservan unidad, offset de bit y ancho para reproducir
// el layout MSVC en headers y LibraryReference.
// MINOR 17 (2026-09-15): los campos portadores de agregados anonimos conservan su marca para
// que una LibraryReference reconstruya la promocion de miembros sin aplanar el layout fisico.
// MINOR 16 (2026-09-14): metodos y constructores publicos conservan `noheap`.
// MINOR 15 (2026-09-14): los metodos publicos con retorno ref conservan su categoria de resultado
// y su portador ABI de direccion al reconstruirse desde una LibraryReference.
// MINOR 14 (2026-09-14): los campos publicos volatile conservan el contrato de acceso observable
// para que un consumidor LibraryReference no los degrade a accesos ordinarios.
// MINOR 13 (2026-09-13): los metodos inline ordinarios conservan politica y cuerpo fuente para
// que un consumidor precompilado pueda analizarlos, emitirlos e inlinearlos localmente.
// MINOR 20 (2026-09-15): delegateFlags bit 1 conserva `extern vectorcall delegate`.
// MINOR 12 (2026-09-11): el modulo enumera identidades nominales propiedad de otras
// LibraryReference para validar la clausura completa antes de reconstruir su API.
// MINOR 11 (2026-09-10): las plantillas reutilizables conservan su grafo de dependencias y la
// clausura privada transitiva necesaria para materializarlas sin ampliar la API publica.
// MINOR 10 (2026-09-10): una declaracion publica `extern C from` conserva la DLL que posee el
// simbolo para que el consumidor reconstruya el import real sin atribuirselo al productor.
// MINOR 9 (2026-09-10): las constantes string conservan sus bytes para que el consumidor pueda
// materializar su valor en vez de perder el miembro publico.
// MINOR 8 (2026-09-07): el fichero publica el contrato ABI versionado y la representacion de
// delegates ordinarios. A partir de aqui NSMETA distingue su propia version de la version del ABI
// que describe; subir el formato no cambia por si solo los binarios de usuario.
// MINOR 7 (2026-09-07): cada plantilla generica publica conserva su implementacion fuente
// canonica para que otro modulo pueda monomorfizarla con el frontend del consumidor.
// MINOR 6 (2026-09-07): cada clase publicable conserva el accessor funcional de su descriptor.
// MINOR 5 (2026-09-07): un delegate conserva su firma y si es extern C.
// MINOR 4 (2026-09-06): cada parametro conserva Value/ref/out/in.
// MINOR 3 (2026-09-06): cada parametro conserva su valor predeterminado literal.
// MINOR 2 (2026-09-03): el registro de tipo lleva `enumUnderlyingTypeKey`.
// MINOR 1: los campos, miembros y parametros llevan `ownership`.
// MINOR 21 (2026-09-27): public namespace functions preserve signature and physical identity.
// MINOR 22 (2026-09-27): public namespace state preserves shared storage and initializers.
// MINOR 23 (2026-09-29): identity payloads reserve one historical source-revision byte.
// MINOR 24 (2026-09-29): parameters preserve the params ReadOnlySpan<T> call-site contract.
inline constexpr std::uint16_t kMetadataFormatMajor = 1;
// MINOR 34 (2026-10-03): public borrowed types and callable signatures preserve erased lifetime
// parameters, parameter/result bindings, and normalized outlives edges across LibraryReference.
// MINOR 33 (2026-10-03): value types preserve their explicit Copy capability across
// LibraryReference boundaries; an absent bit keeps the move-only default.
// MINOR 32 (2026-10-03): public extension methods preserve their receiver contract and extension
// lookup identity across LibraryReference boundaries.
// MINOR 31 (2026-10-03): traits publish the descriptor-backed one-word dyn-object ABI and public
// callable signatures retain the explicit `dyn` marker across LibraryReference boundaries.
// MINOR 30 (2026-10-03): declaration-scoped opaque result identities publish their one-field
// in-place representation and implemented trait as a closed native contract.
// MINOR 34 (2026-10-03): named lifetime binders and outlives edges cross module boundaries.
// MINOR 35 (2026-10-03): per-parameter interprocedural escape summaries are public contracts.
// MINOR 36 (2026-10-03): value types that own storage publish producer-owned drop-glue thunks.
// MINOR 37 (2026-10-03): delegates, members and namespace functions publish their typed-throws
// error key. Empty remains the canonical non-throwing contract.
// MINOR 38 (2026-10-05): reusable generic templates publish parameter kinds, constant types,
// move/copy/drop capability constraints, nominal/equality constraints, and erased lifetime
// binders/outlives edges as an explicit semantic catalogue in addition to materializable source.
// MINOR 38 (2026-10-04): extern-C parameters publish buffer-length and callback-lifetime
// contracts; members publish the explicit unwind-boundary policy.
// MINOR 41 (2026-10-26, freeze 3.0 P8): layout epoch. Arrays whose element needs a value drop
// carry a liveness map after their data (DEC-11), array-backed Span/ReadOnlySpan carry their
// source array and offset (P4) and members may set kMetadataMemberReadonlyReferenceReturn (P3).
// The payload encoding is unchanged; the number marks the producer's layout generation.
// MINOR 41 (2026-10-26, congelamiento 3.0 P8): época de layout. Los arreglos cuyo elemento
// requiere drop llevan mapa de vida tras sus datos (DEC-11), los Span/ReadOnlySpan sobre un
// arreglo llevan el arreglo y su desplazamiento (P4) y los miembros pueden llevar
// kMetadataMemberReadonlyReferenceReturn (P3). La codificación del payload no cambia; el número
// marca la generación de layout del productor.
inline constexpr std::uint16_t kMetadataFormatMinor = 41;

// EN: oldest minor produced with the 3.0 layout. A precompiled library below it is not consumed;
// the consumer asks for its recompilation instead of mixing two layouts.
// ES: minor más antiguo producido con el layout 3.0. Una biblioteca precompilada anterior no se
// consume; el consumidor pide recompilarla en vez de mezclar dos layouts.
inline constexpr std::uint16_t kMetadataLayoutEpochMinor = 41;

// NSMETA 1.39. These bits version producer language capabilities that affect private lowering
// and debug behavior without changing the public NativeSharp ABI projection. Older files decode
// as zero; deliberate down-level emission omits this catalogue.
inline constexpr std::uint32_t kMetadataLanguageStaticConstructors = 1U << 0U;
inline constexpr std::uint32_t kMetadataLanguageStaticLocals = 1U << 1U;
inline constexpr std::uint32_t kMetadataLanguageConditionDeclarations = 1U << 2U;
inline constexpr std::uint32_t kMetadataLanguageFeatureMask =
    kMetadataLanguageStaticConstructors |
    kMetadataLanguageStaticLocals |
    kMetadataLanguageConditionDeclarations;

// NSMETA 1.26. These bits belong to LibraryMetadataType::flags. A record is still physically a
// struct; the flags publish only its compile-time member-generation policy. They therefore do not
// change layout or the native ABI, but they must survive source materialization or a consumer would
// silently gain the legacy struct members and lose the record's explicit opt-ins.
inline constexpr std::uint16_t kMetadataTypeRecord = 1U << 7U;
inline constexpr std::uint16_t kMetadataTypeDeriveEquality = 1U << 8U;
inline constexpr std::uint16_t kMetadataTypeDeriveHash = 1U << 9U;
inline constexpr std::uint16_t kMetadataTypeDerivePrint = 1U << 10U;
inline constexpr std::uint16_t kMetadataTypeDeriveClone = 1U << 11U;
// NSMETA 1.27. A variant is physically a struct, but its ordered closed case domain is part of the
// public language and ABI contract. The payload catalogue below carries the rest of that identity.
inline constexpr std::uint16_t kMetadataTypeVariant = 1U << 12U;
// NSMETA 1.28. Traits intentionally share the interface ABI; this bit preserves their source and
// semantic contract without inventing a sixth physical type kind.
inline constexpr std::uint16_t kMetadataTypeTrait = 1U << 13U;
// NSMETA 1.30. An opaque result is physically a one-field struct. This bit distinguishes that
// compiler-owned nominal identity from a user struct so source/API/ABI tools cannot erase the
// `some Trait` abstraction while preserving the same bytes accidentally.
inline constexpr std::uint16_t kMetadataTypeOpaqueResult = 1U << 14U;
// NSMETA 1.33. This is a semantic capability, not a layout promise: validation still requires
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
// Item 328. These bits belong only to layout-field records. Minor 1.18 appends the exact bit
// coordinates; the flags distinguish a named bit-field from an unnamed padding declaration.
inline constexpr std::uint32_t kMetadataFieldBitField = 1U << 27U;
inline constexpr std::uint32_t kMetadataFieldUnnamedBitField = 1U << 28U;
// Item 331. Bit 29 belongs only to imported-C method records and preserves the terminal `...`.
inline constexpr std::uint32_t kMetadataMemberVariadic = 1U << 29U;
// NSMETA 1.31. The canonical key intentionally remains the trait/interface key because dyn uses
// that ABI. This semantic bit preserves the explicit dynamic-dispatch spelling for consumers.
inline constexpr std::uint32_t kMetadataMemberDynamicTrait = 1U << 30U;
// EN: Freeze 3.0 P3. Bit 31 accompanies bit 24 on a callable declared `ref readonly T`: the alias it
//     returns may be read but not written. Consumers rebuild the `ref readonly` spelling.
// ES: Congelamiento 3.0 P3. El bit 31 acompana al bit 24 en un callable declarado `ref readonly T`:
//     el alias que devuelve se puede leer pero no escribir. Los consumidores reconstruyen la forma
//     `ref readonly`.
inline constexpr std::uint32_t kMetadataMemberReadonlyReferenceReturn = 1U << 31U;

// NSMETA 1.32. LibraryMetadataNamespaceFunction::flags bit 5 marks the declaration as an
// extension method. Its first parameter is the implicit receiver and retains Value/ref/in in the
// normal parameter record; consumers must reconstruct an extension block rather than publishing
// the synthetic receiver as an ordinary free-function parameter.
inline constexpr std::uint8_t kMetadataNamespaceFunctionExtension = 1U << 5U;
// NS304-T08-F07 (NSMETA 1.32 addendum): bit 6 marks a STATIC extension (`ns3-extension-static`),
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
// NativeSharp 2.0 item 319 appends the object-only destruction entry to every class descriptor.
// Existing field offsets remain stable, but a producer without field 7 cannot satisfy `destroy`.
inline constexpr std::uint16_t kObjectAbiVersion = 2;
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
    std::uint16_t formatMajor = kMetadataFormatMajor;
    std::uint16_t formatMinor = kMetadataFormatMinor;
    // Transient provenance. This is never serialized and is set only after the
    // current reader has decoded and authenticated the complete payload. It lets
    // verification distinguish a forward-labelled file whose actual contents
    // were understood from an in-memory model that merely claims a future format.
    bool payloadDecodedByCurrentReader = false;
    std::uint32_t targetId = 1;
    std::uint32_t pointerSize = 8;
    std::uint32_t pointerAlignment = 8;
    std::uint32_t languageFeatureFlags = 0;
    std::string producerVersion;
    std::string moduleName;
    std::string targetTriple;
    // NSMETA 1.8. Version del CONTRATO descrito, independiente de formatMajor/formatMinor.
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
    // NSMETA 1.12: exact nominal types used by this module but owned by another
    // LibraryReference.  A consumer validates these keys against the complete
    // metadata set before parsing any reconstructed API.  Closed generic types
    // already materialized by their provider belong here too; they must not be
    // republished as if this module owned their specialization.
    std::vector<std::string> externalTypeKeys;
    std::vector<LibraryMetadataDispatch> dispatchTables;
    std::vector<LibraryMetadataExport> exports;
    // NSMETA 1.25. These are implementation closure edges of a static archive, not public API.
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
