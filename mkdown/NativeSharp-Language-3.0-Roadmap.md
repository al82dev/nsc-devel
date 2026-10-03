# NativeSharp Language 3.0 — Roadmap del lenguaje / Language Roadmap

> **Estado:** propuesta de planificación  
> **Plataforma normativa inicial:** Windows 64-bit (`x86_64-pc-windows-msvc`)  
> **Filosofía:** semántica nativa tipo C++, ergonomía familiar para C#, costo predecible, control explícito de memoria  
> **No depende de:** CLR, GC, metadata/reflection administrada, JIT ni runtime obligatorio de objetos

---

## 0. Propósito / Purpose

NativeSharp 3.0 debe cerrar la etapa de **lenguaje nativo completo y expresivo**. NativeSharp 2.0 establece la base funcional: compilación incremental y paralela, MIR maduro, referencias y análisis de escape/lifetimes, placement construction, `noheap`, atomics, `asm`, SIMD, control de punto flotante, `comptime`/`#run`, ABI formal, sanitizers y optimización/enlace avanzados. La existencia funcional de esas capacidades no demuestra todavía que su costo, observabilidad o experiencia de diagnóstico sean aceptables. Antes de iniciar la Tanda 00 de 3.0, la base 2.0 debe completar y certificar el roadmap oficial `NativeSharp-2.0-Compiler-Performance-and-Diagnostics-Roadmap.md`, enlazado en la sección 2.3. La serie 3.0 no repite esas bases: las recibe medidas, estabilizadas y construye sobre ellas.

El objetivo de 3.0 es permitir que código de sistemas grande pueda expresar abstracciones modernas sin ocultar asignaciones, copias, sincronización, destrucción ni cambios de ABI. La ergonomía se inspira en C#, pero la semántica sigue siendo nativa, estática y compilada ahead-of-time.

### Resultado esperado / Expected outcome

Al promover 3.0, el lenguaje debe ofrecer:

- un sistema de tipos coherente para genéricos, traits, tipos algebraicos, tuplas y funciones;
- pattern matching exhaustivo y de costo visible;
- ownership, préstamo y destrucción determinista utilizables en código cotidiano;
- manejo de errores sin excepciones administradas ni asignación implícita;
- metaprogramación tipada y reproducible;
- módulos, visibilidad y evolución de API definidos por especificación;
- coroutines y concurrencia con semántica de lenguaje, sin imponer un scheduler;
- control explícito de layout, ABI, punteros, vectores y código inseguro;
- una especificación bilingüe congelable y una suite de conformidad Win64.

---

## 1. Principios no negociables / Non-negotiable principles

1. **Costo visible / Visible cost.** Ninguna feature puede introducir heap, boxing, locks, RTTI, copias profundas o despacho dinámico de forma invisible.
2. **Cero CLR / No CLR.** Los binarios no requieren CLR, CoreCLR, IL, metadata administrada ni recolector de basura.
3. **Destrucción determinista / Deterministic destruction.** El final de vida y el orden de destrucción forman parte de la semántica observable.
4. **Seguro cuando se pide / Safety when requested.** El subconjunto seguro rechaza usos inválidos; `unsafe` delimita las operaciones que el compilador no puede demostrar.
5. **Interoperabilidad nativa / Native interoperability.** Win64 y C ABI son contratos verificables, no aproximaciones.
6. **Abstracciones de costo cero / Zero-cost abstractions.** Genéricos, traits, ranges, pattern matching y coroutines deben bajar a representaciones nativas inspeccionables.
7. **Diagnóstico determinista / Deterministic diagnostics.** El mismo programa, versión y configuración producen el mismo resultado y las mismas categorías de error.
8. **Evolución controlada / Controlled evolution.** Las rupturas exigen edición de lenguaje, guía de migración y diagnóstico accionable.
9. **Implementación independiente / Independently implementable.** Cada ítem del roadmap tiene un ID, un commit lógico propio y criterios de aceptación aislables.
10. **Depurabilidad semántica / Semantic debuggability.** Toda feature define desde su propia tanda qué nombres, valores, scopes, lifetimes, sequence points y estados optimizados publica. Un PDB que contiene nombres pero muestra valores falsos no es conforme.

---

## 2. Alcance y frontera / Scope and boundary

### 2.1 Incluido en NativeSharp Language 3.0 / In scope

- gramática, parser y reglas de nombres;
- sistema de tipos, conversiones y overload resolution;
- semántica de ownership, lifetimes, moves, drops y `unsafe`;
- genéricos, constraints, traits y despacho;
- tuplas, variants, pattern matching y nuevos operadores;
- errores, control de flujo, lambdas, closures y coroutines;
- metaprogramación que forma parte de la semántica del programa;
- módulos y visibilidad como conceptos del lenguaje;
- modelo de memoria, layout, ABI y FFI expresados por el lenguaje;
- reglas normativas de lowering cuando afectan comportamiento observable;
- nombres, lifetimes y sequence points observables que el backend debe preservar para una depuración conforme;
- especificación, pruebas de conformidad y política de compatibilidad del lenguaje.

### 2.2 Fuera del lenguaje / Explicitly outside the language

| Área | Pertenece a | Relación con 3.0 |
|---|---|---|
| Colecciones, strings, filesystem, sockets | SDK/biblioteca estándar | Pueden aprovechar traits, ranges y allocators, pero no definen la feature |
| Allocator concreto, GC opcional, arenas | Runtime/SDK | El lenguaje solo define protocolos, efectos y puntos de asignación |
| Scheduler, thread pool, event loop | Runtime/SDK | Coroutines no prescriben scheduler |
| Driver de compilación, package manager, build graph | Toolchain | Consumen módulos y metadata de compilación |
| IDE, LSP, formatter, debugger UI | Tooling | Deben entender 3.0, pero no bloquean la semántica salvo en gates explícitos |
| Linker, PDB writer, incremental linker | Toolchain/backend | Sujetos a contratos ABI/debug del lenguaje |
| Windowing, input, graphics, audio | SDK/plataforma | Fuera del roadmap del lenguaje |
| Reflection con registro global de tipos | Runtime | No se añade; solo introspección estática en `comptime` |

Una tarea auxiliar de SDK/runtime/toolchain puede aparecer como **dependencia de integración**, pero nunca se contabiliza como feature de lenguaje 3.0.

La interfaz del debugger permanece fuera del lenguaje. No obstante, el significado observable de
un local, su comienzo y fin de vida, el nombre fuente de un binding, los puntos lógicos de parada y
la clasificación de código generado sí forman parte del contrato semántico que el compilador debe
entregar al backend. La sección 11 consolida el contrato PDB/Debug transversal.

### 2.3 Punto de partida vigente de NSC 3.0 / Current NSC 3.0 starting point

NativeSharp 3.0 se desarrolla directamente sobre `development`. El antiguo gate de NSC 2.0 y el roadmap [NativeSharp-2.0-Compiler-Performance-and-Diagnostics-Roadmap.md](NativeSharp-2.0-Compiler-Performance-and-Diagnostics-Roadmap.md) se conservan únicamente como evidencia histórica; ya no bloquean `NS3-T00` ni autorizan cambios sobre la línea 2.0.

El milestone local `v.3.0.2` permanece congelado. Todo cambio posterior se implementa en `development`, con pruebas dirigidas por tanda y sin instalar artefactos experimentales en `C:\native-sharp`. La coordinación vigente entre lenguaje, compilador, linker, debugger y VS Code se define en la sección 12 de este documento.

---

## 3. Convenciones de ejecución / Execution conventions

### 3.1 Identificadores y commits / IDs and commits

- Tanda: `NS3-TNN`.
- Feature: `NS3-TNN-FMM`.
- Commit lógico: `ns3(tNN-fMM): <slug>`.
- Pruebas: `tests/lang3/tNN/fMM/`.
- Especificación ES: `docs/spec/es/3.0/<slug>.md`.
- Specification EN: `docs/spec/en/3.0/<slug>.md`.

Cada feature debe poder revisarse y revertirse de forma independiente. Si una implementación necesita varios commits físicos, todos conservan el ID y el último integra pruebas y documentación. No se mezclan refactors no relacionados.

### 3.2 Estado de una feature / Definition of done

Una feature solo está terminada cuando incluye:

- gramática y semántica normativa;
- implementación completa desde parser hasta lowering/MIR/backend cuando aplique;
- diagnósticos positivos y negativos con códigos estables;
- pruebas dirigidas, de interacción y de regresión;
- ejemplos y documentación Markdown equivalentes en español e inglés;
- nota de compatibilidad y, si cambia 2.x, migración o fix-it;
- ausencia de heap, RTTI o runtime oculto demostrada cuando se prometa costo cero;
- contrato Debug/PDB de la feature: nombres, tipos, scopes, lifetimes, sequence points, código
  generado y comportamiento O0/O2 aplicable;
- prueba de que ningún valor falso se publica como disponible.

### 3.3 Tipos mínimos de prueba / Minimum test classes

Cada carpeta de feature usa, según corresponda:

- `parse-pass`, `parse-fail`;
- `sema-pass`, `sema-fail`;
- `lowering-mir` con snapshots estructurales;
- `codegen-win64` con checks de ABI/assembly;
- `run-pass`, `run-fail`;
- `compile-fail` con código y span de diagnóstico;
- `interop-c` o `interop-cpp`;
- `consteval`, `layout`, `drop-order`, `borrow`, `concurrency`;
- `debug-mir`, `debug-ir`, `debug-pdb` y `debug-session` cuando la feature produzca estado
  observable, bindings, control de flujo o procedimientos;
- `compat-2x` y `migration`.

### 3.4 Regla de baseline / Baseline rule

Las tandas de desarrollo no ejecutan automáticamente el baseline general. Cada commit construye
lo necesario y corre solamente sus pruebas dirigidas. Las subsecciones “Baseline TNN” expresan
criterios de aceptación acumulados, no autorizan por sí solas una corrida completa.

Un baseline se crea únicamente en un milestone autorizado expresamente. En ese momento deben pasar:

1. todas las pruebas nuevas de las tandas incluidas;
2. toda la suite acumulada 2.x y 3.0;
3. build limpio e incremental en Debug y Release para Win64;
4. determinismo de salida en dos builds desde directorios distintos;
5. cero regresiones ABI o Debug/PDB no aprobadas;
6. paridad de documentación ES/EN y ejemplos compilables.

Hasta esa autorización no se instala ningún artefacto experimental sobre la instalación estable de
C:\native-sharp.

### 3.5 Contrato Debug/PDB transversal / Cross-cutting Debug/PDB contract

Cada feature que cree variables, bindings, tipos, control de flujo, procedimientos o código
generado debe definir y probar desde su tanda:

- qué nombres fuente son visibles y cuáles son internos;
- cuándo comienza y termina la vida observable de cada valor;
- qué sequence points existen y dónde liga un breakpoint;
- cómo se representan parámetros, locals, constantes, capturas y temporales;
- qué ocurre después de move, destrucción, optimización o suspensión;
- cómo se clasifican thunks, helpers, funclets, inline sites y state machines;
- qué garantías ofrece O0 y qué degradaciones honestas admite O2;
- cómo se inspeccionan MIR, LLVM IR, CodeView/PDB y una sesión real.

NS3-T15-F05 es la auditoría final de este contrato. No es el momento en que se implementa por
primera vez la depurabilidad de las features T00–T14.

---

# Tanda 00 — Constitución de 3.0 / 3.0 Language Constitution

**Objetivo:** fijar el terreno normativo antes de añadir sintaxis.

## NS3-T00-F01 — Ediciones de lenguaje / Language editions

**Commit:** `ns3(t00-f01): language-editions`

- **Implementación:** introducir `language = "2" | "3"` en la unidad de compilación; definir que la edición selecciona reglas del frontend, no ABI ni versión de biblioteca; prohibir mezcla silenciosa dentro de un módulo.
- **Pruebas dirigidas:** misma fuente bajo ambas ediciones; imports con edición incompatible; mensaje para sintaxis 3.0 usada en 2.x; build incremental al cambiar edición.
- **Markdown ES/EN:** definición de edición, compatibilidad entre unidades y ejemplos de selección.

## NS3-T00-F02 — Modelo de etapas / Compilation-stage model

**Commit:** `ns3(t00-f02): stage-model`

- **Implementación:** formalizar parse, binding, type checking, `comptime`, MIR, monomorfización y codegen; especificar qué información existe en cada etapa y cuándo se emiten diagnósticos.
- **Pruebas dirigidas:** referencias adelantadas, evaluación estática antes/después de inferencia, diagnósticos sin depender del backend y ciclos de etapa rechazados.
- **Markdown ES/EN:** diagrama normativo de etapas y tabla de operaciones permitidas.

## NS3-T00-F03 — Categorías de valores / Value categories

**Commit:** `ns3(t00-f03): value-categories`

- **Implementación:** definir place, value, temporary, borrowed value y moved-from; unificar reglas de lvalue/rvalue, materialización y lifetime extension heredadas de 2.0.
- **Pruebas dirigidas:** acceso a campos, temporales, retorno por valor, moves parciales, referencias a temporales y orden de materialización.
- **Markdown ES/EN:** terminología oficial y tabla por clase de expresión.

## NS3-T00-F04 — Contextos de efectos / Effect contexts

**Commit:** `ns3(t00-f04): effect-contexts`

- **Implementación:** crear el marco semántico para `unsafe`, `throws`, `async`, `comptime`, `noheap` y futuras restricciones; los efectos son verificables y forman parte del tipo solo donde se indique.
- **Pruebas dirigidas:** operación fuera de contexto, propagación anidada, alias de tipos función y mensajes con el efecto faltante.
- **Markdown ES/EN:** matriz de efectos, reglas de composición y no-objetivos.

## NS3-T00-F05 — Catálogo estable de diagnósticos / Stable diagnostic catalog

**Commit:** `ns3(t00-f05): diagnostic-contract`

- **Implementación:** reservar códigos `NS3xxx` para errores normativos; separar error, warning y lint; spans primario/secundario y notas estructuradas.
- **Pruebas dirigidas:** snapshots por código, estabilidad ante whitespace, múltiples errores recuperables y errores equivalentes en builds incrementales.
- **Markdown ES/EN:** índice bilingüe con causa, ejemplo mínimo y corrección.

## NS3-T00-F06 — Harness de conformidad / Conformance harness

**Commit:** `ns3(t00-f06): conformance-harness`

- **Implementación:** manifest por caso con edición, target, resultado, stdout, diagnósticos y checks MIR/ABI; diferenciar prueba normativa de regresión interna.
- **Pruebas dirigidas:** autocheck del harness, casos inválidos, timeouts, resultados no deterministas y ejecución aislada.
- **Markdown ES/EN:** cómo añadir casos y qué evidencia exige cada clase.

### Baseline T00 / T00 baseline

- Especificación esqueleto 3.0 publicada en ES/EN.
- Ediciones 2 y 3 seleccionables y diagnosticables.
- Categorías de valores y efectos aprobados por revisión de diseño.
- Harness ejecuta la suite 2.x sin alterar resultados.
- Etiqueta: `lang-3.0-t00-baseline`.

---

# Tanda 01 — Expresiones y ergonomía / Expressions and Ergonomics

**Objetivo:** adoptar ergonomía familiar sin introducir semántica oculta.

## NS3-T01-F01 — Argumentos nombrados / Named arguments

**Commit:** `ns3(t01-f01): named-arguments`

- **Implementación:** permitir `Draw(x: 1, color: red)`; nombres participan después de formar el conjunto de overloads; definir orden de evaluación de izquierda a derecha y reglas de mezcla posicional/nombrada.
- **Pruebas dirigidas:** overloads, defaults, genéricos, parámetros renombrados entre módulos, duplicados, orden de efectos y ABI idéntica al llamado posicional.
- **Markdown ES/EN:** sintaxis, estabilidad de nombres públicos y guía para autores de APIs.

## NS3-T01-F02 — Parámetros variádicos tipados / Typed parameter packs

**Commit:** `ns3(t01-f02): typed-params`

