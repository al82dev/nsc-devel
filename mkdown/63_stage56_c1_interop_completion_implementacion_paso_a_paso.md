# Stage 56 / C1 — Implementación paso a paso

Registro mecánico de la ronda: qué archivo se tocó, en qué orden y por qué. El diseño y el
razonamiento están en `63_stage56_c1_interop_completion.md`; esto es el cómo.

---

## Paso 0 — Línea base (antes de tocar nada)

Regla 2 del método de trabajo: extraer el proyecto real, construirlo y correr los tests **antes**
de afirmar nada.

1. Extracción del ZIP recibido (`NativeSharp_Stage55x_Interop_Foundation_full_1.zip`).
2. El entorno traía LLVM 18; `CMakeLists.txt` exige **LLVM 20 o superior** (por
   `LLVMRunPassesOnFunction`, ver bug B-008). Se instaló `llvm-20-dev` y se configuró con
   `-DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm`.
3. Configuración con `-DNSC_WARNINGS_AS_ERRORS=ON` **desde el primer build** (regla de proceso
   adoptada en Stage 54.3): build limpio.
4. `ctest` completo: **232/232**. Esa es la línea base contra la que se mide todo lo que sigue.
5. **Wine no está disponible en este sandbox.** Se preguntó y Alberto instruyó no invertir tiempo
   en instalarlo, así que la mitad de ejecución real de la política de testing queda documentada
   como skip explícito (ver la sección de verificación del documento conceptual).

## Paso 1 — Medir los tres huecos antes de arreglarlos

Se escribieron programas mínimos y se compilaron con el `nsc` de la línea base, para tener el
comportamiento **medido** y no inferido:

| Programa | Resultado medido |
|---|---|
| `nint raw = (nint)fp;` | `nsc: error: Stage 34 local initialization failed` |
| `Adder g = (Adder)raw;` | idem |
| `void* p = (void*)fp;` | idem |
| `Other g = (Other)fp;` | idem |
| `Adder g = (Adder)fp;` (mismo tipo) | compila (el cast era un no-op trivial) |
| import con retorno `extern C delegate` | `Stage 47 C ABI does not return delegate values` |
| `a.dll!Init` + `b.dll!Init` | `lld-link: error: undefined symbol: Init.1` |

El último es el hallazgo que fijó el diseño de C1.2: **LLVM renombraba la segunda declaración por su
cuenta.**

## Paso 2 — C1.1a: permitir el retorno de function pointers

`src/Frontend/ProjectMemberModel.cpp` — el rechazo de retornos delegate se estrechó de "cualquier
delegate" a "delegate que **no** sea `extern C`". El mensaje pasó a nombrar el arreglo
(*"return an extern C delegate (a raw function pointer) instead"*).

`src/LLVM/ProjectOopLlvmLowerer.cpp` — `defineCAbiWrappers` tenía la mitad de backend de esa misma
regla (`Stage 47 does not return extern C delegates`); se eliminó, porque `llvmType` ya mapea un
`extern C delegate` a `ptr` y el wrapper no necesita nada especial.

## Paso 3 — C1.1b: clasificar el cast en el frontend

`include/nativesharp/Frontend/ProjectMethodBodyModel.h` + `.cpp` — cuatro predicados nuevos sobre
type keys: `isExternCDelegateTypeKey`, `isAnyDelegateTypeKey`, `isRawPointerTypeKey`,
`isPointerSizedIntegralTypeKey`. El primero lee el modificador `extern` de la sintaxis del tipo,
exactamente igual que `isExternCDelegateType` del backend — una regla, dos lugares que deben
coincidir, ambos leyendo la misma fuente en vez de un flag cacheado.

En `analyzeExpression`, caso `CastExpression`, **antes** de la rama de conversiones de referencia:

- si alguno de los dos lados es `extern C delegate`, el otro debe ser "address-like"
  (`extern C delegate`, `T*`/`void*`, `nint`/`nuint`); si no, diagnóstico y `result.valid = false`;
- si alguno de los dos lados es un delegate **ordinario** y el otro es address-like, diagnóstico
  aparte que nombra el arreglo (`declare the type 'extern C delegate'`).

`isPointerSizedIntegralTypeKey` acepta deliberadamente **sólo** `nint`/`nuint`, no cualquier
integral: pasar un puntero a función por un `int` truncaría en Win64.

## Paso 4 — C1.1c: bajar el cast a LLVM

`src/LLVM/ProjectOopLlvmLowerer.cpp`, función `convert()` — tres líneas al final, después de todo
el mundo numérico y de referencias:

```cpp
if (sourceIsAddress && targetIsAddress) return value;                 // punteros opacos: no-op
if (sourceIsAddress && targetWidth != 0)  return LLVMBuildPtrToInt(...);
if (sourceWidth != 0 && targetIsAddress)  return LLVMBuildIntToPtr(...);
```

