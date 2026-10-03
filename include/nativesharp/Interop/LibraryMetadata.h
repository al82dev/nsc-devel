#pragma once

#include "nativesharp/Runtime/RuntimeAbiContract.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

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
// MINOR 23 (2026-09-29): modules preserve their NativeSharp language edition.
// MINOR 24 (2026-09-29): parameters preserve the params ReadOnlySpan<T> call-site contract.
inline constexpr std::uint16_t kMetadataFormatMajor = 1;
// MINOR 33 (2026-10-03): edition-3 value types preserve their explicit Copy capability across
// LibraryReference boundaries; an absent bit keeps the move-only default.
// MINOR 32 (2026-10-03): public extension methods preserve their receiver contract and extension
// lookup identity across LibraryReference boundaries.
// MINOR 31 (2026-10-03): traits publish the descriptor-backed one-word dyn-object ABI and public
// callable signatures retain the explicit `dyn` marker across LibraryReference boundaries.
// MINOR 30 (2026-10-03): declaration-scoped opaque result identities publish their one-field
// in-place representation and implemented trait as a closed native contract.
inline constexpr std::uint16_t kMetadataFormatMinor = 33;

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

// NSMETA 1.32. LibraryMetadataNamespaceFunction::flags bit 5 marks the declaration as an
// extension method. Its first parameter is the implicit receiver and retains Value/ref/in in the
// normal parameter record; consumers must reconstruct an extension block rather than publishing
// the synthetic receiver as an ordinary free-function parameter.
inline constexpr std::uint8_t kMetadataNamespaceFunctionExtension = 1U << 5U;

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
enum class LibraryMetadataTypeKind : std::uint8_t
{
    Class = 0,
    Interface = 1,
    Struct = 2,
    Enum = 3,
    Delegate = 4
};

enum class LibraryMetadataOwnershipKind : std::uint8_t
{
    None = 0,
    Owned = 1,
    Borrowed = 2
};

enum class LibraryMetadataDefaultValueKind : std::uint8_t
{
    None = 0,
    Null = 1,
    Boolean = 2,
    Integer = 3,
    Floating = 4,
    Character = 5,
    String = 6
};

enum class LibraryMetadataParameterPassKind : std::uint8_t
{
    Value = 0,
    Ref = 1,
    Out = 2,
    In = 3
};

struct LibraryMetadataField final
{
    std::string name;
    std::string typeKey;
    LibraryMetadataOwnershipKind ownership = LibraryMetadataOwnershipKind::None;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint32_t flags = 0;
    std::uint64_t bitOffset = 0;
    std::uint64_t bitWidth = 0;
};

struct LibraryMetadataVariantCase final
{
    std::string name;
    // Empty for a unit case; otherwise the canonical type key of the in-place payload.
    std::string payloadTypeKey;
};

enum class LibraryMetadataAssociatedItemKind : std::uint8_t
{
    Type = 0,
    Constant = 1
};

struct LibraryMetadataAssociatedItem final
{
    LibraryMetadataAssociatedItemKind kind = LibraryMetadataAssociatedItemKind::Type;
    std::string name;
    // Associated type: canonical default/definition type key, empty for a requirement.
    // Associated constant: canonical declared value type key.
    std::string typeKey;
    bool hasDefault = false;
    // Normalized typed integral/boolean bits for a constant default/definition. Zero otherwise.
    std::uint64_t valueBits = 0;
};