- **Implementación:** `params ReadOnlySpan<T>` como azúcar con construcción explícitamente stack/readonly cuando sea posible; no permitir array de heap implícito; añadir packs genéricos estáticos por separado.
- **Pruebas dirigidas:** cero/uno/muchos argumentos, escape, tipos no copiables, forwarding, `noheap`, codegen sin allocation y ambigüedad de overload.
- **Markdown ES/EN:** costos, storage temporal y diferencias frente a C#.

## NS3-T01-F03 — Operadores nulos / Null-aware operators

**Commit:** `ns3(t01-f03): null-aware-operators`

- **Implementación:** `??`, `??=` y `?.` solo para punteros anulables, referencias anulables y tipos que implementen el protocolo estático de optional; short-circuit y evaluación única del operando.
- **Pruebas dirigidas:** punteros, optionals, propiedades con efectos, temporales, encadenamiento, overloads prohibidos y ausencia de boxing.
- **Markdown ES/EN:** truth table, lowering equivalente y distinción entre null y estado vacío.

## NS3-T01-F04 — Operadores de rango e índice / Range and index operators

**Commit:** `ns3(t01-f04): range-index`

- **Implementación:** `a[start..end]`, rangos abiertos y `^n`; resolver mediante protocolos estáticos `Length`/`Slice`; overflow y bounds siguen el modo de seguridad.
- **Pruebas dirigidas:** arrays, spans, tipos usuario, rangos vacíos, negativos inválidos, `^0`, evaluación única y checks eliminados en Release cuando son demostrables.
- **Markdown ES/EN:** semántica half-open, requisitos del protocolo y costos.

## NS3-T01-F05 — Inicializadores generalizados / Generalized initializers

**Commit:** `ns3(t01-f05): generalized-initializers`

- **Implementación:** inicializadores de structs, arrays y colecciones mediante protocolos estáticos; prohibir miembros omitidos no inicializados salvo opt-in explícito; mantener orden textual.
- **Pruebas dirigidas:** campos readonly, destructores durante fallo, tipos no copiables, spreads, duplicados, generic structs y construcción in-place.
- **Markdown ES/EN:** formas admitidas, reglas de inicialización completa y lowering.

## NS3-T01-F06 — Interpolación estática / Static string interpolation

**Commit:** `ns3(t01-f06): static-interpolation`

- **Implementación:** bajar `$"..."` a un handler genérico elegido en compile time; no exigir `String`, heap ni cultura global; literales y holes conservan orden de evaluación.
- **Pruebas dirigidas:** handlers stack-only, formatos constantes, tipos usuario, `noheap`, caracteres escapados, evaluación única y diagnóstico cuando no existe handler.
- **Markdown ES/EN:** contrato del handler, ejemplos allocation-free y diferencias frente a interpolación administrada.

## NS3-T01-F07 — Expresiones `with` y actualización no destructiva / Non-destructive mutation

**Commit:** `ns3(t01-f07): with-expressions`

- **Implementación:** `value with { X = 1 }` para tipos copiables o con protocolo de clone explícito; el costo de copia debe ser visible y rechazable para tipos move-only sin estrategia.
- **Pruebas dirigidas:** POD, fields privados, clone usuario, moves, drops en fallo, genéricos y layout sin wrapper.
- **Markdown ES/EN:** elegibilidad, costos y equivalencia expandida.

### Baseline T01 / T01 baseline

- Toda azúcar tiene lowering documentado y snapshots MIR.
- Suite `noheap` demuestra cero asignaciones implícitas.
- Orden de evaluación único para todas las expresiones nuevas.
- Compatibilidad de llamadas 2.x preservada.
- Etiqueta: `lang-3.0-t01-baseline`.

---

# Tanda 02 — Tuplas, deconstrucción y registros / Tuples, Deconstruction, and Records

**Objetivo:** ofrecer agregados ligeros y composición estructurada sin identidad runtime.

## NS3-T02-F01 — Tipos tupla nativos / Native tuple types

**Commit:** `ns3(t02-f01): native-tuples`

- **Implementación:** `(T1, T2, ...)` como agregado nativo; nombres de elementos no afectan identidad ni ABI; layout sigue reglas de agregados y admite retorno Win64 directo o indirecto según ABI.
- **Pruebas dirigidas:** aridades 0–16 y grandes, nesting, nombres, genéricos, move-only, layout, retorno C-compatible donde proceda y drops parciales.
- **Markdown ES/EN:** identidad, layout, nombres y convenciones ABI.

## NS3-T02-F02 — Literales y conversión de tuplas / Tuple literals and conversions

**Commit:** `ns3(t02-f02): tuple-literals`

- **Implementación:** inferencia contextual para `(a, b)`; conversiones elemento por elemento sin conversión estructural peligrosa; evaluación estricta de izquierda a derecha.
- **Pruebas dirigidas:** inferencia, overloads, conversiones numéricas, referencias, temporales, error por elemento y evaluación con efectos.
- **Markdown ES/EN:** reglas de inferencia y matriz de conversiones.

## NS3-T02-F03 — Deconstrucción / Deconstruction

**Commit:** `ns3(t02-f03): deconstruction`

- **Implementación:** declaraciones y asignaciones `(x, y) = value`; protocolo estático `Deconstruct` para tipos nominales; temporales ocultos no deben copiar valores move-only.
- **Pruebas dirigidas:** swaps, descartes, nesting, aliasing, moves, ref returns, fallo intermedio y orden de escritura.
- **Markdown ES/EN:** binding, temporales y diferencias entre declarar y asignar.

## NS3-T02-F04 — Registros nativos / Native records

**Commit:** `ns3(t02-f04): native-records`

- **Implementación:** `record struct` como generación de miembros puramente estática; igualdad, hash, impresión o clone solo mediante opt-ins/derives explícitos; sin identidad administrada.
- **Pruebas dirigidas:** expansión generada, campos no comparables, layout, visibilidad, genéricos, derives parciales y ABI equivalente a struct.
- **Markdown ES/EN:** miembros sintetizados, opt-ins y garantías de costo.

## NS3-T02-F05 — Campos y parámetros promovidos / Promoted fields and parameters

**Commit:** `ns3(t02-f05): promoted-fields`

- **Implementación:** sintaxis concisa para constructores primarios de structs/records; especificar inicialización, visibilidad y shadowing; no capturar parámetros invisiblemente.
- **Pruebas dirigidas:** orden base/campos, defaults, readonly, nombres duplicados, herencia permitida y depuración de nombres.
- **Markdown ES/EN:** desazucarado completo y reglas de compatibilidad.

## NS3-T02-F06 — Spreads de agregados / Aggregate spreads

**Commit:** `ns3(t02-f06): aggregate-spreads`

- **Implementación:** `(..base, X: 2)` con una sola fuente y campos explícitos; copiar o mover según categoría de valor; prohibir lectura posterior de campos movidos.
- **Pruebas dirigidas:** partial moves, drop flags, campos privados, nesting, duplicados, source temporal y `noheap`.
- **Markdown ES/EN:** precedencia, move/copy y limitaciones.

### Baseline T02 / T02 baseline

- Tuplas y records no requieren tipos runtime preinstalados.
- Layout y retorno Win64 cubiertos por pruebas doradas.
- Moves y destrucción parcial pasan sanitizers.
- Todas las expansiones sintetizadas aparecen en dumps semánticos.
- Etiqueta: `lang-3.0-t02-baseline`.

---

# Tanda 03 — Tipos suma y pattern matching / Sum Types and Pattern Matching

**Objetivo:** modelar estados válidos de forma cerrada, exhaustiva y sin allocation.

## NS3-T03-F01 — Variants etiquetados / Tagged variants

**Commit:** `ns3(t03-f01): tagged-variants`

- **Implementación:** `variant Result<T,E> { Ok(T), Error(E) }`; tag mínimo compatible con alineación; payload in-place; control de layout con restricciones explícitas.
- **Pruebas dirigidas:** variants vacíos/genéricos/recursivos indirectos, payload move-only, drops por caso, niche optimization autorizada y tamaños Win64.
- **Markdown ES/EN:** construcción, identidad, layout observable y restricciones recursivas.

## NS3-T03-F02 — Patterns fundamentales / Fundamental patterns

**Commit:** `ns3(t03-f02): fundamental-patterns`

- **Implementación:** wildcard, literal, binding, tuple, struct, variant, range y OR-pattern; bindings por valor, `ref` o `move` explícitos.
- **Pruebas dirigidas:** nesting profundo, aliasing, moves parciales, rangos solapados, OR-bindings incompatibles y literales float restringidos.
- **Markdown ES/EN:** gramática, modos de binding y ejemplos de ownership.

## NS3-T03-F03 — Expresión `match` exhaustiva / Exhaustive match expression

**Commit:** `ns3(t03-f03): exhaustive-match`

- **Implementación:** `match` produce valor o `void`; algoritmo de utilidad/exhaustividad para dominios cerrados; lowering a branch/switch sin objeto runtime.
- **Pruebas dirigidas:** casos faltantes, brazos inalcanzables, guards, tipos divergentes, retorno de referencias, codegen jump-table y destrucción por brazo.
- **Markdown ES/EN:** exhaustividad, orden de brazos, guards y tipo resultado.

## NS3-T03-F04 — `if let`, `while let` y `let else` / Conditional patterns

**Commit:** `ns3(t03-f04): conditional-patterns`

- **Implementación:** azúcar sobre pattern test; scopes de bindings precisos; `let ... else` exige rama divergente para garantizar inicialización.
- **Pruebas dirigidas:** alcance, loops, borrow extendido, else no divergente, nesting y equivalencia MIR con `match`.
- **Markdown ES/EN:** flujo, alcance y reglas de inicialización.

## NS3-T03-F05 — Guards y refinamiento / Guards and narrowing

**Commit:** `ns3(t03-f05): pattern-guards`

- **Implementación:** guards booleanos evaluados tras match estructural; refinamiento de nullability, variante y rangos enteros dentro del brazo; guard no cuenta para exhaustividad salvo prueba estática.
- **Pruebas dirigidas:** efectos en guard, borrows, cobertura, guards constantes, narrowing y optimización de checks redundantes.
- **Markdown ES/EN:** orden, alcance y límites del refinamiento.

## NS3-T03-F06 — Variants compatibles con C / C-compatible unions

**Commit:** `ns3(t03-f06): c-unions`

- **Implementación:** mantener `union` sin tag solo bajo `unsafe`/layout C, separado de `variant`; acceso requiere miembro activo demostrado o bloque inseguro.
- **Pruebas dirigidas:** headers C equivalentes, tamaño/alineación, endian assumptions, lectura inactiva rechazada en safe y round-trip DLL.
- **Markdown ES/EN:** comparación `union` vs `variant` y contrato FFI.

### Baseline T03 / T03 baseline

- `variant` no asigna ni boxea y destruye exactamente un payload activo.
- Exhaustividad validada por corpus combinatorio.
- `match` produce codegen comparable a switch manual.
- Interop C de uniones validado contra MSVC.
- Etiqueta: `lang-3.0-t03-baseline`.

---

# Tanda 04 — Genéricos 3.0 / Generics 3.0

**Objetivo:** hacer los genéricos expresivos, diagnosticables y aptos para código de sistemas.

## NS3-T04-F01 — Constraints declarativos / Declarative constraints

**Commit:** `ns3(t04-f01): where-constraints`

- **Implementación:** cláusulas `where` para traits, igualdad de tipos, construcción, copy/move, lifetime y constantes; normalización canónica para caché e incrementalidad.
- **Pruebas dirigidas:** constraints redundantes/contradictorios, ciclos, inferencia, overloads, diagnósticos de obligación y módulos separados.
- **Markdown ES/EN:** gramática y catálogo de constraints fundamentales.

## NS3-T04-F02 — Parámetros const genéricos / Const generics

**Commit:** `ns3(t04-f02): const-generics`

- **Implementación:** parámetros compile-time enteros, booleanos, enums y tipos estructurales aprobados; identidad incluye valor normalizado; expresiones deben ser evaluables y deterministas.
- **Pruebas dirigidas:** `Vector<T,N>`, matrices, overflow const, valores equivalentes, mangling Win64, inferencia y límites de instanciación.
- **Markdown ES/EN:** tipos admitidos, identidad y evaluación.

## NS3-T04-F03 — Packs de tipos y valores / Type and value packs

**Commit:** `ns3(t04-f03): generic-packs`

- **Implementación:** packs con expansión léxicamente marcada; índices y fold expressions; prohibir expansión ambigua o recursión no acotada.
- **Pruebas dirigidas:** pack vacío, zip de longitudes, nested packs, forwarding, overload sets y límite de recursos.
- **Markdown ES/EN:** expansión, folds y mensajes de error.

## NS3-T04-F04 — Inferencia bidireccional / Bidirectional inference

**Commit:** `ns3(t04-f04): bidirectional-inference`

- **Implementación:** usar tipo esperado sin inferir a través de conversiones arbitrarias; algoritmo ordenado y terminante; escape hatch con argumentos explícitos.
- **Pruebas dirigidas:** lambdas, tuples, variants, overloads, const params, casos ambiguos y estabilidad ante candidatos irrelevantes.
- **Markdown ES/EN:** algoritmo, límites y consejos de desambiguación.

## NS3-T04-F05 — Alias de tipo y alias opacos / Type aliases and opaque aliases

**Commit:** `ns3(t04-f05): type-aliases`

- **Implementación:** separar alias transparente de `opaque type`; este último tiene identidad nominal y representación controlada por su módulo, sin wrapper runtime.
- **Pruebas dirigidas:** conversiones, visibilidad, layout interno/externo, genéricos, FFI explícito y ciclos de alias.
- **Markdown ES/EN:** identidad, encapsulación y compatibilidad ABI.

## NS3-T04-F06 — Especialización controlada / Controlled specialization

**Commit:** `ns3(t04-f06): specialization`

- **Implementación:** permitir implementación más específica solo con orden parcial demostrable y opt-in; rechazar overlaps incoherentes entre módulos; no cambiar semántica al importar código.
- **Pruebas dirigidas:** diamond overlaps, downstream impls, constraints equivalentes, selección estable y builds separados.
- **Markdown ES/EN:** reglas de coherencia y versión inicial conservadora.

## NS3-T04-F07 — Límites de monomorfización / Monomorphization limits

**Commit:** `ns3(t04-f07): generic-resource-limits`

- **Implementación:** detección de recursión infinita, presupuestos configurables y backtrace compacto de instanciación; los límites no alteran programas válidos dentro del perfil normativo.
- **Pruebas dirigidas:** explosión exponencial, recursión directa/indirecta, caché incremental y diagnóstico reproducible.
- **Markdown ES/EN:** límites mínimos garantizados y cómo reducir instanciaciones.

### Baseline T04 / T04 baseline

- Solver de constraints termina en todo el corpus normativo.
- Mangling e identidad de instancias son estables en Win64.
- No existe selección de especialización dependiente del orden de imports.
- Builds incrementales reutilizan instancias sin contaminación entre ediciones.
- Etiqueta: `lang-3.0-t04-baseline`.

---

# Tanda 05 — Traits, interfaces estáticas y despacho / Traits, Static Interfaces, and Dispatch

**Objetivo:** abstraer comportamiento con despacho estático por defecto y dinámico explícito.

## NS3-T05-F01 — Declaraciones trait / Trait declarations

**Commit:** `ns3(t05-f01): traits`

- **Implementación:** traits con métodos requeridos/default, propiedades estáticas y requisitos de operador; no tienen layout ni instancia por sí mismos.
- **Pruebas dirigidas:** defaults, conflictos, herencia de traits, visibilidad, genéricos y requisitos incompletos.
- **Markdown ES/EN:** modelo, sintaxis y diferencia frente a clases/interfaces CLR.

## NS3-T05-F02 — Implementaciones y coherencia / Implementations and coherence

**Commit:** `ns3(t05-f02): impl-coherence`

- **Implementación:** bloques `impl Trait for Type`; regla orphan o alternativa sellada; unicidad global verificable con módulos compilados por separado.
- **Pruebas dirigidas:** impl duplicado, orphan, impl condicional, módulo downstream, privacidad y mensajes de candidatos.
- **Markdown ES/EN:** ownership de impls y compatibilidad de bibliotecas.

