#pragma once

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopMirStaticField final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    std::string name;
    std::string typeKey;
    bool isReadonly = false;
    bool isVolatile = false;
    bool isConst = false;
    bool isImported = false;
    bool isCppExternal = false;
    bool cppDllImport = false;
    std::string genericSpecializationKey;
    // Mutable static storage remains owned by the producer. Consumers call the stable accessor
    // carried in NSMETA instead of defining a second global with a compilation-local ordinal.
    std::string storageSymbol;
    std::string accessorName;
    bool hasConstantInitializer = false;
    std::uint64_t constantIntegerValue = 0;
    double constantFloatingValue = 0.0;
    bool constantBooleanValue = false;
    // String constant bytes, or the exact decimal literal spelling for a decimal constant.
    std::string constantStringValue = {};
    // L-002: a C string pointer (`sbyte const*`, `byte const*`) initialized with a string literal points at the
    //     literal's NUL-terminated UTF-8 bytes (constantStringValue); false keeps a pointer constant null.
    //     / Un puntero de cadena C inicializado con un literal apunta a sus bytes UTF-8 con NUL.
    bool constantStringPointer = false;
    // Stage 51.x: an enum member's declared type is its own enum type (so it type-checks and
    // compares correctly against other enum-typed expressions), not its int-sized underlying
    // representation -- so `ProjectTypeConversion::isIntegral(typeKey)` returns false for it.
    // This flag lets the LLVM lowerer take the integral constant-materialization path anyway.
    bool isEnumConstant = false;
    std::optional<std::size_t> namespaceStateOrdinal = std::nullopt;
    SourceRange sourceRange;
    std::string debugDisplayName;
    bool cppThreadLocal = false;
    // EN: T19-036. Each access first runs the CRT's on-demand TLS initialization if __tls_guard is clear.
    // ES: T19-036. Cada acceso ejecuta antes la inicializacion TLS bajo demanda del CRT si __tls_guard es cero.
    bool cppTlsOnDemand = false;
    // EN: T19-005. C++ reference storage: the symbol holds the address of the referent, loaded at each access.
    // ES: T19-005. Almacenamiento referencia C++: el simbolo guarda la direccion del referente, cargada en cada acceso.
    bool cppReferenceStorage = false;
    // T19-041: the address comes from calling `storageSymbol`. / La direccion sale de llamar a `storageSymbol`.
    bool cppMacroAccessor = false;
    std::string cppTlsRuntimeIdentity;
    bool cppCBoolStorage = false;
    // EN: C2-07. The accessor of a value macro writes the value to the caller's storage, its only argument.
    // ES: C2-07. La accesora de una macro de valor escribe el valor en el almacenamiento de quien llama, su unico
    //     argumento.
    bool cppMacroValue = false;
};
}
