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
// MINOR 2 (2026-09-03): el registro de tipo lleva `enumUnderlyingTypeKey`.
// MINOR 1: los campos, miembros y parametros llevan `ownership`.
inline constexpr std::uint16_t kMetadataFormatMajor = 1;
inline constexpr std::uint16_t kMetadataFormatMinor = 2;
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
    std::vector<LibraryMetadataField> fields;
};

struct LibraryMetadataParameter final
{
    std::string name;
    std::string typeKey;
    LibraryMetadataOwnershipKind ownership = LibraryMetadataOwnershipKind::None;
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
};

struct LibraryMetadataGenericTemplate final
{
    std::uint8_t kind = 0;
    std::string canonicalKey;
    std::string moduleName;
    std::string namespaceName;
    std::string ownerName;
    std::string name;
    std::vector<std::string> parameters;
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
    std::array<std::uint8_t, 32> abiFingerprint{};
    std::vector<LibraryMetadataType> types;
    std::vector<LibraryMetadataMember> members;
    std::vector<LibraryMetadataGenericTemplate> genericTemplates;
    std::vector<LibraryMetadataGenericSpecialization> genericSpecializations;
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