## NS3-T05-F03 — Tipos y constantes asociados / Associated types and constants

**Commit:** `ns3(t05-f03): associated-items`

**Estado 2026-10-03:** `TERMINADA`. Proyecciones, defaults genéricos, igualdad, ciclos,
constantes y round-trip NSMETA 1.29 están cubiertos por 16 checks dirigidos O0/NSLink.

- **Implementación:** proyecciones `T.Item`, normalización por solver y constraints de igualdad; evitar ciclos y proyecciones ambiguas.
- **Pruebas dirigidas:** iteradores, proyecciones encadenadas, defaults, ciclos, const associated y diagnóstico de normalización.
- **Markdown ES/EN:** resolución y ejemplos de APIs genéricas.

## NS3-T05-F04 — Existenciales opacos / Opaque result types

**Commit:** `ns3(t05-f04): opaque-result-types`

- **Implementación:** `some Trait` con un tipo concreto único por declaración; tamaño conocido en el punto de compilación consumidor mediante metadata nativa, sin boxing.
- **Pruebas dirigidas:** múltiples returns, branches incompatibles, lifetimes capturados, módulos, recursión y ABI versioning.
- **Markdown ES/EN:** identidad, captura y límites públicos.

## NS3-T05-F05 — Objetos trait explícitos / Explicit trait objects

**Commit:** `ns3(t05-f05): trait-objects`

- **Implementación:** `dyn Trait` como fat pointer o representación declarada; object-safety normativa; creación y destrucción no asignan por sí mismas.
- **Pruebas dirigidas:** vtable layout, referencias/owning wrappers, métodos genéricos rechazados, upcast seguro, nullability y DLL boundary.
- **Markdown ES/EN:** costo de dispatch, layout y object-safety.

## NS3-T05-F06 — Extensiones estáticas / Extension methods

**Commit:** `ns3(t05-f06): extension-methods`

- **Implementación:** métodos de extensión importables, sin mutar el tipo; miembros reales ganan; resolución independiente del orden textual y con ambigüedad explícita.
- **Pruebas dirigidas:** namespaces/módulos, dos extensiones rivales, genéricos, ref receiver, visibilidad y no-ADL accidental.
- **Markdown ES/EN:** lookup, prioridad y recomendaciones de API.

## NS3-T05-F07 — Operadores mediante traits / Trait-based operators

**Commit:** `ns3(t05-f07): operator-traits`

- **Implementación:** mapear operadores a traits lang-item estables, preservando reglas especiales de built-ins; short-circuit no sobrecargable salvo protocolo dedicado futuro.
- **Pruebas dirigidas:** tipos usuario, promotion numérica, referencias, ambigüedad, consteval y codegen inline.
- **Markdown ES/EN:** tabla operador-trait y restricciones.

### Baseline T05 / T05 baseline

- Despacho estático no introduce vtables.
- `dyn` tiene layout y ABI versionados, con costos documentados.
- Coherencia produce el mismo resultado en build limpio/incremental.
- Ecosistema de traits fundamental puede definirse sin privilegios internos salvo lang-items listados.
- Etiqueta: `lang-3.0-t05-baseline`.

---

# Tanda 06 — Ownership, préstamos y destrucción / Ownership, Borrowing, and Destruction

**Objetivo:** convertir las bases de lifetimes 2.0 en un modelo diario, completo y diagnosticable.

## NS3-T06-F01 — Tipos move-only y `Copy` explícito / Move-only types and explicit Copy

**Commit:** `ns3(t06-f01): move-only-defaults`

**Estado:** `TERMINADA` (2026-10-03). Contrato y migración: `docs/3.0/language/move-only-copy.md`;
evidencia del commit: `docs/3.0/items/NS3-T06-F01-move-only-defaults.md`.

- **Implementación:** formalizar copy implícito solo para tipos que satisfacen `Copy`; el resto se mueve; estados moved-from no son observables salvo reasignación/destrucción permitida.
- **Pruebas dirigidas:** parámetros, returns, branches, loops, fields, arrays, generics y diagnóstico use-after-move.
- **Markdown ES/EN:** matriz copy/move y ejemplos de migración desde 2.x.

## NS3-T06-F02 — Moves parciales y drop flags / Partial moves and drop flags

**Commit:** `ns3(t06-f02): partial-moves`

**Estado:** `TERMINADA` (2026-10-03). Contrato normativo:
`docs/3.0/language/partial-moves-drop-flags.md`; evidencia del commit:
`docs/3.0/items/NS3-T06-F02-partial-moves.md`.

- **Implementación:** análisis campo por campo; drop flags solo cuando sean necesarios y optimizables; tipos con destructor personalizado pueden restringir moves parciales.
- **Pruebas dirigidas:** structs nested, match, excepciones nativas si existen, early returns, loops y conteo exacto de destructores.
- **Markdown ES/EN:** estados de inicialización y costo potencial de flags.

## NS3-T06-F03 — Reborrow y préstamos divididos / Reborrowing and split borrows

**Commit:** `ns3(t06-f03): reborrow-split`

**Estado:** `TERMINADA` (2026-10-03). Contrato normativo:
`docs/3.0/language/reborrow-split-borrows.md`; evidencia del commit:
`docs/3.0/items/NS3-T06-F03-reborrow-split.md`.

- **Implementación:** reborrow implícito de referencias mutables cuando no transfiere ownership; préstamos disjuntos de campos/índices demostrables; fallback conservador.
- **Pruebas dirigidas:** campos hermanos, arrays con índices const, slices, loops, calls, aliasing y rechazo de solapamiento.
- **Markdown ES/EN:** reglas gráficas de exclusividad y duración.

## NS3-T06-F04 — Lifetimes nombrados e inferidos / Named and inferred lifetimes

**Commit:** `ns3(t06-f04): lifetime-parameters`

**Estado:** `TERMINADA` (2026-10-03). Contrato normativo:
`docs/3.0/language/named-inferred-lifetimes.md`; evidencia del commit:
`docs/3.0/items/NS3-T06-F04-lifetime-parameters.md`.

- **Implementación:** parámetros de lifetime en APIs donde la elisión es ambigua; reglas de elisión simples; constraints outlives integrados al solver.
- **Pruebas dirigidas:** retorno ligado a receiver/parámetro, structs prestados, traits, closures, HRTB limitado y escapes a global.
- **Markdown ES/EN:** sintaxis, elisión y mensajes de corrección.

## NS3-T06-F05 — Destructores, `defer` y orden de salida / Destructors, defer, and exit order

**Commit:** `ns3(t06-f05): deterministic-cleanup`

**Estado:** `TERMINADA` (2026-10-03). Contrato normativo:
`docs/3.0/language/deterministic-cleanup-and-defer.md`; evidencia del commit:
`docs/3.0/items/NS3-T06-F05-deterministic-cleanup.md`.

- **Implementación:** un único modelo de cleanup para scope exit, `return`, `break`, `continue`, propagation y panic nativo; `defer` captura por referencia/valor de forma explícita.
- **Pruebas dirigidas:** nesting, múltiples exits, captures move-only, unwind/abort profiles, goto restringido y snapshots MIR.
- **Markdown ES/EN:** orden exacto y diferencias entre destructor y `defer`.

## NS3-T06-F06 — Pinning y direcciones estables / Pinning and stable addresses

**Commit:** `ns3(t06-f06): pinning`

- **Implementación:** `pin ref T`/protocolo equivalente para objetos que no pueden moverse; construcción in-place y proyección segura de campos aprobados.
- **Pruebas dirigidas:** self-references, coroutines, placement, containers, moves rechazados y destrucción in-place.
- **Markdown ES/EN:** invariantes, operaciones seguras e integración con FFI.

## NS3-T06-F07 — Análisis de escape interprocedural / Interprocedural escape analysis

**Commit:** `ns3(t06-f07): escape-summaries`

- **Implementación:** summaries de escape en metadata de módulo; genéricos y funciones externas conservadores; invalidación incremental al cambiar contrato.
- **Pruebas dirigidas:** DLL/imports, callbacks, ref fields, closures, specialization, metadata corrupta y compatibilidad 2.x.
- **Markdown ES/EN:** garantías públicas y límites del análisis.

## NS3-T06-F08 — Drop glue genérico e indirecto / Generic and indirect drop glue

**Commit:** `ns3(t06-f08): generic-drop-glue`

- **Implementación:** emitir y versionar thunks de destrucción a partir de layout y metadata; cubrir arrays, variants, genéricos y tipos importados; garantizar una sola destrucción a través de fronteras DLL.
- **Pruebas dirigidas:** especializaciones locales/importadas, arrays parciales, variants activos, metadata corrupta, unload y conteo exacto de destrucciones cross-module.
- **Markdown ES/EN:** identidad del thunk, ownership del storage y reglas de compatibilidad NSMETA.

### Baseline T06 / T06 baseline

- Corpus de ownership pasa sin falsos negativos conocidos.
- Todo path destruye cada objeto inicializado exactamente una vez.
- Sanitizers no detectan use-after-move, double-drop ni dangling refs en suite positiva.
- Metadata de lifetimes tiene versión y prueba de compatibilidad.
- Etiqueta: `lang-3.0-t06-baseline`.

---

# Tanda 07 — Manejo de errores y control de flujo / Error Handling and Control Flow

**Objetivo:** errores explícitos, componibles y aptos para código sin runtime pesado.

## NS3-T07-F01 — Protocolo `Result` y operador `?` / Result protocol and propagation

**Commit:** `ns3(t07-f01): result-propagation`

- **Implementación:** `?` trabaja mediante trait/protocolo estático sobre variants compatibles; conversión de error explícita; lowering a branches y early return.
- **Pruebas dirigidas:** payload move-only, conversión de error, `defer`, loops, coroutines, `noheap` y MIR sin unwind oculto.
- **Markdown ES/EN:** contrato del protocolo y expansión equivalente.

## NS3-T07-F02 — Funciones `throws` nativas / Native throwing functions

**Commit:** `ns3(t07-f02): typed-throws`

- **Implementación:** `throws E` como efecto tipado con ABI definida; el perfil base puede bajar a resultado/retorno dual, no a excepciones CLR; llamadas deben manejar o propagar.
- **Pruebas dirigidas:** varios tipos de error, function pointers, overrides, FFI prohibido sin adapter, cleanup y tamaño/calling convention.
- **Markdown ES/EN:** semántica, ABI y comparación con `Result`.

## NS3-T07-F03 — `try`, `catch` tipado y `finally` / Typed try-catch-finally

**Commit:** `ns3(t07-f03): typed-catch`

- **Implementación:** captura solo errores declarados; patterns en catch; `finally` comparte el modelo de cleanup; no existe búsqueda por RTTI administrado.
- **Pruebas dirigidas:** exhaustividad, rethrow, return en finally restringido, destructor order, nested try y codegen sin heap.
- **Markdown ES/EN:** flujo, precedence y perfiles de implementación.

## NS3-T07-F04 — Expresiones divergentes y tipo `never` / Diverging expressions and never type

**Commit:** `ns3(t07-f04): never-type`

- **Implementación:** tipo `never` coercible a cualquier tipo resultado; usado por `panic`, loops infinitos demostrados, `return` y branches imposibles.
- **Pruebas dirigidas:** inference, overloads, match, generic constraints, unreachable diagnostics y layout sin valores.
- **Markdown ES/EN:** inhabitation, coerciones y control flow.

## NS3-T07-F05 — Loop expressions y break con valor / Loop expressions

**Commit:** `ns3(t07-f05): loop-values`

- **Implementación:** `loop { break value; }`; unificar tipos de breaks, lifetimes y cleanup; `while`/`for` permanecen `void` salvo decisión explícita.
- **Pruebas dirigidas:** múltiples breaks, never, references, destructores, labels, nested loops e inferencia.
- **Markdown ES/EN:** tipo de loop, labels y ejemplos.

## NS3-T07-F06 — Assertions y panic profiles / Assertions and panic profiles

**Commit:** `ns3(t07-f06): panic-semantics`

- **Implementación:** definir `assert`, `unreachable`, `assume` y `panic`; perfiles `abort` y `unwind-native`; `assume` inseguro cuando una premisa falsa cause UB.
- **Pruebas dirigidas:** Debug/Release, cleanup por perfil, optimización, source location, FFI boundary y handlers custom.
- **Markdown ES/EN:** garantías, UB y contrato del handler.

### Baseline T07 / T07 baseline

- Propagación de errores funciona en `noheap` y no depende de RTTI.
- Orden de cleanup coincide entre `Result`, `throws` y salidas normales.
- ABI de errores tipados congelada de forma provisional para preview.
- Casos de panic cruzando FFI se rechazan o adaptan explícitamente.
- Etiqueta: `lang-3.0-t07-baseline`.

---

# Tanda 08 — Funciones, lambdas y closures / Functions, Lambdas, and Closures

**Objetivo:** funciones como valores nativos con captura y ABI transparentes.

## NS3-T08-F01 — Tipos función completos / Complete function types

**Commit:** `ns3(t08-f01): function-types`

- **Implementación:** tipos incluyen parámetros, retorno, calling convention, variadicidad, efectos relevantes, safety y nullability; sintaxis uniforme para pointers y references a función.
- **Pruebas dirigidas:** asignabilidad, overloads, DLL imports, `throws`, `unsafe`, generics y mangling.
- **Markdown ES/EN:** identidad de tipo y tabla de compatibilidad.

## NS3-T08-F02 — Lambdas sin captura / Non-capturing lambdas

**Commit:** `ns3(t08-f02): noncapturing-lambdas`

- **Implementación:** conversión a function pointer con calling convention conocida; inferencia contextual; cada lambda tiene tipo nominal anónimo zero-size cuando no se convierte.
- **Pruebas dirigidas:** overloads, generic lambdas, consteval, C callbacks, static locals y codegen directo.
- **Markdown ES/EN:** conversiones y garantías de tamaño.

## NS3-T08-F03 — Closures con captura explícita / Explicit-capture closures

**Commit:** `ns3(t08-f03): closures`

- **Implementación:** listas de captura por copy/move/ref; environment como struct nativo; lifetime/escape verificados; invocación mediante traits `Fn`/`FnMut`/`FnOnce` equivalentes.
- **Pruebas dirigidas:** captures mixtas, move-only, nested closure, retorno inválido, `noheap`, layout y drop order.
- **Markdown ES/EN:** expansión del environment y costo de cada modo.

## NS3-T08-F04 — Lambdas genéricas / Generic lambdas

**Commit:** `ns3(t08-f04): generic-lambdas`

- **Implementación:** parámetros de tipo/const explícitos o `auto` restringido; monomorfización como callable genérico, sin erased dispatch automático.
- **Pruebas dirigidas:** inference, constraints, packs, recursion, captures y diagnósticos de instanciación.
- **Markdown ES/EN:** sintaxis y relación con traits callable.

## NS3-T08-F05 — Captura de `this` y métodos como valores / Receiver capture and method values

**Commit:** `ns3(t08-f05): method-values`

- **Implementación:** distinguir method group, bound method y function pointer; binding del receiver por ref/move explícito; no crear delegate administrado.
- **Pruebas dirigidas:** overloads, virtual/dyn dispatch, receiver temporal, const/mut, lifetimes y tamaño del bound callable.
- **Markdown ES/EN:** formas, ownership y representación.

## NS3-T08-F06 — Tail calls y atributos de llamada / Tail calls and call attributes

**Commit:** `ns3(t08-f06): call-attributes`

- **Implementación:** atributo verificable `must_tail`; hints `inline`/`noinline` no cambian semántica; error si ABI o cleanup impide tail call requerida.
- **Pruebas dirigidas:** recursion, destructores pendientes, calling conventions, generics y assembly Win64.
- **Markdown ES/EN:** requisitos fuertes frente a hints.

### Baseline T08 / T08 baseline

- Closures no asignan salvo que el usuario las coloque en storage asignado.
- Layout de environments visible en dumps y estable dentro de una ABI.
- Callbacks C y métodos bound tienen adaptadores explícitos.
- Borrow checker cubre escapes de closures entre módulos.
- Etiqueta: `lang-3.0-t08-baseline`.

---