struct LibraryMetadataType final
{
    LibraryMetadataTypeKind kind = LibraryMetadataTypeKind::Struct;
    std::uint8_t accessibility = 0;
    std::uint16_t flags = 0;
    std::string identityKey;
    std::string fullyQualifiedName;
    std::string baseTypeKey;
    std::vector<std::string> interfaceTypeKeys;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint64_t headerSize = 0;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    std::uint64_t stableTypeId = 0;
    // EL TIPO ENTERO SUBYACENTE DE UN ENUM ("builtin:byte", "builtin:sbyte"...). Vacio para todo
    // lo que no sea un enum.
    //
    // POR QUE NO BASTA CON `size`. `enum : byte` y `enum : sbyte` miden LOS DOS un byte, asi que
    // un consumidor que solo lea el tamano no puede distinguirlos -- y un generador de host que
    // eligiera el signo contrario convertiria un 200 en un -56. El signo es parte del ABI igual
    // que el ancho, y estos metadatos son lo que los generadores de host leen.
    //
    // Se guarda la CLAVE y no un par (ancho, signo) porque la clave es lo que ya circula por todo
    // el compilador, y porque lleva las dos respuestas sin que nadie tenga que recomponerlas.
    std::string enumUnderlyingTypeKey;
    // NSMETA 1.5. Delegate declarations are types, not ordinary member records. Without their
    // return and parameter keys a precompiled consumer can know that `Transform` exists but not
    // reconstruct `int Transform(int)`. Bit 0 marks a raw-function-pointer extern delegate and
    // bit 1 selects Microsoft x64 vectorcall rather than the ordinary C convention.
    std::uint8_t delegateFlags = 0;
    std::string delegateReturnTypeKey;
    std::vector<std::string> delegateParameterTypeKeys;
    // NSMETA 1.31. `dyn` is a source-level trait-object contract. It uses the same
    // physical reference ABI as the nominal interface key, but must survive source
    // materialization and API-diff round trips.
    bool delegateReturnDynamicTrait = false;
    std::vector<std::uint8_t> delegateParameterDynamicTraits;
    // NSMETA 1.27. Declaration order is deliberately retained because it defines CaseTag().
    std::vector<LibraryMetadataVariantCase> variantCases;
    // NSMETA 1.28. Effective instance-method signatures in their normative trait-dispatch order.
    // Default method bodies compiled by the producer use these ordinals, so a consumer must not
    // reconstruct the trait from the signature-sorted public member catalogue.
    std::vector<std::string> traitMethodSignatures;
    // NSMETA 1.29. Ordered as declared. These are logical compile-time items and never fields,
    // vtable slots, or exported storage.
    std::vector<LibraryMetadataAssociatedItem> associatedItems;
    // NSMETA 1.31. Zero for non-traits. NativeSharp's Windows x64 representation is one
    // object-reference word; dispatch is selected through the object's descriptor/interface map.
    std::uint8_t traitObjectFlags = 0;
    std::uint8_t traitObjectWordCount = 0;
    std::vector<LibraryMetadataField> fields;
};

struct LibraryMetadataParameter final
{
    std::string name;
    std::string typeKey;
    LibraryMetadataOwnershipKind ownership = LibraryMetadataOwnershipKind::None;
    LibraryMetadataParameterPassKind passKind = LibraryMetadataParameterPassKind::Value;
    // NSMETA 1.24. The final parameter keeps its `params ReadOnlySpan<T>` call-site contract
    // across a LibraryReference boundary; the physical type and ABI remain unchanged.
    bool isParameterPack = false;
    // NSMETA 1.31. Source-level dynamic dispatch marker; physical typeKey is unchanged.
    bool isDynamicTrait = false;
    // NSMETA 1.3. `defaultValueBits` stores the integer/character magnitude, a boolean bit, or
    // the IEEE-754 bits of the parsed double literal. Strings use `defaultValueText`; null has no
    // payload. Bit 0 of `defaultValueFlags` preserves a unary minus for integer literals.
    LibraryMetadataDefaultValueKind defaultValueKind = LibraryMetadataDefaultValueKind::None;
    std::uint8_t defaultValueFlags = 0;
    std::uint64_t defaultValueBits = 0;
    std::string defaultValueText;
};

struct LibraryMetadataMember final
{
    std::uint8_t kind = 0;
    std::uint8_t accessibility = 0;
    std::uint32_t flags = 0;
    std::string declaringTypeKey;
    std::string name;
    std::string signatureKey;
    std::string declaredTypeKey;
    LibraryMetadataOwnershipKind declaredOwnership = LibraryMetadataOwnershipKind::None;
    std::string cAbiName;
    std::string mangledName;
    std::vector<LibraryMetadataParameter> parameters;
    std::uint8_t constantKind = 0;
    std::uint64_t constantBits = 0;
    // NSMETA 1.9. Payload UTF-8 de una constante string; vacio para los demas kinds.
    std::string constantText;
    // NSMETA 1.10. DLL de origen para `extern C from`; vacia para imports estaticos y miembros
    // NativeSharp ordinarios. El bit ImportedC de flags distingue los dos ultimos casos.
    std::string importDllName;
    // NSMETA 1.13. Declaracion completa, incluido el cuerpo, de un metodo ordinario `inline` o
    // `forceinline`. Vacio para contratos, imports, miembros no inline y especializaciones cuya
    // plantilla reutilizable ya viaja en `genericTemplates`.
    std::string inlineSourceText;
};

struct LibraryMetadataNamespaceFunction final
{
    std::uint8_t accessibility = 0;
    std::uint8_t flags = 0;
    std::string namespaceName;
    std::string name;
    std::string signatureKey;
    std::string declaredTypeKey;
    LibraryMetadataOwnershipKind declaredOwnership = LibraryMetadataOwnershipKind::None;
    std::string mangledName;
    std::vector<LibraryMetadataParameter> parameters;
};