donde "address" = `isPointerLike || isClassReference || isExternCDelegateType`. El frontend ya
rechazó toda combinación que no sea parte del round trip, así que llegar aquí significa que la
conversión es una de las tres sancionadas.

Verificado en el IR del programa del Paso 1: `ptrtoint ptr … to i64` e `inttoptr i64 … to ptr`, y
**cero** `delegate.code`.

## Paso 5 — C1.2a: el triple, en el modelo de miembros

`include/nativesharp/Frontend/ProjectMemberModel.h` — campo nuevo
`importRequiresRuntimeBinding`.

`src/Frontend/ProjectMemberModel.cpp` — pase nuevo justo después del chequeo de nombres de export
duplicados (mismo lugar, mismo estilo). Recorre `members_` una vez y:

1. agrupa por `upper(dll) + "!" + symbol`; si el mismo par aparece con **firma distinta**, emite el
   conflicto + nota a la declaración previa;
2. agrupa las librerías por nombre de símbolo;
3. para todo símbolo reclamado por ≥2 librerías: si una de las declaraciones no tiene librería
   (`extern C {}`, resolución estática por nombre) → error, porque un símbolo de archive no se
   puede renombrar; si no → marca `importRequiresRuntimeBinding` en esos miembros.

Helper nuevo `upperAsciiText` en el namespace anónimo, para que los nombres de DLL comparen sin
distinguir mayúsculas igual que ya lo hace el planificador de import libraries.

## Paso 6 — C1.2b: propagar la identidad por HIR/MIR

`ProjectOopHirModel.h` / `ProjectOopMirModel.h` / `ProjectOopMirModel.cpp` — el flag viaja con el
resto de los campos del import.

`src/Frontend/ProjectOopHirModel.cpp` — helper nuevo `importIdentityInternalName(dll, symbol)` en el
namespace anónimo (reusa el `sanitize` que ya estaba ahí), y la separación explícita de los dos
nombres del import:

- `cAbiName` = **siempre** el símbolo externo (lo que exporta la DLL o el archive);
- `mangledName` = el nombre **interno** que usan el módulo y todos los call sites.

En el caso ordinario coinciden —y eso es justamente lo que hace que el enlace resuelva el import
estáticamente por nombre, sin cambios—. Sólo divergen cuando dos librerías reclaman el nombre.

## Paso 7 — C1.2c: qué entra a la import library

`src/Driver/Driver.cpp`, `collectExternalImports` — tres cambios:

1. un import con `importRequiresRuntimeBinding` **no entra** a la import library (no hay slot que
   pueda compartir);
2. el símbolo que entra es `cAbiName`, nunca `mangledName` — coinciden en el caso estático, y esta
   línea es la que evita confundirlos si algún día dejan de coincidir;
3. si hubo al menos un import ligado en runtime, se piden `LoadLibraryA` y `GetProcAddress` **por la
   misma puerta** que los imports del proyecto, así que la tabla única del runtime los fusiona y
   deduplica. Un proyecto sin nombres ambiguos no los importa nunca.

## Paso 8 — C1.2d: el cuerpo del import ligado en runtime

`src/LLVM/ProjectOopLlvmLowerer.cpp` — `defineRuntimeBoundImport`, más dos helpers:

- `cStringGlobal(text, stem)`: un array de bytes NUL-terminado, cacheado por contenido. A propósito
  **no** es la forma de string literal de NativeSharp (longitud + bytes): `LoadLibraryA` y
  `GetProcAddress` quieren un C string y nada más.
- reutiliza `emitGuard(...)` para el trap de binding fallido, y `applySretAttributes` para replicar
  el atributo `sret` en el call site de reenvío.

Antes de llegar a esto se **midieron y descartaron** tres caminos de linker que prometían resolver
la colisión sin cuerpo (`.def` con `alias == real` en `lld-link`, la palabra clave `EXPORTAS`, y un
short import record escrito a mano con `NameType = IMPORT_NAME_EXPORTAS`): ninguno produce un import
por nombre con símbolo renombrado en esta versión de LLD. El detalle de cada medición está en el
documento conceptual.

`include/nativesharp/LLVM/LlvmTrapReason.h` — `ImportBindingFailure = 12` (exit 140).

## Paso 9 — C1.3: el borde de excepciones

`include/nativesharp/LLVM/LlvmTrapReason.h` — `CallbackExceptionEscape = 13` (exit 141).

`src/LLVM/ProjectOopLlvmLowerer.cpp`:

- `emitCallbackBoundaryHandler(error)` — la mitad *handler*, compartida: un `catchswitch` que
  `unwind to caller`, un `catchpad` filtrado por `__nativesharp_eh_filter`, la llamada al trap con
  su bundle `funclet`, y `unreachable`.