# Tanda 09 — Iteración, ranges y consultas / Iteration, Ranges, and Queries

**Objetivo:** composición de secuencias mediante protocolos estáticos y fusionables.

## NS3-T09-F01 — Protocolo `foreach` 3.0 / Foreach protocol 3.0

**Commit:** `ns3(t09-f01): foreach-protocol`

- **Implementación:** resolución por traits de iteración; iteración por valor/ref/mut explícita; cleanup del iterator en toda salida.
- **Pruebas dirigidas:** arrays, spans, usuario, ref iteration, move-only, break/error y eliminación de bounds.
- **Markdown ES/EN:** protocolo, expansión y ownership.

## NS3-T09-F02 — Sintaxis de ranges perezosos / Lazy range syntax

**Commit:** `ns3(t09-f02): lazy-ranges`

- **Implementación:** rangos numéricos y adaptadores se representan como valores pequeños; no materializar colecciones; overflow/step cero diagnosticados.
- **Pruebas dirigidas:** inclusivo/exclusivo, descendente, const ranges, integer widths, fusion y `noheap`.
- **Markdown ES/EN:** límites, steps y costos.

## NS3-T09-F03 — Comprehensions / Comprehension expressions

**Commit:** `ns3(t09-f03): comprehensions`

- **Implementación:** forma compacta para construir o producir secuencias mediante protocolo de builder/yield; el sink determina allocation policy, nunca el lenguaje.
- **Pruebas dirigidas:** filters, nesting, move-only, builder stack, early error, inference y equivalencia con loops.
- **Markdown ES/EN:** lowering, selección de builder y advertencias de costo.

## NS3-T09-F04 — Views y préstamo durante iteración / Views and iteration borrows

**Commit:** `ns3(t09-f04): iteration-borrows`

- **Implementación:** expresar iteradores que prestan del source; lifetime asociado por trait; impedir mutación estructural incompatible durante recorrido.
- **Pruebas dirigidas:** lending iterators, windows/chunks, nested loops, mutation, escape y trait objects restringidos.
- **Markdown ES/EN:** modelo de préstamo y patrones seguros.

## NS3-T09-F05 — `yield` síncrono / Synchronous generators

**Commit:** `ns3(t09-f05): sync-generators`

- **Implementación:** bajar generadores a state machines nativas; captures y locals determinan layout; pinning cuando existan self-references; sin heap obligatorio.
- **Pruebas dirigidas:** múltiples yields, finally/defer, borrowed yields, move-only locals, cancelación/drop y tamaño del frame.
- **Markdown ES/EN:** transformación, lifetime y storage del frame.

## NS3-T09-F06 — Fusión y garantías de evaluación / Fusion and evaluation guarantees

**Commit:** `ns3(t09-f06): range-evaluation`

- **Implementación:** especificar laziness, orden y single-pass/multi-pass mediante traits; optimización puede fusionar sin cambiar efectos observables.
- **Pruebas dirigidas:** contadores de efectos, short-circuit, aliasing, vectorización, Debug/Release y adaptadores usuario.
- **Markdown ES/EN:** categorías y leyes requeridas.

### Baseline T09 / T09 baseline

- Ningún range/comprehension materializa memoria sin builder explícito.
- Generadores pasan pruebas de drop/cancelación y pinning.
- Loops manuales y pipelines equivalentes tienen MIR/codegen comparable.
- Protocolos admiten tipos de usuario sin hooks mágicos no documentados.
- Etiqueta: `lang-3.0-t09-baseline`.

---

# Tanda 10 — Coroutines y concurrencia estructurada / Coroutines and Structured Concurrency

**Objetivo:** definir semántica async nativa sin convertir el scheduler en parte del lenguaje.

## NS3-T10-F01 — Funciones `async` nativas / Native async functions

**Commit:** `ns3(t10-f01): async-functions`

- **Implementación:** `async` transforma la función en state machine con tipo futuro opaco; lazy/eager se fija normativamente; frame puede vivir en stack, arena o heap elegido por caller/runtime.
- **Pruebas dirigidas:** locals move-only, borrows cruzando await, recursion, `noheap`, layout frame, debug states y ejecución sin scheduler global.
- **Markdown ES/EN:** lowering, storage y puntos de suspensión.

## NS3-T10-F02 — `await` y protocolo Future / Await and Future protocol

**Commit:** `ns3(t10-f02): await-protocol`

- **Implementación:** protocolo estático de poll/resume; `await` solo en contexto permitido; resultado/error y cancellation son tipos explícitos.
- **Pruebas dirigidas:** future ready/pending, wake contract, references, nested awaits, propagation y no double-resume.
- **Markdown ES/EN:** contrato del protocolo y pseudocódigo de lowering.

## NS3-T10-F03 — Cancelación y destrucción de frames / Cancellation and frame destruction

**Commit:** `ns3(t10-f03): coroutine-cancellation`

- **Implementación:** cancelar equivale a destruir el future según protocolo; ejecutar drops/defer de locals activos; no existe thread abort implícito.
- **Pruebas dirigidas:** cancelación en cada suspensión, nested future, pinned fields, finally/defer y recursos FFI.
- **Markdown ES/EN:** garantías y operaciones que no se prometen.

## NS3-T10-F04 — `async` generators / Asynchronous generators

**Commit:** `ns3(t10-f04): async-generators`

- **Implementación:** combinar yield y await en state machine única con backpressure explícito; item lifetime y cancellation tipados.
- **Pruebas dirigidas:** ready/pending alternado, borrowed items, early stop, errors, move-only y frame size.
- **Markdown ES/EN:** protocolo y estados.

## NS3-T10-F05 — Tipos `Send`/`Sync` estructurales / Structural Send and Sync

**Commit:** `ns3(t10-f05): send-sync`

- **Implementación:** traits auto/unsafe para movilidad y acceso entre threads; reglas estructurales con opt-out; raw pointers no obtienen garantía automáticamente.
- **Pruebas dirigidas:** composites, atomics, refs, dyn traits, negative impl, DLL metadata y closures async.
- **Markdown ES/EN:** axiomas de seguridad y responsabilidad de `unsafe impl`.

## NS3-T10-F06 — Bloques de concurrencia estructurada / Structured concurrency blocks

**Commit:** `ns3(t10-f06): task-scopes`

- **Implementación:** concepto lingüístico mínimo de task scope que evita escape de borrows; spawn/join se resuelve por protocolo de executor; no se fija thread pool.
- **Pruebas dirigidas:** child borrow, early error, cancellation, panic, nested scopes, executor custom y `noheap`.
- **Markdown ES/EN:** lifetime de tareas y frontera lenguaje/runtime.

## NS3-T10-F07 — Memoria atómica y `volatile` consolidados / Atomic and volatile semantics

**Commit:** `ns3(t10-f07): memory-model-consolidation`

- **Implementación:** consolidar 2.0 en el modelo 3.0: data races son UB, órdenes atómicos válidos por operación, `volatile` es acceso observable y no sincronización.
- **Pruebas dirigidas:** litmus tests, invalid orderings, fences, tearing, volatile MMIO, optimizer checks y ThreadSanitizer cuando esté disponible.
- **Markdown ES/EN:** modelo de memoria formal y ejemplos correctos/incorrectos.

## NS3-T10-F08 — Primitivas de conteo de referencias / Reference-counting primitives

**Commit:** `ns3(t10-f08): refcount-primitives`

- **Implementación:** primitivas tipadas de incremento/decremento, detección de overflow, publicación y release/acquire final; hook de destrucción explícito y sin recolector oculto.
- **Pruebas dirigidas:** último release concurrente, overflow, weak/strong cuando aplique, destructor una sola vez, DLL boundary y ausencia de allocation implícita.
- **Markdown ES/EN:** modelo de memoria, responsabilidad del runtime y costos observables.

### Baseline T10 / T10 baseline

- Programa async mínimo corre con executor de prueba sin runtime global.
- Todos los estados de cancelación destruyen recursos una vez.
- `Send`/`Sync` impiden escapes cross-thread del corpus negativo.
- Modelo de memoria tiene revisión técnica independiente y litmus suite.
- Etiqueta: `lang-3.0-t10-baseline`.

---

# Tanda 11 — Metaprogramación estática / Static Metaprogramming

**Objetivo:** generar y consultar código en compilación con tipos, higiene y reproducibilidad.

## NS3-T11-F01 — Introspección estática tipada / Typed static introspection

**Commit:** `ns3(t11-f01): typed-introspection`

- **Implementación:** API `comptime` para consultar fields, methods, attributes, layout autorizado y traits; devuelve valores compile-time, no objetos reflection runtime.
- **Pruebas dirigidas:** tipos privados, genéricos abiertos/cerrados, orden estable, módulos, tipos incompletos y ausencia de metadata runtime.
- **Markdown ES/EN:** superficie permitida y privacidad.

## NS3-T11-F02 — Atributos definidos por usuario / User-defined attributes

**Commit:** `ns3(t11-f02): user-attributes`

- **Implementación:** valores constantes adjuntos a símbolos/tipos; targets y repetición declarados; retención compile-time por defecto, runtime solo en secciones nativas opt-in.
- **Pruebas dirigidas:** validación de target, arguments, duplicates, import/export, stripping y lectura `comptime`.
- **Markdown ES/EN:** definición, retención y ABI.

## NS3-T11-F03 — Derivación estática / Static derive

**Commit:** `ns3(t11-f03): derive`

- **Implementación:** `derive(Trait)` invoca generador tipado/higiénico; salida limitada a impls o miembros declarados; dependencias registradas para incrementalidad.
- **Pruebas dirigidas:** equality/hash/format examples, fields ignorados, generics, diagnostics mapped to source, cycles y deterministic output.
- **Markdown ES/EN:** contrato del derive y guía de autores.

## NS3-T11-F04 — Macros sintácticas higiénicas / Hygienic syntax macros

**Commit:** `ns3(t11-f04): hygienic-macros`

- **Implementación:** token trees o AST tipado con scopes higiénicos; expansión limitada y trazable; no acceso ambiental no declarado.
- **Pruebas dirigidas:** capture avoidance, nested expansions, errors/spans, module privacy, recursion limit y formatting round-trip.
- **Markdown ES/EN:** fases, higiene y límites.

## NS3-T11-F05 — `#run` reproducible y capacidades / Reproducible compile-time execution

**Commit:** `ns3(t11-f05): comptime-capabilities`

- **Implementación:** continuar `#run` de 2.0 con capabilities explícitas para archivos/env/reloj/aleatoriedad; modo hermético normativo; toda entrada externa entra al hash incremental.
- **Pruebas dirigidas:** lectura permitida/denegada, cache invalidation, paths, locale/timezone, random seed y builds bit-identical.
- **Markdown ES/EN:** modelo de capacidades y reproducibilidad.

## NS3-T11-F06 — Errores y warnings de usuario / User-authored diagnostics

**Commit:** `ns3(t11-f06): comptime-diagnostics`

- **Implementación:** `compile_error`, `compile_warning` y notes con spans autorizados; códigos reservados a paquetes no colisionan con `NS3xxx`.
- **Pruebas dirigidas:** generics, macro backtrace, multiple spans, localization fallback y incremental cache.
- **Markdown ES/EN:** API y estilo de mensajes.

## NS3-T11-F07 — Presupuesto y sandbox semántico / Resource budgets and semantic sandbox

**Commit:** `ns3(t11-f07): comptime-budget`

- **Implementación:** límites de pasos, memoria y recursión; error determinista con backtrace; operaciones `unsafe`/FFI prohibidas en evaluación hermética salvo capability fuera del perfil de conformidad.
- **Pruebas dirigidas:** loops infinitos, allocation masiva, recursion, cancellation, host crash isolation y diagnósticos reproducibles.
- **Markdown ES/EN:** mínimos garantizados, configuración y seguridad.

### Baseline T11 / T11 baseline

- Builds herméticos son bit-identical en dos rutas y dos procesos.
- Introspección no deja tablas runtime salvo opt-in explícito.
- Macros/derive conservan spans e higiene en suite adversarial.
- Toda dependencia externa de `#run` queda registrada.
- Etiqueta: `lang-3.0-t11-baseline`.

---

# Tanda 12 — Módulos, nombres y evolución de API / Modules, Names, and API Evolution

**Objetivo:** escalar el lenguaje a bases de código grandes sin convertir el package manager en parte de la especificación.

## NS3-T12-F01 — Módulos semánticos / Semantic modules

**Commit:** `ns3(t12-f01): semantic-modules`

- **Implementación:** unidad de encapsulación e import distinta del archivo; identidad canónica; imports no ejecutan código; ciclos permitidos solo donde binding/layout sea resoluble.
- **Pruebas dirigidas:** multi-file, cycles, duplicate identity, incremental rebuild, case-insensitive Windows paths y reproducibilidad.
- **Markdown ES/EN:** concepto de módulo y frontera con paquetes/build system.

## NS3-T12-F02 — Imports explícitos y aliases / Explicit imports and aliases

**Commit:** `ns3(t12-f02): imports`

- **Implementación:** import de módulo/símbolo/trait/extensions; glob restringido; alias local; lookup estable e independiente del orden.
- **Pruebas dirigidas:** collisions, private symbols, re-export, extensions, cycles y unused import lint.
- **Markdown ES/EN:** formas, lookup y prácticas recomendadas.

## NS3-T12-F03 — Visibilidad granular / Granular visibility

**Commit:** `ns3(t12-f03): visibility`

- **Implementación:** `private`, módulo, paquete lógico y `public`; friend access explícito y verificable; privacidad se aplica también a introspección y generated code.
- **Pruebas dirigidas:** nested types, impls, derives, consteval, re-export y DLL surface.
- **Markdown ES/EN:** matriz de acceso y compatibilidad.

## NS3-T12-F04 — Re-exports y fachadas / Re-exports and facades

**Commit:** `ns3(t12-f04): reexports`

- **Implementación:** re-export conserva identidad original; controlar nombres públicos y conflictos; no duplicar instancias ni símbolos de linker.
- **Pruebas dirigidas:** diamond facades, aliases, generics, docs identity, ABI names y removal diagnostics.
- **Markdown ES/EN:** identidad y diseño de API estable.

## NS3-T12-F05 — Deprecación y disponibilidad / Deprecation and availability

**Commit:** `ns3(t12-f05): availability`

- **Implementación:** atributos normativos `deprecated`, `since`, `removed`, target/architecture guards; mensajes y reemplazos estructurados; no hacer version solving.
- **Pruebas dirigidas:** warning/error por edición, transitive use, generics, macro-generated calls y suppressions scoped.
- **Markdown ES/EN:** política y esquema de metadata.

## NS3-T12-F06 — API surface fingerprint / Huella de superficie pública

**Commit:** `ns3(t12-f06): api-fingerprint`

- **Implementación:** representación canónica de símbolos públicos, tipos, effects, layouts contractuales e impls; consumible por herramientas pero definido desde semántica.
- **Pruebas dirigidas:** cambios source/ABI compatible e incompatible, orden irrelevante, docs-only, private changes y generics.
- **Markdown ES/EN:** elementos que forman contrato y versionado del formato.

## NS3-T12-F07 — Ownership genérico exportado / Exported generic ownership

**Commit:** `ns3(t12-f07): exported-generic-ownership`

- **Implementación:** publicar en NSMETA los contratos move/copy/drop, lifetimes y constraints de parámetros genéricos; rehidratar el mismo comportamiento sin requerir fuentes del productor.
- **Pruebas dirigidas:** biblioteca estática/DLL, consumidor sin fuente, especialización anidada, incompatibilidad N−1/N, metadata incompleta y diagnóstico causal.
- **Markdown ES/EN:** schema NSMETA, compatibilidad y límites de inferencia del consumidor.

### Baseline T12 / T12 baseline

- Grafo de módulos produce binding determinista en Windows.
- Privacidad se preserva en macros, introspección y metadata.
- Fingerprint distingue cambios source y ABI relevantes.
- No se ha incorporado resolución de paquetes al lenguaje.
- Etiqueta: `lang-3.0-t12-baseline`.

---

# Tanda 13 — Layout, ABI, FFI y unsafe / Layout, ABI, FFI, and Unsafe

