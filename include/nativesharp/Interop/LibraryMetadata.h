#pragma once

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
// MINOR 15 (2026-09-14): los metodos publicos con retorno ref conservan su categoria de resultado
// y su portador ABI de direccion al reconstruirse desde una LibraryReference.
// MINOR 14 (2026-09-14): los campos publicos volatile conservan el contrato de acceso observable
// para que un consumidor LibraryReference no los degrade a accesos ordinarios.
// MINOR 13 (2026-09-13): los metodos inline ordinarios conservan politica y cuerpo fuente para
// que un consumidor precompilado pueda analizarlos, emitirlos e inlinearlos localmente.
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
inline constexpr std::uint16_t kMetadataFormatMajor = 1;
inline constexpr std::uint16_t kMetadataFormatMinor = 15;

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

// Contrato binario de NativeSharp 1.0. Son numeros, no nombres decorativos: entran en el
// fingerprint ABI, se verifican antes de compilar al consumidor y sólo cambian cuando cambia una
// decisión física. NSMETA puede evolucionar de forma aditiva manteniendo estos valores.
inline constexpr std::uint16_t kNativeAbiMajor = 1;
inline constexpr std::uint16_t kNativeAbiMinor = 0;
inline constexpr std::uint16_t kRuntimeAbiMajor = 1;
inline constexpr std::uint16_t kRuntimeAbiMinor = 0;
inline constexpr std::uint16_t kManglingAbiVersion = 1;
inline constexpr std::uint16_t kObjectAbiVersion = 1;
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
    // reconstruct `int Transform(int)`. Bit 0 marks the raw-function-pointer `extern C` form.
    std::uint8_t delegateFlags = 0;
    std::string delegateReturnTypeKey;
    std::vector<std::string> delegateParameterTypeKeys;
    std::vector<LibraryMetadataField> fields;
};

struct LibraryMetadataParameter final
{
    std::string name;
    std::string typeKey;
    LibraryMetadataOwnershipKind ownership = LibraryMetadataOwnershipKind::None;
    LibraryMetadataParameterPassKind passKind = LibraryMetadataParameterPassKind::Value;
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
};

class LibraryMetadataCodec final
{
public:
    bool write(
        const frontend::ProjectCompilation& compilation,
        const std::filesystem::path& destination,
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
};
}