- `callbackGuardThunk(targetName, error)` — la mitad *thunk*: una función con la firma exacta del
  objetivo, personality SEH, `invoke` al objetivo, `ret` en el camino normal, el handler en la
  arista de unwind. Cachea por nombre de objetivo (`callbackGuardThunks_`) para que dos
  conversiones del mismo método devuelvan **el mismo puntero**.
- `defineCallbackGuardThunks()` — pase nuevo en el pipeline, **antes** de `defineFunctions`. Recorre
  todos los cuerpos MIR y los inicializadores de campo buscando nodos `DelegateCreate` cuyo
  `typeKey` sea un `extern C delegate`, y emite un thunk por objetivo distinto. Se hace como pase
  previo y no dentro de `lowerDelegateCreate` para no tener que salvar y restaurar el estado del
  builder en medio de la bajada de otro cuerpo.
- `lowerDelegateCreate` — para un `extern C delegate`, devuelve el thunk en vez de la función.
- `defineCAbiWrappers` — el `call` a la función interna pasó a ser `invoke` + el mismo handler, y al
  wrapper se le pone la personality SEH. Es la otra puerta por la que entra código foráneo.

## Paso 10 — Deduplicación de identidades idénticas

Encontrado por la **propia suite de esta ronda**, no en revisión: el escenario `SameIdentityTwice`
(dos clases envolviendo `kernel32!GetCurrentProcessId`) fallaba con
`lld-link: error: undefined symbol: GetCurrentProcessId.1`. El pase del Paso 5 lo aceptaba
correctamente como **un** import, pero el backend seguía pidiéndole a LLVM el mismo nombre dos
veces.

Arreglo en `declareFunctions`: para `isImportedC`, si el nombre ya está en `functions_`, se reusa la
declaración existente. Cada miembro conserva su propio registro MIR y simplemente resuelve a la
declaración compartida. Lo mismo en `defineFunctions` con un `std::unordered_set` para no darle
cuerpo dos veces al mismo import ligado en runtime.

## Paso 11 — Tests

`tests/unit/Stage56C1InteropCompletionTests.cpp` — suite nueva, **92 checks**, 18 escenarios:

- round trip por `nint` y por `void*`, con asserts de forma de IR positivos **y negativos**
  (`ptrtoint`/`inttoptr` presentes, `delegate.code` ausente);
- reinterpretación explícita entre firmas + su control negativo (la conversión implícita sigue
  rechazada);
- retorno de function pointer desde un import (`declare ptr @wglGetProcAddress(ptr)`) y desde un
  export (`define ptr @GetAdder()`), más function pointer como parámetro;
- los dos negativos que antes eran silencio (delegate ordinario, referencia de clase);
- dos librerías con el mismo símbolo: IR con las dos identidades, sus dos cachés, `LoadLibraryA`/
  `GetProcAddress`, **ausencia** de `@Init.1`, y **enlace real** a un `.exe`;
- regresión del camino estático: sin `LoadLibraryA`, sin `__nsc_import_`, PE importando `user32.dll`;
- firmas conflictivas → rechazo; mismo triple dos veces → aceptado y **una** declaración;
- símbolo de archive compartiendo nombre con una DLL → rechazo;
- thunk de borde presente, con su filtro, su trap y su `unwind to caller`; **una sola** definición
  de thunk por objetivo; la excepción en el callback esperando exit 141; y el borde del export.

`tests/unit/Stage47HostCAbiTests.cpp` — el caso negativo `ExternDelegateReturn` era exactamente la
regla que C1.1 cambia a propósito. Se reemplazó por `ManagedDelegateReturn` (delegate ordinario),
que es lo que **sigue** rechazado, con el comentario explicando el cambio de contrato.

`tests/interop_contract/` — carpeta nueva pedida por la sección 5 del roadmap: los casos
contractuales como proyectos NativeSharp reales, no como literales dentro de un `.cpp`. Cinco
fixtures (casos 3, 4, 5, 13 y 14) más un `README.md` con el estado de las 15 filas de la matriz. La
suite los compila desde el árbol de fuentes vía `NSC_SOURCE_DIR`, así que no pueden pudrirse en
decoración.

`examples/project_stage56_c1/` — ejemplo empaquetado, misma convención que
`examples/project_stage55_1/`: cada chequeo fallido devuelve su propio código de salida y sólo si
todos pasan se llega al `return 0`. La suite también lo compila a `-O0` y `-O2`.

`CMakeLists.txt` — registro del ejecutable de test, con `NSC_SOURCE_DIR` (lo necesita para las
fixtures y el ejemplo) y `nsc_enable_warnings`.

## Paso 12 — Verificación final

1. Reconfiguración + build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
2. `ctest` completo: **233/233**.
3. Manifest regenerado y verificado.
4. Extracción aislada del ZIP completo: configure + build + `ctest` desde cero.