**Objetivo:** completar el contrato de bajo nivel sobre la ABI formal de 2.0.

## NS3-T13-F01 — Atributos de representación / Representation attributes

**Commit:** `ns3(t13-f01): repr-attributes`

- **Implementación:** `repr(native)`, `repr(C)`, `repr(transparent)`, packing/alignment controlados; combinaciones inválidas rechazadas; `native` puede evolucionar entre ABI majors.
- **Pruebas dirigidas:** tamaño, alineación, offsets, zero-sized fields, generics, inheritance permitida y comparación con MSVC C.
- **Markdown ES/EN:** garantías exactas por representación.

## NS3-T13-F02 — Bitfields y tipos enteros de tamaño fijo / Bitfields and fixed-width integers

**Commit:** `ns3(t13-f02): bitfields`

- **Implementación:** bitfields solo con layout declarado; signedness, orden y acceso volatile definidos para Win64; tipos `i8..i128`, `u8..u128`, `isize/usize` normativos.
- **Pruebas dirigidas:** boundaries, sign extension, packing, MMIO volatile, atomics prohibidos/no soportados y headers C.
- **Markdown ES/EN:** layout y portabilidad.

## NS3-T13-F03 — Punteros enriquecidos / Rich pointer types

**Commit:** `ns3(t13-f03): pointer-qualifiers`

- **Implementación:** nullability, mutability, address space reservado, provenance y alignment knowledge; casts que pierden garantía requieren `unsafe`.
- **Pruebas dirigidas:** pointer arithmetic, null, integer casts, alignment, provenance through FFI, alias optimizations y diagnostics.
- **Markdown ES/EN:** operaciones, UB y promesas al optimizador.

## NS3-T13-F04 — Calling conventions como tipos / Calling conventions in types

**Commit:** `ns3(t13-f04): calling-conventions`

- **Implementación:** `native`, `cdecl`, `stdcall` donde aplique, `vectorcall` y Win64; forman parte del tipo función; adapters solo explícitos.
- **Pruebas dirigidas:** callbacks, variadics C, structs retornados, SIMD args, name mangling, DLL round-trip y mismatch errors.
- **Markdown ES/EN:** tabla Win64 y limitaciones por target.

## NS3-T13-F05 — Bloques `unsafe` auditables / Auditable unsafe blocks

**Commit:** `ns3(t13-f05): unsafe-auditing`

- **Implementación:** catálogo de operaciones unsafe; `unsafe fn` expresa requisito del caller y `unsafe {}` la asunción local; lint para bloques demasiado amplios y razón opcional.
- **Pruebas dirigidas:** raw deref, unions, FFI, inline asm, unsafe traits, nested blocks y false-safe regression corpus.
- **Markdown ES/EN:** lista exhaustiva y checklist de invariantes.

## NS3-T13-F06 — Contratos FFI / FFI contracts

**Commit:** `ns3(t13-f06): ffi-contracts`

- **Implementación:** atributos para nullability, ownership transfer, buffers length-linked, callbacks y unwind boundary; verificables en llamadas cuando haya metadata.
- **Pruebas dirigidas:** C DLL real, null mismatch, borrowed buffers, callback lifetime, ownership return y panic/error crossing.
- **Markdown ES/EN:** schema y ejemplos C/NativeSharp.

## NS3-T13-F07 — Tipos SIMD portables 3.0 / Portable SIMD types 3.0

**Commit:** `ns3(t13-f07): portable-simd`

- **Implementación:** consolidar SIMD 2.0 con vectors genéricos por lane/count, masks y operaciones definidas; fallback/scalarization permitido sin cambiar resultados.
- **Pruebas dirigidas:** lane counts, shuffles const, masks, NaN/FP modes, ABI vectorcall, assembly AVX2 y fallback.
- **Markdown ES/EN:** semántica por operación y requisitos de target.

## NS3-T13-F08 — Allocator y destrucción entre módulos / Cross-module allocator and destruction

**Commit:** `ns3(t13-f08): cross-module-allocation`

- **Implementación:** contratos `create/destroy/deallocate` mediante thunks versionados; el módulo que reserva define cómo destruir/liberar; proteger vida del módulo y rechazar unload con objetos vivos cuando no exista estrategia segura.
- **Pruebas dirigidas:** EXE↔DLL, DLL↔DLL, CRT estático, allocator personalizado, excepción durante construcción, unload y mismatch negativo.
- **Markdown ES/EN:** ownership de allocation, lifetime del módulo y reglas ABI 1.1.

### Baseline T13 / T13 baseline

- Layout suite coincide con programas testigo compilados por MSVC.
- Todo cast o acceso que pierde garantías está marcado `unsafe`.
- DLL round-trip cubre callbacks, aggregates, SIMD y errores.
- ABI provisional 3.0 queda lista para congelamiento.
- Etiqueta: `lang-3.0-t13-baseline`.

---

# Tanda 14 — Semántica numérica y constantes / Numeric and Constant Semantics

**Objetivo:** eliminar ambigüedades que causan diferencias entre Debug, Release y targets.

## NS3-T14-F01 — Literales numéricos tipados / Typed numeric literals

**Commit:** `ns3(t14-f01): numeric-literals`

- **Implementación:** literales inicialmente sin tipo con resolución contextual acotada; suffixes completos; separadores y bases; overflow diagnosticado antes de conversión.
- **Pruebas dirigidas:** boundaries de todos los widths, unary minus, overloads, generics const, hex float si se adopta y mensajes exactos.
- **Markdown ES/EN:** gramática y algoritmo de tipado.

## NS3-T14-F02 — Política de overflow / Overflow policy

**Commit:** `ns3(t14-f02): overflow-policy`

- **Implementación:** contextos `checked`, `wrapping`, `saturating` y `unchecked` con operaciones/traits explícitos; ninguna diferencia semántica accidental entre Debug y Release.
- **Pruebas dirigidas:** add/sub/mul/div/shift/cast, constants, SIMD, optimization y intrinsics.
- **Markdown ES/EN:** tablas por operación y consecuencias de UB donde exista.

## NS3-T14-F03 — Conversiones numéricas / Numeric conversions

**Commit:** `ns3(t14-f03): numeric-conversions`

- **Implementación:** matriz normativa de widening/narrowing/sign/float; conversiones implícitas solo sin pérdida bajo reglas aprobadas; casts explícitos siguen contexto overflow.
- **Pruebas dirigidas:** todos los pares fundamentales, overload ranking, constants representables, enum underlying y SIMD splats.
- **Markdown ES/EN:** matriz completa y ejemplos ambiguos.

## NS3-T14-F04 — Semántica FP y modos / Floating-point semantics and modes

**Commit:** `ns3(t14-f04): floating-point-semantics`

- **Implementación:** consolidar control FP 2.0: strict, precise y fast por scope/función; NaN, signed zero, contraction y reassociation definidos por modo.
- **Pruebas dirigidas:** NaN payload assumptions, `-0`, FMA, denormals, consteval/runtime parity, SIMD y optimizer.
- **Markdown ES/EN:** garantías de cada modo y portabilidad.

## NS3-T14-F05 — Unidades definidas por tipos / Units through types

**Commit:** `ns3(t14-f05): strong-numeric-newtypes`

- **Implementación:** facilitar newtypes transparentes y operadores/derives sin crear conversión dimensional mágica; esta feature cierra ergonomía, no impone un sistema de unidades.
- **Pruebas dirigidas:** repr transparent, operator traits, const generics, FFI, zero overhead y conversiones explícitas.
- **Markdown ES/EN:** patrón recomendado y límites deliberados.

## NS3-T14-F06 — Evaluación constante unificada / Unified constant evaluation

**Commit:** `ns3(t14-f06): consteval-unification`

- **Implementación:** misma semántica aritmética en runtime y `comptime`; memoria const con provenance; diagnósticos de UB durante compilación.
- **Pruebas dirigidas:** pointer const, unions, overflow modes, FP, generics, loops y paridad de resultados.
- **Markdown ES/EN:** subset ejecutable y diferencias intencionales.

### Baseline T14 / T14 baseline

- No hay cambios de overflow por configuración de optimización.
- Consteval y runtime coinciden en corpus numérico.
- Modos FP producen atributos/backend verificables.
- Matriz de conversiones queda congelada para RC.
- Etiqueta: `lang-3.0-t14-baseline`.

---

# Tanda 15 — Integración del lenguaje y calidad / Language Integration and Quality

**Objetivo:** probar interacciones reales, no solo features aisladas.

## NS3-T15-F01 — Matriz combinatoria / Feature interaction matrix

**Commit:** `ns3(t15-f01): interaction-matrix`

- **Implementación:** suite generada/manual que cruza variants, traits, generics, lifetimes, async, errors, macros, modules y FFI en combinaciones de alto riesgo.
- **Pruebas dirigidas:** pares y tríos priorizados por cambios MIR/ownership; seeds fijados; minimización de fallos.
- **Markdown ES/EN:** matriz cubierta, huecos aceptados y proceso de expansión.

## NS3-T15-F02 — Corpus de programas completos / Whole-program corpus

**Commit:** `ns3(t15-f02): whole-program-corpus`

- **Implementación:** programas testigo sin depender de un SDK amplio: parser pequeño, allocator arena de prueba, job graph, serializer binario, DLL host/plugin y servidor async mínimo.
- **Pruebas dirigidas:** Debug/Release, clean/incremental, LTO/no-LTO, sanitizers, determinismo y perfiles `noheap` parciales.
- **Markdown ES/EN:** propósito y métricas de cada programa.

## NS3-T15-F03 — Presupuestos de rendimiento / Performance budgets

**Commit:** `ns3(t15-f03): language-performance-budgets`

- **Implementación:** thresholds para tiempo/memoria de frontend, tamaño de MIR, monomorfización y code size; separar regresión del lenguaje de backend/toolchain.
- **Pruebas dirigidas:** archivos gigantes, generic-heavy, macro-heavy, module fanout, async frames y pathological diagnostics.
- **Markdown ES/EN:** hardware de referencia, metodología y tolerancias.

## NS3-T15-F04 — Calidad de diagnósticos / Diagnostic quality gate

**Commit:** `ns3(t15-f04): diagnostic-quality`

- **Implementación:** revisar top errores de borrow, traits, match, async y modules; sugerencias nunca cambian semántica; macro backtraces compactos.
- **Pruebas dirigidas:** golden diagnostics ES/EN si hay localización, spans, fix-its compilables, cascades y terminal width.
- **Markdown ES/EN:** guía de estilo y catálogo final.

## NS3-T15-F05 — Debuggability semántica / Semantic debuggability

**Commit:** `ns3(t15-f05): debug-semantics`

- **Implementación:** auditar el contrato transversal iniciado en T00 y aplicado por T01–T14:
  nombres, tipos, valores, scopes, lifetimes, sequence points, código generado, inline sites,
  closures, variants, ownership, exceptions, coroutines y temporales. Verificar que O0 prioriza
  fidelidad fuente y que O2 nunca inventa valores. Esto define la evidencia que debe producir el
  compilador; la interfaz del debugger sigue perteneciendo a tooling.
- **Pruebas dirigidas:** inspección estructural MIR/LLVM/PDB, sesión DAP real, invariantes de
  stepping O0/O2, valores exactos, ausencia antes de declaración, fin de vida, variant display
  metadata, async state names, SEH/RAII y módulos DLL. Incluir obligatoriamente la reproducción
  local-SEH descrita en la sección 11.
- **Markdown ES/EN:** contrato de información observable, estados de disponibilidad, clasificación
  de código generado, degradaciones permitidas bajo optimización y matriz por feature.

## NS3-T15-F06 — Auditoría de costos ocultos / Hidden-cost audit

**Commit:** `ns3(t15-f06): zero-cost-audit`

- **Implementación:** instrumento que marca alloc, copy, drop, dynamic dispatch, unwind edge y atomic/fence generados; aprobar excepciones explícitas.
- **Pruebas dirigidas:** cada azúcar 3.0, worst cases, optimization-off/on, library-free fixtures y diffs MIR/assembly.
- **Markdown ES/EN:** tabla de costos por constructo.

### Baseline T15 / T15 baseline

- Corpus completo compila y ejecuta en Win64 sin CLR.
- Cero regresiones críticas en rendimiento/size respecto al código manual equivalente.
- Matriz de interacciones no tiene combinaciones P0/P1 pendientes.
- Auditoría Debug/PDB no contiene valores falsos, scopes prematuros ni sequence points O0 faltantes.
- Cost audit publicado en ES/EN.
- Etiqueta: `lang-3.0-t15-baseline`.

---

# Tanda 16 — Compatibilidad y migración / Compatibility and Migration

**Objetivo:** permitir adopción gradual y medir cada ruptura.

## NS3-T16-F01 — Modo de compatibilidad 2.x / 2.x compatibility mode

**Commit:** `ns3(t16-f01): edition-2-compat`

- **Implementación:** frontend 3.0 compila edición 2 con semántica 2.x; módulos binarios 2.x aceptados cuando ABI/fingerprint sea compatible; adapters explícitos cuando no.
- **Pruebas dirigidas:** corpus histórico, mezcla por módulo, old metadata, corrupted versions y incremental rebuild.
- **Markdown ES/EN:** promesas exactas y límites.

## NS3-T16-F02 — Lints de migración / Migration lints

**Commit:** `ns3(t16-f02): migration-lints`

- **Implementación:** detectar palabras ahora reservadas, cambios de overload, copy/move, nullability, imports, numeric conversions y cleanup order.
- **Pruebas dirigidas:** un caso por ruptura, false positives, suppressions, generated code y mensajes con alternativa.
- **Markdown ES/EN:** catálogo `MIG3xxx`.

## NS3-T16-F03 — Fix-its semánticamente seguros / Semantics-preserving fix-its

**Commit:** `ns3(t16-f03): safe-fixits`

- **Implementación:** correcciones estructuradas solo si preservan significado demostrable; cambios ambiguos producen guía, no edición automática.
- **Pruebas dirigidas:** aplicar/recompilar, formatting, comments, macros, multi-file y idempotencia.
- **Markdown ES/EN:** lista de transformaciones automáticas y manuales.

## NS3-T16-F04 — Matriz source/ABI / Source and ABI compatibility matrix

**Commit:** `ns3(t16-f04): compatibility-matrix`

- **Implementación:** clasificar cada feature/cambio como source-compatible, source-breaking, ABI-neutral o ABI-breaking; ligar clasificación al API fingerprint.
- **Pruebas dirigidas:** bibliotecas fixture v2/v3, consumer rebuild/no-rebuild, DLL swapping y symbol/layout checks.
- **Markdown ES/EN:** matriz completa con ejemplos.

## NS3-T16-F05 — Guía de migración 2.x → 3.0 / Migration guide

**Commit:** `ns3(t16-f05): migration-guide`

- **Implementación:** documento ejecutable por capítulos; antes/después; estrategia incremental; inventario de decisiones manuales.
- **Pruebas dirigidas:** todos los snippets compilan en su edición; proyecto testigo migra por etapas y conserva resultados.
- **Markdown ES/EN:** guía completa, no solo release notes.

## NS3-T16-F06 — Ventana de deprecación / Deprecation window

**Commit:** `ns3(t16-f06): deprecation-policy`

- **Implementación:** política para aliases sintácticos y comportamientos 2.x; ninguna eliminación posterior en 3.x sin pasar por deprecación, salvo bug de seguridad/UB documentado.
- **Pruebas dirigidas:** warning por versión/edición, suppression, CI warnings-as-errors y removal simulation.
- **Markdown ES/EN:** calendario basado en releases, excepciones y soporte.

### Baseline T16 / T16 baseline

- Corpus 2.x alcanza la meta de compatibilidad acordada.
- Cada ruptura conocida tiene diagnóstico y sección de migración.
- Proyecto testigo migra sin big-bang.
- Matriz source/ABI firmada antes de RC1.
- Etiqueta: `lang-3.0-t16-baseline`.

---

# Tanda 17 — Congelamiento y promoción / Freeze and Promotion

**Objetivo:** convertir el conjunto de previews en NativeSharp Language 3.0 oficial.

## NS3-T17-F01 — Congelamiento de sintaxis / Syntax freeze