enum class LibraryMetadataNamespaceStateInitializerKind : std::uint8_t
{
    Zero = 0,
    Integer = 1,
    Floating = 2,
    Boolean = 3,
    String = 4,
    Null = 5
};

struct LibraryMetadataNamespaceState final
{
    std::uint8_t accessibility = 0;
    std::uint8_t flags = 0;
    LibraryMetadataNamespaceStateInitializerKind initializerKind =
        LibraryMetadataNamespaceStateInitializerKind::Zero;
    std::string namespaceName;
    std::string name;
    std::string declaredTypeKey;
    std::string storageName;
    std::string accessorName;
    std::uint64_t initializerBits = 0;
    std::string initializerText;
};
struct LibraryMetadataGenericTemplate final
{
    std::uint8_t kind = 0;
    std::uint8_t accessibility = 0;
    std::string canonicalKey;
    std::string moduleName;
    std::string namespaceName;
    std::string ownerName;
    std::string name;
    std::vector<std::string> parameters;
    // NSMETA 1.7: declaration slice as authored by the producer. Public entries are reusable API;
    // non-public entries are materialization-only dependencies and retain their original access.
    // The consumer parses both in the producer's synthetic project, while only public roots are
    // nameable from the consumer project.
    std::string sourceText;
    // NSMETA 1.11: canonical template identities directly required by this declaration/body.
    // Same-module edges must be present in the file; external-module edges are resolved from the
    // consumer's other LibraryReferences.
    std::vector<std::string> dependencies;
};

struct LibraryMetadataGenericSpecialization final
{
    std::uint8_t kind = 0;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
};

struct LibraryMetadataDispatchInterface final
{
    std::string interfaceTypeKey;
    std::uint64_t stableInterfaceTypeId = 0;
    std::string methodTableName;
    std::vector<std::string> interfaceSignatures;
    std::vector<std::string> implementationSignatures;
    std::vector<std::string> calleeNames;
};

struct LibraryMetadataDispatch final
{
    std::string typeKey;
    std::uint64_t stableTypeId = 0;
    std::string descriptorName;
    // NSMETA 1.6. PE imports functions and data through different indirections. Consumers call
    // this no-argument function to obtain the producer's actual descriptor address instead of
    // treating an import-address-table cell as the descriptor itself.
    std::string descriptorAccessorName;
    std::string vtableName;
    std::string destructionEntryName;
    std::vector<std::string> slotSignatures;
    std::vector<std::string> slotCalleeNames;
    std::vector<LibraryMetadataDispatchInterface> interfaces;
};

struct LibraryMetadataExport final
{
    std::string cAbiName;
    std::string declaringTypeKey;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
    std::string nativeMangledName;
};

struct LibraryMetadataBinaryDependency final
{
    std::string moduleName;
    std::string libraryFileName;
    std::string metadataFileName;
    std::string runtimeFileName;
    std::array<std::uint8_t, 32> abiFingerprint{};
};

struct LibraryMetadataPlatformImport final
{
    std::string dllName;
    std::string symbolName;
};

struct LibraryMetadata final
{
    std::uint16_t formatMajor = kMetadataFormatMajor;
    std::uint16_t formatMinor = kMetadataFormatMinor;
    std::uint32_t targetId = 1;
    std::uint32_t pointerSize = 8;
    std::uint32_t pointerAlignment = 8;
    std::string producerVersion;
    std::string moduleName;
    std::string targetTriple;
    std::uint8_t languageEdition = 2;
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

class LibraryMetadataCodec final
{
public:
    bool write(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
        std::string& error) const;

    bool write(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
        LibraryMetadata& writtenModel,
        std::string& error) const;

    // Escribe un modelo ya construido, en la version menor que el modelo dice. Es lo que permite
    // MEDIR la compatibilidad hacia atras en vez de afirmarla: produce un fichero de una version
    // anterior y leelo con el lector actual.
    bool writeModel(
        const LibraryMetadata& model,
        const std::filesystem::path& destination,
        std::string& error) const;

    bool read(
        const std::filesystem::path& source,
        LibraryMetadata& metadata,
        std::string& error) const;

    bool verifyCurrentCompiler(
        const LibraryMetadata& metadata,
        std::string& error) const;

    static std::string fingerprintHex(const std::array<std::uint8_t, 32>& fingerprint);

private:
    bool writeModelImpl(
        const LibraryMetadata& model,
        const std::filesystem::path& destination,
        LibraryMetadata* writtenModel,
        std::string& error) const;
};
}
