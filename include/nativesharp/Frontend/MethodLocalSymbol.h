#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisContext.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisStatistics.h"
#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectConstructionSite.h"
#include "nativesharp/Frontend/ProjectOwnershipFlowStatistics.h"
#include "nativesharp/Frontend/ProjectEscapePropagationStatistics.h"
#include "nativesharp/Frontend/ProjectMethodBodyBuildOptions.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Sema/ExpressionInfo.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct MethodLocalSymbol final
{
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::size_t declarationOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string name;
    std::string typeKey;
    // NativeSharp 3.0.4 T01-F06. Static locals retain lexical name lookup but their storage and
    // lifetime belong to the defining module rather than to an invocation frame.
    bool isStatic = false;
    // NativeSharp 2.0 item 269: `T[length] local;` is stack storage whose extent is
    // evaluated when the declaration executes.  Downstream expression typing uses the
    // pointer-shaped `typeKey`, while these two fields preserve the array declaration's
    // element type and count expression so HIR can synthesize the existing stack-allocation
    // operation without changing fields, parameters, or return types.
    bool isVariableLengthArray = false;
    std::string variableLengthArrayElementTypeKey;
    const ast::AstNode* variableLengthArrayCountSyntax = nullptr;
    // Source-order dimensions for multidimensional VLAs plus the compiler-generated locals
    // that snapshot each extent exactly once at the declaration.
    std::vector<const ast::AstNode*> variableLengthArrayExtentSyntaxes;
    std::vector<std::string> variableLengthArrayExtentTypeKeys;
    std::vector<std::size_t> variableLengthArrayExtentLocalOrdinals;
    // Set on the VLA itself and on local pointer aliases derived from it. This provenance is
    // consumed by return, assignment, and lambda-capture checks so frame storage cannot escape.
    std::optional<std::size_t> variableLengthArrayRootLocalOrdinal;
    // NativeSharp 2.0 item 312. typeKey is the logical element type; the physical local slot
    // stores an address to another exact-type, mutable location.
    bool isReference = false;
    // Pattern `ref` and `ref mut` share the physical alias representation, but only the latter
    // grants write capability. Preserve that distinction after the parser-owned lowering so HIR,
    // MIR, diagnostics and cache identity never have to infer it from source text.
    bool referenceMutable = false;
    // Item 315. A ref local keeps the escape origin of the address it aliases. This closes
    // transitive escapes (`ref alias = ref local; return ref alias`) without guessing again at
    // the return site and also propagates through reference-returning calls.
    MethodEscapeOrigin referenceEscapeOrigin;
    // ES: Para un local puntero por valor: el origen más inseguro de cualquier dirección que se le
    // haya asignado (inicializador y asignaciones; unión sin flujo). `p = &local` lo marca como
    // almacenamiento del marco; `p = Allocate(n)` o `p = Global` no. Al pasar `p` a un parámetro
    // retenido se valida este origen, no el del propio slot de `p`.
    // EN: For a by-value pointer local: the least safe origin of any address ever assigned to it
    // (initializer and assignments; flow-insensitive join). `p = &local` marks it as frame
    // storage; `p = Allocate(n)` or `p = Global` does not. Passing `p` to a retained parameter
    // validates this origin, not the one of `p`'s own slot.
    MethodEscapeOrigin pointerValueOrigin;
    std::optional<std::size_t> referenceLifetimeScopeOrdinal;
    // NativeSharp 3.0 T06-F03. Mutable reference locals are exclusive borrows of a canonical
    // storage place.  The key is rooted in a local/parameter (L#/P#) and contains field/index
    // projections.  "U" represents an origin that cannot be proven and therefore overlaps
    // every other live mutable borrow; empty is reserved for non-reference locals.
    std::string referencePlaceKey;
    // A reborrow does not create a second independent alias.  It temporarily suspends this
    // source reference while the child reference is live, and preserves the original place.
    std::optional<std::size_t> referenceReborrowSourceLocalOrdinal;
    bool isConst = false;
    // T42: volatile local storage uses the same observable-load/store contract as volatile
    // fields. It is not atomic and supplies no inter-thread ordering.
    bool isVolatile = false;
    bool onePastEndPointer = false;
    bool pointerPointeeReadonly = false;
    bool pointerNullable = true;
    bool pointerHasProvenance = true;
    std::size_t pointerKnownAlignment = 1;
    unsigned pointerAddressSpace = 0;
    // El VALOR de un local `const` de tipo entero, plegado al declararlo.
    //
    // POR QUE SE GUARDA AQUI y no se recalcula al usarlo: la expresion que lo inicializa esta en
    // el sitio de la declaracion y en ningun otro. Cuando alguien escribe `x / cero`, lo unico
    // que tiene a mano es el nombre.
    //
    // Y POR QUE NO VIAJA EN `ExpressionResult::integerConstant`, que seria lo obvio: ese campo lo
    // consume `commonNumericType` para decidir el tipo comun de una operacion, asi que darle un
    // valor a cada `const` local cambiaria la PROMOCION de expresiones que hoy compilan. Esto se
    // usa para una pregunta concreta --¿el divisor es cero?-- y no para tipar nada.
    std::optional<std::int64_t> constantIntegerValue;
    bool isRaii = false;
    // EN: Freeze 3.0 DEC-10. The exception a catch handles belongs to the handler until the handler
    //     ends or `move(e)` takes it out. `isCatchVariable` marks the catch parameter;
    //     `caughtExceptionScopeOrdinal` is the catch scope of the exception this local names. It is
    //     set on the catch parameter and on every local of that handler that copies it, so storing
    //     either where it outlives the handler (a field, a static, an outer local, a return) is a
    //     compile error.
    // ES: Congelamiento 3.0 DEC-10. La excepcion que maneja un catch pertenece al handler hasta que
    //     termina o `move(e)` la saca. `isCatchVariable` marca el parametro del catch;
    //     `caughtExceptionScopeOrdinal` es el scope del catch cuya excepcion nombra este local. Se
    //     fija en el parametro del catch y en cada local de ese handler que lo copia, asi guardar
    //     cualquiera donde sobrevive al handler (un campo, un static, un local exterior, un return)
    //     es error de compilacion.
    bool isCatchVariable = false;
    std::optional<std::size_t> caughtExceptionScopeOrdinal;
    // EN: Freeze 3.0 P10. The ordinal of the exception parameter this local copies: a non-owned
    //     parameter of an exception type is a loan from the caller (it may be a caught exception), so
    //     neither it nor a local that copies it may be stored where it outlives the call.
    // ES: Congelamiento 3.0 P10. El ordinal del parametro de excepcion que copia este local: un
    //     parametro no owned de tipo excepcion es un prestamo del llamador (puede ser una excepcion
    //     atrapada), asi que ni el ni un local que lo copia pueden guardarse donde sobreviven a la
    //     llamada.
    std::optional<std::size_t> loanedExceptionParameterOrdinal;
    // NS304-T13-F03: `scope T x = new T(...)` — the object's physical storage lives in the current
    // frame (stack), never heap; the destructor runs once at lexical-scope exit and never calls the
    // heap deallocator. Escape (return/capture/field/static) is rejected by Sema.
    bool isScope = false;
    // This local stores a value type that did not opt into Copy. It shares the existing
    // path-sensitive moved-from sets with RAII owners, but it does not acquire a cleanup region
    // merely for being move-only.
    bool isMoveOnly = false;
    // T08-F03. Present for locals initialized from an explicit-capture closure. The delegate
    // carrier stays ABI-compatible; this semantic capability controls invocation rules.
    std::optional<language::CallableTrait> callableTrait;
    // T06-F02. A value local with an owned field or a (possibly nested) custom destructor owns
    // cleanup despite not being a reference-shaped `raii` local.
    bool requiresValueDrop = false;
    // NativeSharp 3.0 T06-F06. A `pin raii` owner keeps its ownership slot and object identity
    // fixed until lexical cleanup. It may be borrowed and destroyed in place, but never moved or
    // replaced after initialization.
    bool isPinned = false;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    // Item 320. A placement result owns object destruction while borrowing supplied bytes.
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
    // ES: Array cuyo inicializador guardó literales de cadena: esos slots no le pertenecen (los
    // literales duran todo el proceso), así que `delete[]` libera sólo el bloque.
    // EN: Array whose initializer stored string literals: it does not own those slots (literals
    // last for the whole process), so `delete[]` releases the block only.
    bool borrowedElementSlots = false;
    std::optional<std::size_t> localBorrowBindingOrdinal;
    std::optional<std::size_t> lambdaOrdinal;
    std::optional<std::size_t> lambdaParameterOrdinal;
    // SHR-013 "vistas y tiempo de vida". Solo para un local de tipo vista: el NOMBRE del local
    // de este mismo metodo del que la vista toma prestado, si lo hay. Vacio quiere decir que la
    // vista se enraiza en algo que sobrevive a la llamada -- un parametro, un campo, un literal,
    // o el resultado de una llamada que no menciono ningun local.
    //
    // Se calcula al declarar y se vuelve a calcular en cada asignacion, porque `v = otra;`
    // cambia de que presta `v`. Es un ANALISIS SINTACTICO deliberadamente conservador: mira que
    // nombres aparecen en la expresion, no a donde apunta el puntero en ejecucion. Un analisis
    // exacto necesitaria parametros de tiempo de vida en las firmas, que es otro problema y no
    // el de este bloque; este contesta la pregunta que el bloque plantea -- devolver una vista
    // de un local -- y se equivoca siempre del lado de rechazar, nunca del de aceptar.
    std::string viewRootLocalName;
};
}