**Commit:** `ns3(t17-f01): syntax-freeze`

- **Implementación:** cerrar gramática, precedencia, palabras reservadas y reglas de recuperación; generar gramática de referencia desde una fuente canónica.
- **Pruebas dirigidas:** parser corpus, fuzzing, ambiguity checks, formatter round-trip y todos los snippets del spec.
- **Markdown ES/EN:** gramática final y apéndice de precedencia.

## NS3-T17-F02 — Congelamiento semántico / Semantic freeze

**Commit:** `ns3(t17-f02): semantic-freeze`

- **Implementación:** cerrar overloads, conversions, inference, ownership, drop, errors, async y memory model; toda excepción se registra como issue de 3.1+.
- **Pruebas dirigidas:** suite normativa completa, differential configurations y determinismo.
- **Markdown ES/EN:** versión candidata final del specification set.

## NS3-T17-F03 — Congelamiento ABI Win64 / Win64 ABI freeze

**Commit:** `ns3(t17-f03): win64-abi-freeze`

- **Implementación:** congelar mangling, layout contractual, calling conventions, variants, trait objects, errors, coroutines públicas y metadata de módulo.
- **Pruebas dirigidas:** DLL fixtures preservadas, MSVC interop, cross-version consumer, symbol snapshots y PDB contract.
- **Markdown ES/EN:** ABI 3.0 Win64 normativa.

## NS3-T17-F04 — Suite de conformidad candidata / Candidate conformance suite

**Commit:** `ns3(t17-f04): conformance-rc`

- **Implementación:** separar tests obligatorios, recomendados y de implementación; versionar manifest/resultados; habilitar ejecución por terceros.
- **Pruebas dirigidas:** self-validation, intentionally broken compiler fixtures, shard/retry y hermetic run.
- **Markdown ES/EN:** manual de conformidad y reporte.

## NS3-T17-F05 — Auditoría de especificación bilingüe / Bilingual specification audit

**Commit:** `ns3(t17-f05): bilingual-spec-audit`

- **Implementación:** cada cláusula normativa recibe ID estable compartido; ES y EN deben contener iguales requisitos, ejemplos y notas de compatibilidad.
- **Pruebas dirigidas:** link checker, clause parity, code sample compilation, glossary consistency y orphan pages.
- **Markdown ES/EN:** índices finales y glosario cruzado.

## NS3-T17-F06 — Release candidates / Candidatos de lanzamiento

**Commit:** `ns3(t17-f06): release-candidates`

- **Implementación:** RC1 congela features; RC2 solo acepta fixes; RC3 solo bloqueadores de promoción; todo cambio reinicia el gate afectado.
- **Pruebas dirigidas:** full clean matrix, upgrade/downgrade metadata, reproducibility, long-running stress y package-free samples.
- **Markdown ES/EN:** notas RC y lista cerrada de known issues.

## NS3-T17-F07 — Promoción oficial / Official promotion

**Commit:** `ns3(t17-f07): promote-language-3-0`

- **Implementación:** marcar edición 3 estable, fijar feature set, publicar hashes de spec/suite y abrir rama de mantenimiento 3.0; experimental features quedan fuera por nombre y flag.
- **Pruebas dirigidas:** instalación/build limpio en máquina Win64 de referencia, smoke interop, samples y verificación de artefactos.
- **Markdown ES/EN:** anuncio técnico, soporte y ruta 3.1.

### Baseline T17 / T17 baseline final

- Sintaxis, semántica y ABI Win64 congeladas.
- Cero issues P0; P1 solo con waiver público y fecha, idealmente cero.
- Suite de conformidad completa en Debug y Release.
- Documentación ES/EN en paridad y sin ejemplos rotos.
- Migración 2.x validada sobre proyectos testigo.
- Artefactos reproducibles y hashes publicados.
- Etiquetas: `lang-3.0-rc1`, `lang-3.0-rc2`, `lang-3.0-rc3` cuando proceda y `native-sharp-language-3.0.0` para promoción.

---

## 4. Criterios globales de congelamiento / Global freeze criteria

### 4.1 Feature freeze

Se alcanza cuando:

- todas las tandas T00–T16 tienen baseline;
- ninguna feature estable depende de una feature experimental;
- todo punto tiene especificación ES/EN, pruebas y nota de costo;
- no quedan decisiones abiertas que alteren gramática, sistema de tipos o ownership;
- SDK/runtime/toolchain dependencies están identificadas y no se presentan como lenguaje.

Después del feature freeze no se aceptan nuevas features, solo correcciones, eliminación de una feature incompleta o clarificación sin cambio semántico.

### 4.2 Syntax freeze

Requiere gramática no ambigua, precedencia cerrada, palabras reservadas definitivas, parser fuzzed y todos los ejemplos compilados. Cambiar tokens tras este punto reinicia RC.

### 4.3 Semantic freeze

Requiere cerrar:

- categorías de valores y orden de evaluación;
- lookup, overload resolution e inferencia;
- copy/move/borrow/drop;
- layouts contractuales y efectos;
- errors, panic, cleanup y cancellation;
- memory model y fronteras `unsafe`;
- comportamiento de `comptime` y macros.

### 4.4 ABI freeze Win64

Requiere fixtures binarios conservados desde RC1 para:

- funciones y calling conventions;
- structs, records, tuples, variants y unions;
- generics exportados donde se soporten;
- trait objects y vtables;
- typed errors y coroutine frames públicos;
- symbols, mangling, metadata y PDB contract.

El PDB contract congelado incluye semántica observable, no sólo la existencia física del archivo:
líneas, columnas cuando apliquen, scopes, tipos, valores, location ranges, inline sites y
clasificación de procedimientos generados.

### 4.5 Documentation freeze

Cada cláusula normativa debe tener ID y versión. La edición española y la inglesa tienen la misma autoridad; discrepancias bloquean promoción hasta resolverse.

---

## 5. Compatibilidad prometida / Compatibility promise

### 5.1 Código fuente / Source

- La edición 2 conserva sus reglas al compilarse con el frontend 3.0 dentro del periodo de soporte.
- La edición 3 no adopta cambios breaking en releases 3.x salvo corrección de seguridad, UB o contradicción normativa claramente documentada.
- Los warnings futuros no cambian el resultado del programa; un nuevo hard error necesita proceso de edición o excepción de seguridad.

### 5.2 ABI

- El compromiso inicial es **Win64**.
- `repr(C)` sigue el ABI C soportado; `repr(transparent)` preserva el ABI de su único campo no vacío bajo sus restricciones.
- `repr(native)` solo se considera estable bajo la misma ABI major y las condiciones publicadas.
- Tipos cuyo layout no sea contractual no deben cruzar DLL boundaries sin wrapper explícito.

### 5.3 Metadata de módulos

- Incluye magic, schema version, language edition, target triple, ABI major, feature flags y fingerprint.
- Un consumidor rechaza incompatibilidad con diagnóstico, nunca interpreta silenciosamente metadata desconocida.
- Debe existir al menos lectura de la versión anterior durante la ventana de migración acordada.

### 5.4 Migración

La ruta recomendada es:

1. actualizar compilador manteniendo edición 2;
2. activar lints `MIG3xxx`;
3. eliminar dependencias en comportamiento ambiguo;
4. migrar módulos hoja a edición 3;
5. estabilizar fronteras ABI;
6. migrar módulos centrales;
7. activar warnings-as-errors 3.0;
8. regenerar fingerprint y fixtures binarios.

---

## 6. Criterios de promoción oficial / Official promotion gates

NativeSharp Language 3.0.0 solo se promueve si se cumplen todos:

| Gate | Requisito |
|---|---|
| Semántica | Especificación congelada; cero contradicciones conocidas |
| Correctitud | 100% de tests normativos; cero P0; cero P1 sin waiver |
| Regresión | Suite 2.x dentro de la meta de compatibilidad aprobada |
| Ownership | Cero fallos conocidos de soundness; corpus negativo rechazado |
| ABI | Win64 congelada y validada contra fixtures/MSVC donde aplique |
| Debug/PDB | O0 fiel a fuente; O2 honesto; cero valores falsos; SEH/RAII/inline/DLL certificados |
| Rendimiento | Sin costos ocultos; presupuestos T15 cumplidos |
| Determinismo | Builds y `comptime` reproducibles bajo perfil hermético |
| Interop | DLL/C callbacks/layout/errors probados end-to-end |
| Documentación | Paridad ES/EN, links válidos, snippets compilables |
| Migración | Guía ejecutada sobre al menos dos proyectos testigo no triviales |
| RC | Mínimo dos candidatos; el último sin cambios semánticos durante la ventana acordada |

No se bloquea 3.0 por features de SDK, package manager, IDE o plataformas distintas de Win64, salvo que una dependencia sea imprescindible para demostrar una promesa explícita del lenguaje.

---

## 7. Features experimentales y pospuestas / Experimental and deferred features

Estas ideas pueden incubarse detrás de flags, pero no forman parte de 3.0 estable sin completar todas las tandas/gates correspondientes:

- compile-time borrow proofs avanzados dependientes de SMT;
- reflection runtime general;
- garbage collector integrado;
- exceptions compatibles con CLR;
- herencia múltiple de estado;
- keyword arguments estructurales dinámicos;
- variadics C portables fuera de targets soportados;
- GPU kernels/shader language;
- distributed actors;
- hot reload como semántica del lenguaje;
- stable ABI multiplataforma;
- package/version resolution;
- macros con acceso irrestricto al host.

Su exclusión no implica rechazo permanente: evita que 3.0 dependa de semántica aún no demostrada.

---

## 8. Orden de implementación / Dependency order

El orden normativo recomendado es:

```text
T00 Constitución
 ├─ T01 Expresiones
 ├─ T02 Tuplas/records ─┐
 ├─ T04 Genéricos ──────┼─ T05 Traits ── T09 Iteración ── T10 Coroutines
 └─ T06 Ownership ──────┼─ T07 Errores ────────────────┘
                        └─ T08 Closures

T03 Variants/match depende de T02 y toca T06/T07
T11 Metaprogramación depende de T04/T05/T12 parcial
T12 Módulos puede avanzar en paralelo tras T00
T13 ABI/unsafe y T14 numéricos pueden avanzar tras T00, cerrando después de T03–T10
T15 integración requiere T01–T14
T16 migración requiere semántica candidata
T17 congelamiento requiere todos los baselines
`Debug/PDB` cruza T00–T17: cada feature entrega su contrato y T15/T17 sólo lo certifican
```

Las tandas expresan dependencias y puntos de estabilización, no fechas. Se permite trabajo paralelo siempre que cada commit conserve pruebas independientes y no se cree un baseline con dependencias incompletas.

---

## 9. Checklist por pull request/commit lógico / Per-change checklist

```markdown
- [ ] ID `NS3-TNN-FMM` y commit lógico correcto
- [ ] Feature pertenece al lenguaje; dependencias externas están separadas
- [ ] Gramática y semántica normativa
- [ ] Parser/binder/type checker/lowering implementados según aplique
- [ ] Diagnósticos con códigos y spans estables
- [ ] Casos positivos y negativos
- [ ] MIR/codegen/ABI checks cuando aplique
- [ ] Ownership/drop/noheap checks cuando aplique
- [ ] Contrato Debug/PDB: nombres, tipos, valores, scopes, lifetimes y sequence points
- [ ] Inspección MIR/LLVM/PDB y sesión real cuando exista estado observable
- [ ] O0 no pierde fidelidad por SEH; O2 no inventa valores
- [ ] Interacciones con al menos dos features previas
- [ ] Compatibilidad 2.x y nota de migración
- [ ] Markdown ES y EN con clause IDs equivalentes
- [ ] Ejemplos compilables
- [ ] Clean e incremental Win64
- [ ] Determinismo verificado
- [ ] Sin cambios no relacionados
```

---

## 10. Definición final de NativeSharp Language 3.0 / Final definition

NativeSharp Language 3.0 no es “C# sin CLR” ni “C++ con otra sintaxis”. Es un lenguaje nativo AOT con:

- control de memoria, layout, ABI, concurrencia y destrucción propio de sistemas;
- expresividad moderna mediante variants, matching, traits, genéricos, closures y coroutines;
- ergonomía reconocible sin disfrazar allocations ni dispatch;
- metaprogramación estática reproducible, no reflection administrada;
- interoperabilidad Win64 verificable;
- evolución por ediciones y contratos medibles.

La versión 3.0 queda completa cuando esas propiedades están simultáneamente especificadas, implementadas, probadas, documentadas en ambos idiomas y congeladas; no cuando alcanza un número arbitrario de features.

---

## 11. Contrato maestro PDB y modo Debug / Master PDB and Debug contract

Esta sección consolida la auditoría de PDB realizada el 2 de octubre de 2026. Es parte normativa
del roadmap, no una lista opcional posterior a la implementación del lenguaje.

### 11.1 Veredicto de la auditoría

El caso real de al82dev demostró que un PDB puede:

- ser nuevo;
- coincidir con el SHA-256 del fuente;
- corresponder al ejecutable correcto;
- contener procedimientos, líneas y nombres;
- y aun así publicar estado fuente falso.

El PDB inspeccionado registró Program.ns con SHA-256:

    865E076969C05A38ABF8A3A65598C9599C8E5048AA0EF06701784A652AF216E8

La identidad fuente era correcta. La información semántica no:

| Local | Registro CodeView observado | Valor publicado | Valor correcto |
|---|---|---:|---:|
| a | S_CONSTANT | 10 | 10 |
| b | S_CONSTANT | 90 | 90 |
| c | S_CONSTANT | 10 | 100 |
| x | S_CONSTANT | 1 | 1, solamente después de declararse |

Las declaraciones de a, b, c y x tampoco tenían sitios ejecutables propios en la tabla de líneas.
Por tanto, éste no es un problema de “rebuild” ni de PDB obsoleto: es un defecto del productor.

### 11.2 Cadena causal confirmada

1. Program.Main contiene try/catch.
2. El lowering marca la función con personalidad Windows SEH.
3. La ruta especial O0 ejecuta mem2reg sobre toda la función para evitar la corrupción histórica
   de frame B-001.
4. Los locals ordinarios se habían descrito sólo con dbg.declare asociado a allocas.
5. mem2reg elimina los slots, transforma valores a SSA/constantes y puede eliminar las únicas
   instrucciones que sostenían las líneas de declaración.
6. No existe assignment tracking completo para cada inicialización y asignación de local ordinario.
7. CodeView publica constantes sin rangos equivalentes a los S_DEFRANGE esperados.
8. nsdbg recibe x con valor presente y no tiene evidencia suficiente para ocultarlo antes de su
   declaración.

La corrección histórica B-001 sigue siendo obligatoria. El roadmap prohíbe “arreglar” la
depuración reintroduciendo corrupción de ejecución.

### 11.3 Grieta exacta en las pruebas existentes

| Prueba | Lo que demuestra | Lo que no cruza |
|---|---|---|
| DebugScopesT36 | scopes ordinarios, local tardío, dos variables i | no contiene SEH; no ejecuta mem2reg especial |
| DebugValuesT37 | parámetros, out, catch tipado y globals | locals ricos y SEH están en funciones distintas |
| DebugPdb | catálogo amplio y control de excepciones | no exige valores/lifetimes exactos en cada dirección |
| DebugExtreme | nombres y tipos de muchas features | presencia no demuestra valor, rango ni stepping |
| K&R Fahrenheit | valores, scopes y Console stepping | no envuelve los locals en try/catch/finally |

La nueva regla es: ninguna capacidad se declara cubierta sólo porque su nombre aparezca en
llvm-pdbutil. La evidencia debe probar interacción y comportamiento de una sesión real.

### 11.4 Estados normativos de una variable

El compilador y sus artefactos deben permitir distinguir:

1. **No declarada:** no aparece en el scope activo.
2. **Inicializándose:** ausente o marcada not initialized; nunca contiene bytes accidentales.
3. **Disponible:** ubicación y valor válidos en la dirección actual.
4. **Constante válida:** valor correcto limitado a su scope y rango semántico.
5. **Optimizada:** nombre conocido, valor imposible de reconstruir.
6. **Fuera de vida:** no aparece como local activo.
7. **Movida:** estado moved-from conforme a la semántica del tipo.
8. **Destruida:** no se presenta como objeto vivo.
9. **Metadata del productor insuficiente:** distinto de optimized out.
10. **Ubicación no soportada por el consumidor:** distinto de los estados anteriores.

Un valor falso bloquea release. Mostrar unavailable cuando no hay evidencia es aceptable bajo el
perfil documentado; inventar una ubicación o un valor no lo es.

### 11.5 Sequence points normativos

O0 debe definir una política determinista para:

- declaración con initializer;
- asignación;
- llamada simple y llamada anidada;
- return;
- condición de if, while, for y match;
- incremento de for;
- break, continue y propagación de error;
- throw y rethrow;
- entrada de catch y cuerpo de finally;
- construcción, move y destrucción RAII;
- defer;
- await y yield;
- macro expansion y código generado.

No se exige trabajo observable falso por línea. Se exige que el usuario pueda predecir dónde liga
el breakpoint, en qué estado están los valores y cuál es el siguiente stop lógico.

Las columnas y una política equivalente a discriminadores son obligatorias cuando dos operaciones
fuente comparten línea o una expresión contiene llamadas anidadas.

### 11.6 Política de código generado

Cada procedimiento o rango ejecutable se clasifica como:

- user code;
- compiler-generated visible;
- compiler-generated hidden;
- ABI thunk;
- cleanup/destruction helper;
- SEH filter o funclet;
- inline site;
- coroutine/generator state machine.

Prologues, epilogues, guardas y housekeeping no deben secuestrar Step Over. Un helper con semántica
que el usuario deba comprender puede ser visible, pero siempre identificado como generado.

### 11.7 Solución técnica obligatoria para O0 + SEH

El orden de preferencia es:

1. corregir la forma de IR/funclets que obliga al workaround y eliminar mem2reg completo en O0;
2. si no es posible, promover únicamente allocas indispensables para la corrección SEH;
3. si toda la función debe seguir en SSA, emitir seguimiento completo de asignaciones y valores,
   además de sequence points resistentes a la promoción.

La solución debe demostrar simultáneamente:

- el crash histórico B-001 no regresa;
- los locals no relacionados conservan fidelidad O0;
- c = a + b publica 100;
- x no existe antes de su declaración;
- cada declaración exigida tiene breakpoint exacto o una reubicación explícita y documentada.

### 11.8 Assignment tracking

Todo local susceptible de promoción necesita:

- asociación entre cada store semántico y su variable;
- resultado SSA real de la expresión;
- actualización después de cada asignación;
- terminación de ubicación al salir del scope;
- soporte para ramas y PHI;
- fragmentos de agregados;
- cambios stack/register;
- constantes limitadas a su vida.

Se evaluará dbg.assign/DIAssignID. Si la API C de LLVM no expone lo necesario, se permite ampliar
el shim, introducir una capa C++ mínima o emitir dbg.value controlado. La elección se acepta por el
PDB final y la sesión real, no únicamente por la forma del IR.

### 11.9 Fixture mínimo obligatorio

NSC debe poseer un fixture equivalente a:

    int Main()
    {
        int a = 10;
        int b = 90;
        int c = a + b;
        int x = 1;
        try
        {
            Consume(c);
        }
        catch (Exception error)
        {
            Consume(error);
        }
        return c;
    }

El gate dirigido exige:

- breakpoint exacto en cada declaración;
- ausencia antes de declaración;
- a = 10, b = 90, c = 100 y x = 1;
- valores exactos después de mutaciones;
- catch parameter con tipo, categoría y lifetime correctos;
- salida semántica correcta;
- conservación de EXE/PDB después de detener y relanzar.

### 11.10 Evidencia de cuatro capas

Cada caso Debug/PDB importante conserva:

1. **MIR:** variable, scope, initializer, assignments, moves y cleanups.
2. **LLVM antes/después de pases:** dbg.declare/value/assign, locations, PHI y transforms.
3. **CodeView/PDB:** procedimiento, lexical parent, símbolo, tipo, constante, ranges y líneas.
4. **Sesión real:** breakpoint, stepping, scopes y valores mediante nsdbg/DAP.

Los verificadores de texto siguen siendo smoke tests. La aceptación semántica requiere un inspector
estructural que pueda preguntar, por ejemplo: “x está ausente en la dirección de la declaración de
a y vale 1 después de su propia declaración”.

### 11.11 Matriz Debug/PDB por feature

| Tanda | Obligación Debug/PDB |
|---|---|
| T00 | estados de variable, sequence points, user/generated code y categorías de error |
| T01 | orden de evaluación, named args, rangos, null operators e interpolación |
| T02 | elementos de tuple/record, deconstrucción, temporales y partial moves |
| T03 | discriminante, payload activo, pattern bindings, guards y ramas |
| T04 | nombres genéricos cerrados, const generics y especializaciones |
| T05 | tipo concreto, trait object, dispatch y ausencia de frames falsos |
| T06 | move, borrow, reborrow, drop, defer, pinning y lifetimes |
| T07 | Result, operador de propagación, throws, catch, finally, never y panic |
| T08 | capturas, receiver, lambdas genéricas, delegate cell, inline y tail calls |
| T09 | iterator state, yield, comprehension y locals por iteración |
| T10 | await/yield sites, frame suspendido, cancelación y logical stack |
| T11 | macro backtrace, fuente original, generated code y atributos de debugger |
| T12 | módulo dueño, re-export, fuentes duplicadas y package symbols |
| T13 | layout, bitfields, pointers, calling conventions, FFI y SIMD |
| T14 | valor exacto, overflow mode, FP mode y constant evaluation |
| T15 | matriz combinatoria y auditoría final |
| T16 | convivencia 2.x/3.0, source mapping y migración |
| T17 | congelamiento del contrato Windows x64 Debug/PDB |

### 11.12 Superficie de valores y tipos

La certificación acumulada cubre:

- parámetros value/ref/borrowed/owned/out/variadic y this;
- locals escalares, constantes, agregados, temporales y capturas;
- shadowing, homónimos, loop variables, catch variables y pattern bindings;
- enteros, bool, char, float/double, enums, structs, unions y bitfields;
- clases, interfaces, trait objects y herencia;
- arrays fijos/dinámicos/multidimensionales/VLA, spans, slices y strings;
- pointers, references, function pointers, delegates y native handles;
- tuples, records, variants, Result y optional;
- generics, const generics, SIMD, closures y coroutine frames;
- fragments, múltiples location ranges, gaps y optimized-out.

### 11.13 Ownership, excepciones e interop

El PDB debe describir de forma honesta:

- owned, borrowed, raii y move-only;
- comienzo después de construcción exitosa;
- estado tras move y final después de destrucción;
- partial moves y drop flags sin contaminar Locals;
- cleanup por return, break, continue y excepción;
- catch tipado, throw, rethrow y finally;
- filtros artificiales sin frames fuente falsos;
- callbacks C/COM y fronteras de excepción;
- assembly integrado;
- EXE/DLL, carga tardía, unload y breakpoints pendientes.

### 11.14 Breakpoints y stepping

Cuando una línea solicitada no tiene sitio físico, nsdbg/DAP debe publicar:

- requestedLine;
- actualLine;
- si el breakpoint fue exacto o ajustado;
- la causa del ajuste.

Nunca se cruza silenciosamente a otra función o scope. En los gates estrictos de O0, una declaración
ejecutable ajustada se considera defecto del productor.

Step Over usa la siguiente ubicación lógica de usuario, no únicamente la siguiente dirección o
línea distinta. Las llamadas Console, runtime, cleanup y helpers tienen presupuesto acotado y no
pueden congelar la sesión.

### 11.15 Atributos de experiencia de depuración

Cuando T11 disponga de atributos definidos por usuario, se evaluará una biblioteca sin nuevas
keywords:

- DebuggerDisplay;
- DebuggerBrowsable;
- DebuggerHidden;
- DebuggerStepThrough;
- DebuggerNonUserCode;
- DebuggerTypeProxy.

No introducen reflection runtime, heap ni dependencia de nsdbg. Su efecto sobre stepping y
presentación debe especificarse por separado.

### 11.16 Tandas operativas Debug/PDB

Cada tanda es un commit, incluye su Markdown y ejecuta sólo sus pruebas dirigidas.

#### NS3-DBG-01 — Contrato y reproducción

- fixture local-SEH basado en al82dev;
- dumps MIR/IR/PDB;
- escenario DAP exacto;
- reproducción roja antes del fix.

#### NS3-DBG-02 — O0 SEH sin degradación global

- rebisección del workaround B-001;
- reducir o eliminar mem2reg completo;
- gate de ejecución y gate de slots/locales.

#### NS3-DBG-03 — Assignment tracking y SSA

- inicializaciones, assignments, PHI y ramas;
- valor c = 100;
- constantes con vida acotada.

#### NS3-DBG-04 — Sequence points O0

- declaraciones, asignaciones, condiciones, increments, calls y returns;
- líneas y columnas;
- breakpoint exacto.

#### NS3-DBG-05 — Lifetimes y scopes

- antes de declaración, fin de scope, shadowing y back-edges;
- x ausente antes de su línea;
- cero inferencia textual en el debugger.

#### NS3-DBG-06 — Console y stepping

- llamadas simples y anidadas;
- Step In/Over/Out;
- Write, WriteLine, ToString y ToStringFixed;
- cero congelamientos.

#### NS3-DBG-07 — SEH, RAII y ownership

- catch/finally/rethrow/nested catch;
- construcción, move, cleanup y destrucción;
- catch parameter correcto.

#### NS3-DBG-08 — Tipos y visualización

- scalars, enums, strings, arrays, structs, classes, delegates, pointers, SIMD y generics;
- tamaños, offsets y valores exactos.

#### NS3-DBG-09 — Features nuevas 3.0

- tuples, records, variants, patterns, traits y Result;
- contrato Debug entregado con la feature.

#### NS3-DBG-10 — Closures, inline y optimización

- capturas e inline frames;
- tail calls;
- O0 fiel y O2 honesto.

#### NS3-DBG-11 — Async, generators y módulos

- estados suspendidos y logical stacks;
- EXE/DLL, carga tardía y PDB identity.

#### NS3-DBG-12 — nsdbg y VS Code

- NSDBG congela primero el contrato DAP y sus capabilities reales;
- breakpoint relocation visible;
- estados de disponibilidad distintos;
- diagnóstico de identidad y metadata;
- transcript DAP reproducible como entrega para el consumidor;
- la integración de UI se ejecuta después, en `NS3-VSC-01`–`NS3-VSC-07`.

#### NS3-DBG-13 — Certificación dirigida

- toda la matriz Debug/PDB;
- evidencia y tiempos por caso;
- sin baseline general;
- sin instalación en C:/native-sharp.

#### NS3-DBG-14 — Candidato de componente

- build limpio de NSDBG en `A:`;
- suites dirigidas acumuladas de `NS3-DBG-01`–`NS3-DBG-13`;
- corrección de todas las regresiones dirigidas;
- paquete candidato conservado en staging de `A:`;
- contrato DAP entregado a VS Code;
- sin baseline general, VSIX, instalación en `C:\native-sharp` ni tag.

La autorización del milestone integral ocurre sólo después de completar la etapa
VS Code y el cierre único definido en la sección 12.

### 11.17 Criterios de aceptación del milestone

1. al82dev liga correctamente las declaraciones consecutivas.
2. c se muestra como 100.
3. x no aparece antes de declararse.
4. B-001 no regresa.
5. Step Over en Console no se congela.
6. detener y relanzar no borra el ejecutable.
7. PE/PDB coinciden por GUID+age.
8. checksum fuente coincide.
9. catch/finally/RAII conservan líneas y lifetimes.
10. O0 nunca muestra valores falsos.
11. O2 usa unavailable/optimized out cuando no puede reconstruir.
12. DLL y módulos conservan identidad.
13. VS Code muestra la ubicación real de breakpoints ajustados.
14. no se instala en C:/native-sharp antes de autorización.

### 11.18 Prohibiciones

- no atribuir automáticamente al debugger un defecto observado en VS Code;
- no ocultar todas las S_CONSTANT;
- no desactivar excepciones para conservar debugging;
- no eliminar B-001 sin su reproducción;
- no aceptar presencia de nombre como prueba de valor correcto;
- no tratar O0 como O2;
- no añadir sintaxis para compensar defectos del backend;
- no modificar proyectos reales para hacer pasar una prueba;
- no ejecutar baseline en cada tanda;
- no reemplazar la instalación estable durante desarrollo.

La definición de éxito es concreta: en el breakpoint exacto solicitado, el depurador muestra
solamente las variables que existen, con los valores que realmente tienen, y Next avanza a la
siguiente operación fuente sin congelarse ni mentir.

---

## 12. Plan integral y ordenado del toolchain / Unified Ordered Toolchain Plan

Esta sección fusiona el roadmap del lenguaje con el plan operativo de los cuatro
consumidores que deben cerrarse en orden. No crea otra versión del producto: los
milestones `v.3.0.2`, `v.3.0.3`, etc. siguen perteneciendo a NativeSharp 3.0.

El orden obligatorio es:

```text
NSC (`NS3-T00`–`NS3-T17`)
  -> NSLink (`NS3-LNK-01`–`NS3-LNK-09`)
    -> NSDBG (`NS3-DBG-01`–`NS3-DBG-14`)
      -> NSC-LSP (`NS3-LSP-01`–`NS3-LSP-10`)
        -> NSC-VSCode (`NS3-VSC-01`–`NS3-VSC-07`)
          -> NSASM (`NS3-ASM-01`–`NS3-ASM-07`)
            -> cierre integrado autorizado
```

### 12.1 Reglas únicas de ejecución / Single execution rules

1. `v.3.0.2` permanece congelada y todo trabajo nuevo ocurre en `development`.
2. Cada tanda produce un commit local y un Markdown de evidencia en el repositorio
   propietario.
3. Cada tanda construye sólo lo necesario y ejecuta únicamente pruebas dirigidas.
4. Ninguna tanda autoriza por sí sola un baseline general.
5. Durante desarrollo todos los artefactos se construyen y consumen desde `A:`.
6. `C:\native-sharp` sólo recibe el despliegue después del cierre autorizado; nunca
   se usa como fuente para reconstruir o completar staging.
7. Un cambio de contrato aterriza primero en el productor y después en su consumidor.
8. Un fallo se corrige en el componente propietario; queda prohibido esconderlo con
   fallback silencioso, ruta hardcodeada o tolerancia en el consumidor.
9. NSC puede construirse con clang/LLVM y lld-link. Los programas `.ns` de prueba
   deben enlazarse con NSLink, salvo una comparación explícita y documentada.
10. El baseline integral sólo se ejecuta al final y mediante autorización expresa del
    propietario.

### 12.2 Etapa 1 — NSC / Stage 1 — NSC

Las únicas tandas normativas del lenguaje son `NS3-T00`–`NS3-T17`, ya definidas
en las secciones anteriores. No se crean identificadores paralelos `NSC-xx`.

#### Estado ejecutivo del catálogo NSC

Los 120 IDs `NS3-T00-F01`–`NS3-T17-F07` constituyen el inventario completo del
lenguaje 3.0. Una vertical dirigida terminada no cierra automáticamente toda la
tanda normativa. El estado vigente y el trabajo que aún impide el cierre son:

| Tanda | Features | Estado vigente | Trabajo todavía abierto |
|---|---:|---|---|
| `NS3-T00` | 6 | `IMPLEMENTING` | propagar edición, stage y `EffectContext` por toda la tubería; catálogo diagnóstico único y harness de conformidad completo |
| `NS3-T01` | 7 | `IMPLEMENTING` | carriers físicos de null-aware, slicing completo, holes runtime y `with` owning sin ABI inventada |
| `NS3-T02` | 6 | `IMPLEMENTING` | cerrar metadata, drop parcial, tooling y combinaciones nested/cross-module de tuplas y records |
| `NS3-T03` | 6 | `IMPLEMENTING` | exhaustiveness/usefulness completo, moves por arm, guards, refinamiento y unions C seguras |
| `NS3-T04` | 7 | `IMPLEMENTING` | const generics/layout, packs, alias opacos, especialización y límites de monomorfización en el pipeline real |
| `NS3-T05` | 7 | `IMPLEMENTING` | associated types/constants, `some`, coherence completa, extensions y operadores mediante traits |
| `NS3-T06` | 8 | `IMPLEMENTING` | `Copy`, partial/split borrow, lifetimes nombrados, `defer`, pinning, escape interprocedural y drop glue importado |
| `NS3-T07` | 6 | `IMPLEMENTING` | `Result`/`?`, typed throws, `try/catch/finally`, loop values y perfiles panic unwind/handler |
| `NS3-T08` | 6 | `IMPLEMENTING` | tipos función completos, captures copy/move/ref, lambdas genéricas, `FnOnce`, stack/noheap y tail-call contracts |
| `NS3-T09` | 6 | `DESIGNED/IMPLEMENTING` | protocolo foreach completo, ranges perezosos, comprehensions, views, `yield` y fusión |
| `NS3-T10` | 8 | `DESIGNED/IMPLEMENTING` | async/await físico, cancelación, generators, `Send`/`Sync`, task scopes, modelo atómico y refcount |
| `NS3-T11` | 7 | `DESIGNED/IMPLEMENTING` | introspección estática, atributos, derive, macros higiénicas, `#run`, diagnósticos de usuario y sandbox/budgets |
| `NS3-T12` | 7 | `DESIGNED/IMPLEMENTING` | módulos/imports/visibilidad/reexports, availability, fingerprints y ownership genérico exportado |
| `NS3-T13` | 8 | `DESIGNED/IMPLEMENTING` | `repr`, bitfields, punteros, calling conventions, unsafe, contratos FFI, SIMD y allocator cross-module |
| `NS3-T14` | 6 | `DESIGNED/IMPLEMENTING` | literales, overflow, conversiones, FP modes, units y consteval unificada contra oráculo independiente |
| `NS3-T15` | 6 | `DESIGNED` | matriz combinatoria, whole-program corpus, budgets, diagnósticos, debug y auditoría de costos |
| `NS3-T16` | 6 | `DESIGNED` | compatibilidad 2.x, lints/fix-its, matriz source/ABI, guía y ventana de deprecación |
| `NS3-T17` | 7 | `DESIGNED` | freeze de sintaxis/semántica/ABI, suite candidata, auditoría bilingüe, RC y promoción |

Los bloques `P00`–`P11`, `SH00`–`SH54`, el ledger de 156 observaciones y las
brechas de `vsBeef` son gates y fuentes de evidencia de estas tandas; no crean un
segundo catálogo de features. Toda brecha debe mapearse a un ID anterior o quedar
clasificada expresamente como runtime/tooling, `NO-OBJETIVO` o `DIFERIDA-4.0`.

| Bloque | Trabajo de cierre | Entrega al siguiente componente |
|---|---|---|
| `NS3-T00` | constitución, ediciones, etapas, value categories, efectos, diagnósticos y harness | contrato estable de lenguaje y diagnóstico |
| `NS3-T01`–`NS3-T05` | expresiones, tuplas/records, variants/patterns, genéricos, traits y despacho | semántica y metadata completas para codegen/link |
| `NS3-T06`–`NS3-T10` | ownership, errores, funciones/closures, iteración y concurrencia | MIR de lifetimes/cleanup, ABI de callables y estados suspendidos |
| `NS3-T11`–`NS3-T14` | metaprogramación, módulos, ABI/FFI/unsafe y numéricos | objetos COFF Win64, NSMETA y manifiestos reproducibles |
| `NS3-T15` | interacción, programas completos, rendimiento, diagnósticos y depurabilidad | corpus integrado y presupuestos medidos |
| `NS3-T16` | compatibilidad, lints, fix-its y migración 2.x → 3.0 | matriz de compatibilidad y guía verificable |
| `NS3-T17` | freeze de sintaxis, semántica, ABI y suite candidata | contrato candidato que NSLink debe consumir |

Antes de cerrar `NS3-T15`, las 156 secciones de
`A:\Limitantes\limitaciones-conversion-v4.md` deben tener estado terminal. Las
observaciones que correspondan a defectos del material fuente permanecen separadas
y no se contabilizan como defectos del compilador.

El gate de salida de NSC exige:

- parser, semántica, ownership, MIR, codegen y diagnósticos cubiertos por pruebas
  dirigidas positivas y negativas;
- proyectos `.nsproj`, referencias, `NS_LIB_ROOT`, `--no-restore`, NSMETA y grafo
  de fuentes reproducibles sin leer `C:`;
- progreso, telemetría y logs deterministas;
- O0 sin passes opcionales innecesarios y mediciones limpia/incremental;
- corpus versionado de objetos COFF válidos e inválidos;
- contrato de objetos, ABI, NSMETA y Debug/PDB entregado a NSLink;
- gates self-host afectados ejecutados individualmente, sin baseline general.

### 12.3 Etapa 2 — NSLink / Stage 2 — NSLink

#### NS3-LNK-01 — Contrato de entrada

- versión de objeto/NSMETA, machine type, arquitectura y opciones;
- códigos de salida y diagnósticos estables;
- corpus válido e inválido recibido de NSC.

#### NS3-LNK-02 — COFF y resolución de símbolos

- objetos y archives;
- índices, weak externals, COMDAT, aliases y `.drectve`;
- duplicados y símbolos ausentes independientes del orden de entrada.

#### NS3-LNK-03 — Imagen PE x64

- secciones, alineación, relocations e imagen base;
- imports, exports, entry point, subsystem, recursos y TLS;
- EXE y DLL mínimas cargables en Windows x64.

#### NS3-LNK-04 — Runtime, ABI y excepciones

- runtime estático aprobado;
- `.pdata`, `.xdata`, unwind y cleanup;
- contrato ABI 1.1 y ausencia de imports runtime prohibidos.

#### NS3-LNK-05 — CodeView/PDB

- contribuciones por objeto, GUID/age y rutas normalizadas;
- identidad PE/PDB y datos consumibles por NSDBG;
- preservación de líneas, funciones, scopes y tipos entregados por NSC.

#### NS3-LNK-06 — Determinismo y entradas hostiles

- orden canónico y timestamps controlados;
- overflow, archivos truncados, Unicode y rutas largas;
- fallo sin crash ni artefacto parcial presentado como válido.

#### NS3-LNK-07 — Rendimiento e incrementalidad

- índices persistentes con claves completas;
- IO acotado y paralelismo medido;
- telemetría por fase y comparación antes/después.

#### NS3-LNK-08 — Integración con el driver NSC

- NSLink predeterminado para programas `.ns`;
- argumentos, response files, logs y cancelación;
- lld-link sólo como comparación interna explícita, nunca fallback oculto.

#### NS3-LNK-09 — Entrega a NSDBG

- fixtures O0–O3 enlazados exclusivamente por NSLink;
- símbolos, PDB, mapa de imagen y resultados esperados;
- prueba dirigida de que no se invocó lld-link.

El gate de salida de NSLink es un staging en `A:` con `nslink.exe` 3.0 y un corpus
de EXE/DLL NativeSharp enlazado por NSLink. No instala en `C:` ni ejecuta baseline.

### 12.4 Etapa 3 — NSDBG / Stage 3 — NSDBG

Esta etapa ejecuta exactamente `NS3-DBG-01`–`NS3-DBG-14` de la sección 11.16;
no existe una segunda serie `NSD-xx`. Su orden cubre:

1. contrato y reproducción;
2. O0+SEH;
3. assignment tracking/SSA;
4. sequence points;
5. lifetimes y scopes;
6. Console y stepping;
7. SEH, RAII y ownership;
8. tipos y visualización;
9. features nuevas 3.0;
10. closures, inline y optimización;
11. async, generators y módulos;
12. contrato NSDBG/VS Code;
13. certificación dirigida;
14. candidato de componente en staging de `A:`.

El gate de salida es `nsdbg.exe` 3.0 estático, sin dependencias dinámicas de LLVM
o del runtime MSVC, acompañado por transcripts DAP, corpus PE/PDB y la matriz de
disponibilidad O0–O3. No incluye VSIX, baseline general ni instalación en `C:`.

### 12.5 Etapa 4 — NSC-LSP / Stage 4 — NSC-LSP

#### NS3-LSP-01 — Frontera pública de análisis

- publicar `NativeSharp::LanguageService` sin exponer AST/HIR/MIR/LLVM;
- compartir identidad canónica del compilador sin enlazar driver/backend;
- exportar una API estática estable para el servidor independiente.

#### NS3-LSP-02 — Documentos versionados en memoria

- overlays sin guardar con versión estrictamente monotónica;
- rutas absolutas Windows normalizadas e insensibles a mayúsculas;
- snapshots concurrentes sin reemplazar texto nuevo con una respuesta vieja.

#### NS3-LSP-03 — Posiciones UTF-16 y diagnósticos

- conversión bidireccional UTF-8/UTF-16 con CRLF, BMP y pares sustitutos;
- rangos, severidad, código y mensaje estructurados;
- rechazo de posiciones dentro de secuencias inválidas.

#### NS3-LSP-04 — Snapshots, cancelación y recuperación

- una compilación inmutable por generación;
- cancelación antes de HIR/MIR/backend;
- diagnósticos actuales con recuperación separada del último modelo válido.

#### NS3-LSP-05 — Índice semántico básico

- símbolos de documento/workspace, hover y definición;
- identidad semántica y overload elegido por el frontend;
- índices reutilizables por snapshots válidos y recuperados.

#### NS3-LSP-06 — Consultas semánticas avanzadas

- completion, signature help, referencias, rename y semantic tokens;
- rangos exactos y workspace edits sin escribir archivos;
- cero reinterpretación de NativeSharp mediante regex en el servidor.

#### NS3-LSP-07 — Formato conservador

- formato estable con espacios/tabs y conservación de CRLF/LF;
- literales y comentarios intactos, sin edición cuando ya está normalizado;
- `TextEdit` completo en coordenadas UTF-16.

#### NS3-LSP-08 — Acciones de código seguras

- quick fixes sólo cuando el frontend demuestra la transformación;
- `SourceFixAll` respaldado por el formateador;
- ninguna corrección inferida desde texto humano del diagnóstico.

#### NS3-LSP-09 — Preservación de inline assembly

- detectar bloques `asm` fuera de comentarios/literales;
- no modificar sus bytes ni sangría;
- preferir ausencia de edición a alterar la entrada física de NSASM.

#### NS3-LSP-10 — Identidad técnica del compilador

- `--print-compiler-identity` con encabezado `NSCOMPILER 1`;
- versión, release, lenguaje, stage y hash en campos estables;
- sin banner, copyright ni texto localizado.

El gate de salida es la API de análisis y su harness directo, sin depender de
VS Code. Cada capability anunciada debe tener handler real y prueba dirigida.

### 12.6 Etapa 5 — NSC-VSCode / Stage 5 — NSC-VSCode

#### NS3-VSC-01 — Identidad y descubrimiento

- detectar NSC, NSLink y NSDBG 3.0;
- mostrar rutas efectivas y prioridad de configuración;
- rechazar mezcla accidental con instalaciones 2.0.

#### NS3-VSC-02 — Proyectos y builds

- `.nsproj`, O0–O3, jobs, referencias, clean/rebuild y cancelación;
- precedencia idéntica a la documentada por NSC;
- línea efectiva reproducible y visible.

#### NS3-VSC-03 — Progreso y diagnósticos

- módulo/fase actual y porcentaje cuando el productor lo proporcione;
- consola inglesa/española;
- Problems, códigos/spans, `error.log` consecutivo y vínculo a evidencia.

#### NS3-VSC-04 — Integración DAP

- launch/attach, breakpoints, watches, stepping y excepciones;
- consumo literal de capabilities de NSDBG;
- ubicación real visible cuando un breakpoint se ajuste.

#### NS3-VSC-05 — Experiencia de edición

- diagnósticos, navegación, símbolos, completion y acciones disponibles;
- ninguna capability se anuncia si el backend no la implementa;
- errores accionables cuando una operación no esté soportada.

#### NS3-VSC-06 — Empaquetado offline Win64

- VSIX, esquemas, snippets, licencias y documentación;
- sin dependencia de Visual Studio, .NET o C++/CLI;
- configuración por prefijo NativeSharp 3.0.

#### NS3-VSC-07 — Aceptación de producto

- crear, abrir, compilar, ejecutar y depurar;
- rutas Unicode/largas y workspaces múltiples;
- migración desde 2.0 y correlación de logs NSC/NSLink/NSDBG.

El gate de salida es una VSIX candidata probada contra el staging de `A:`. Todavía
no se instala el toolchain en `C:` ni se crea un milestone nuevo.

### 12.7 Etapa 6 — NSASM / Stage 6 — NSASM

#### NS3-ASM-01 — Identidad y distribución 3.0

- versión pública 3.0 y Windows x64;
- `nsasm`, `nsdump`, `nsdis` y `NativeAsmCore` autocontenidos;
- runtime MSVC estático y paquete CMake verificable.

#### NS3-ASM-02 — Compatibilidad MASM/ML64

- macros, directivas, tipos, expresiones e `INCLUDE` compatibles con ml64;
- errores y warnings estables;
- decisiones fuera de alcance documentadas y probadas.

#### NS3-ASM-03 — Codificación x86-64

- enteras, x87, SSE, AVX, AVX2, FMA, BMI y AVX-512;
- operandos, prefijos, ModRM/SIB y EVEX completos;
- comparación byte a byte contra oráculo independiente.

#### NS3-ASM-04 — COFF, relocations y unwind

- objetos COFF, `.drectve`, `.pdata` y `.xdata`;
- símbolos externos y `PROC FRAME` consumibles por NSLink;
- addends y relocations verificados mediante ejecución real.

#### NS3-ASM-05 — CodeView y listados

- `/Zi`, archivos, líneas y símbolos útiles para NSDBG;
- listados `/Fl` deterministas y expansión opcional `/Sa`;
- identidad de fuente preservada hasta PE/PDB.

#### NS3-ASM-06 — NSDump, NSDis y round-trip

- inspección estructural de COFF;
- desensamblado con longitud exacta;
- assemble→dump→disassemble→assemble reproducible.

#### NS3-ASM-07 — Robustez e integración

- corpus válido/inválido sin crashes;
- ejemplos reales ejecutados en Windows;
- cadena NSC→NSASM→NSLink sin dependencias dinámicas prohibidas.

El gate de salida es el producto NSASM 3.0 completo con sus suites normales
correctas. No incluye baseline del compilador ni instalación en `C:`.

### 12.8 Cierre integrado autorizado / Authorized integrated closure

1. Construir NSC, NSLink y NSDBG desde `development` usando sólo fuentes y
   dependencias aprobadas en `A:`.
2. Crear desde cero `A:\native-sharp-3.0-baseline` y poblarla de `A:` hacia `A:`.
3. Ejecutar smoke tests NSC → NSLink → NSDBG → NSC-LSP → VS Code y la cadena
   NSC → NSASM → NSLink.
4. Entregar hashes, rutas, commits, lista de pruebas dirigidas y pendientes reales.
5. Esperar autorización expresa del propietario para ejecutar el baseline integral.
6. Ante un fallo, corregirlo en el repositorio propietario y ejecutar únicamente la
   prueba dirigida; no relanzar automáticamente el baseline completo.
7. Con baseline aprobado, registrar Gate 0, desplegar de `A:` hacia
   `C:\native-sharp`, probar la instalación y crear el milestone local autorizado.
8. El nombre del milestone puede avanzar (`v.3.0.3`, `v.3.0.4`, etc.), pero la
   identidad final del producto continúa siendo NativeSharp 3.0.

### 12.9 Fronteras / Boundaries

- Libraries/runtime, Bindings, Project Manager, SaruGL y SaruDX quedan fuera de
  esta mega-tanda por decisión expresa.
- Pueden entregar contratos o fixtures, pero no se incorporan como componentes
  ni filas de tracking de este plan.
- El IDE monolítico permanece fuera del alcance.
- Ningún cierre documental equivale por sí solo a certificación ejecutable.
