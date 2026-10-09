# Host.Language — inventario semántico y contrato reconstructivo

## Estado y alcance

- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Estado:** `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada.
- **Universo:** 42 archivos, 7,323 líneas y 263,231 bytes.
- **Naturaleza:** helpers normativos puros, máquinas de estado, manifests y validadores consumidos por Sema,
  frontend, lowering, metadata y gates de release.
- **Fuera de alcance:** modificar políticas, borrar contratos versionados, implementar pruebas o ejecutar código.

## 1. Frontera del componente

`Host.Language` no es un parser ni el analizador semántico completo. Congela decisiones reutilizables como
funciones de datos: ABI, layouts, conversions, ownership, generators, modules, numerics y release. Los
consumidores son responsables de suministrar hechos ya resueltos; estos helpers validan o derivan un resultado
determinista. La reconstructibilidad exige probar tanto resultados aceptados como todos los rechazos observables.

## 2. Pares de archivos y contrato propietario

| Contrato | Header / source | Líneas | Responsabilidad |
|---|---|---:|---|
| `CL-LANG-001` | `AbiSemantics.h/.cpp` | 264 | ABI, punteros, FFI y SIMD portable |
| `CL-LANG-002` | `AggregateSemantics.h/.cpp` | 246 | tuples, layout, deconstruction, records y spread |
| `CL-LANG-003` | `CallableSemantics.h/.cpp` | 395 | identidades callable, closures, lambdas y tail calls |
| `CL-LANG-004` | `CompatibilitySemantics.h/.cpp` | 237 | compatibilidad binaria, rebuild y migración |
| `CL-LANG-005` | `CoroutineSemantics.h/.cpp` | 1,071 | futures, async generators, Send/Sync y task scopes |
| `CL-LANG-006` | `CrossModuleLifetime.h/.cpp` | 187 | lifetime y registro de objetos entre módulos |
| `CL-LANG-007` | `ErrorSemantics.h/.cpp` | 131 | Result, typed throws, never, panic y assertions |
| `CL-LANG-008` | `ExpressionErgonomics.h/.cpp` | 482 | expresiones, rangos e interpolación estática |
| `CL-LANG-009` | `GenericSemantics.h/.cpp` | 287 | constraints, const generics, specialization y budget |
| `CL-LANG-010` | `IntegrationQuality.h/.cpp` | 687 | matrices, corpus, budgets, diagnostics, debug y costo |
| `CL-LANG-011` | `IterationSemantics.h/.cpp` | 477 | iterators, ranges, generator ABI y fusion |
| `CL-LANG-012` | `Language3Surface.h/.cpp` | 145 | catálogo histórico de superficie y diagnostics |
| `CL-LANG-013` | `MemoryModel.h/.cpp` | 134 | órdenes atómicos, races, volatile y manifest |
| `CL-LANG-014` | `MetaprogrammingSemantics.h/.cpp` | 148 | introspection, attributes, hygiene y comptime |
| `CL-LANG-015` | `ModuleSemantics.h/.cpp` | 195 | identidades, grafo, imports, visibility y API hash |
| `CL-LANG-016` | `NumericSemantics.h/.cpp` | 900 | literals, arithmetic, conversions, FP y const memory |
| `CL-LANG-017` | `OwnershipSemantics.h/.cpp` | 174 | moves, borrows, lifetimes, cleanup, pin y escape |
| `CL-LANG-018` | `ReleaseSemantics.h/.cpp` | 375 | freeze, conformance, RC y promotion |
| `CL-LANG-019` | `SmartPointerSemantics.h/.cpp` | 314 | familias smart pointer y manifest ABI |
| `CL-LANG-020` | `SumTypeSemantics.h/.cpp` | 202 | variants, patterns, exhaustiveness y C unions |
| `CL-LANG-021` | `TraitSemantics.h/.cpp` | 272 | traits, coherence, projections, dyn y operators |

## 3. ABI, FFI y SIMD — `CL-LANG-001`

- `validateRepresentation` acepta packing/alignment potencia de dos hasta 16; `Transparent` exige exactamente
  un campo no vacío y prohíbe packing.
- `bitFieldMask` sólo acepta storage 8/16/32/64, width no cero y rango contenido; width 64 evita shift UB.
- assignability de punteros y necesidad de unsafe son duales sobre nullability, mutability, provenance,
  alignment y address space.
- variadic sólo admite `Cdecl` o `Win64`; las cinco operaciones unsafe siempre requieren contexto y un bloque
  de más de ocho statements sin razón se considera demasiado amplio.
- un callback escapable exige shape callback y ownership `Owned` o `Transferred`; unwind FFI siempre es falso.
- SIMD admite lane 8/16/32/64, count potencia de dos >=2 y total <=512. Identidades `simd<>`/`mask<>` hacen
  round-trip, shuffle exige exactamente laneCount índices válidos y mask lanes producen todos los bits uno.

## 4. Agregados — `CL-LANG-002`

- tuple identity codifica sólo type keys con longitud; nombres no afectan identidad.
- conversion tuple exige misma aridad y acepta igualdad o pares `builtin:*` no string; no modela una tabla de
  conversiones numéricas más fina.
- layout Win64 alinea cada campo, rechaza size cero/alignment inválido y clasifica retornos directos sólo para
  tamaños 1, 2, 4 y 8.
- deconstruction ignora discard y `_`, exige nombre para los demás y rechaza duplicados.
- `RecordDeriveSet` es un bitset inmutable de cuatro flags. Promoted fields exigen nombre/tipo únicos; spread
  admite como máximo una fuente y explicit fields únicos.

## 5. Callables y closures — `CL-LANG-003`

- function identity es length-prefixed y congela carrier, calling convention, cinco flags, lifetime,
  parameters, result y error type. El parser rechaza prefijos, flags, longitudes, count y trailing bytes inválidos.
- assignability exige parameters/result/carrier/convention/lifetime/variadic/async iguales, permite source sin
  error hacia target con error, sólo relaja unsafe/noheap/nullability en la dirección codificada.
- anonymous lambda identity añade owner/lambda ordinals; nombre generado es `__nsc_lambda_O_L`.
- closure layout valida nombre, size y alignment <=16 y alinea en orden. Escape rechaza referencias cuyo source
  no sobreviva; trait es `FnOnce` si hay move, luego `FnMut` si mutable reference, en otro caso `Fn`.
- sólo lambda sin captures convierte a pointer; generic lambda no usa erased dispatch. Bound method Win64 es
  `{32,8}`. `must_tail` rechaza cleanup pendiente o ABI incompatible.

## 6. Compatibilidad y migración — `CL-LANG-004`

- module compatibility rechaza estructura inválida, targets distintos de `windows-x64` y producer metadata
  major mayor al consumer; ABI fingerprint igual es direct y distinto requiere adapter.
- compilation plan exige al menos una unidad, module/source identity/fingerprint, identidades y módulos únicos,
  e imports resueltos.
- rebuild set parte de módulos nuevos/cambiados y propaga transitivamente por imports actuales; módulos
  eliminados que ya no están en `current` no aparecen por sí mismos.
- siete migration issues producen `MIG3001..MIG3007`; enum externo cae en `MIG3000`.
- automatic fix exige replacement y equivalencia probada. API classification separa source/layout/ABI y trata
  private-only como neutral.
- guide exige capítulos no vacíos, orden estrictamente creciente, before/after, decisiones manuales e identidad
  única. Removal 3.x sólo se permite por seguridad/UB o deprecation previa.

## 7. Coroutines, concurrencia y task scopes — `CL-LANG-005`

- futures son lazy; ninguna selección implica heap implícito. Un borrow cruza await sólo con lifetime y address
  estable.
- async plan exige return type, awaits 1..N contiguos, borrow proofs y liveness válida; sin awaits sólo acepta
  locals que no vivan sobre suspensión y produce plan synchronous.
- Future protocol exige value/error/cancellation explícitos, Poll/Resume estáticos y single consumer. Await
  requiere async, no comptime y no cleanup no-suspend.
- `FutureState` implementa Created/Pending/Ready/Cancelled/Consumed y Value/Error completion; terminal states no
  reanudan, ready consume una vez y ready/consumed/cancelled no cancelan.
- cancellation ordena defers por depth descendente y después frame slots droppables en reversa.
- async generator requiere item/error/cancellation, al menos una suspensión y un yield, states contiguos y
  proofs para borrows. Su protocolo exige demanda explícita, un solo item outstanding y terminalidad estable.
- Send/Sync simple falla con raw pointer/negative impl. El evaluador detallado distingue value, atomic, shared,
  mutable, raw y dyn; unsafe override sólo opera con proof reviewed y nunca vence negative impl.
- metadata concurrency valida major y type name. Task scope exige executor estático completo, soporte noheap
  cuando se solicita y borrowed children joined con lifetime compatible.
- salida abnormal cancela children incompletos en reversa, luego join en reversa y finalmente propaga causa.
- manifest/fingerprint son deterministas; escritura crea parent y verificación normaliza CRLF.

## 8. Lifetime entre módulos — `CL-LANG-006`

- contrato construye tres thunks estables usando FNV-1a de `module::type` y ABI major.
- validación exige major compatible, producer/type/allocator y los tres thunks; fingerprint incorpora minor y
  unload policy.
- registry está protegido por mutex, fija un fingerprint por producer, evita mezcla, creación durante unload y
  overflow; unregister exige registro vivo y fingerprint exacto.
- `requestUnload` siempre rechaza live objects y marca unloading cuando llega a cero. La implementación vigente
  no ramifica por `ModuleUnloadPolicy`; `PinUntilObjectsDestroyed` queda sólo dentro del fingerprint.

## 9. Errores y control divergente — `CL-LANG-007`

- `?` exige success/error types y los tres miembros estáticos. Conversión de error es identidad o adapter.
- typed throws exige error type y adapter para cruzar C ABI.
- catch coverage usa set y rethrow cubre todo. `never` convierte a cualquier target no vacío; loop breaks ignoran
  `never`, unifican un tipo o fallan.
- panic parser admite dos spellings por perfil; Abort no cleanup/unwind, NativeUnwind sí cleanup y nunca cruza
  FFI. Sólo `Assume` requiere unsafe.

## 10. Expresiones e interpolación — `CL-LANG-008`

- siete features devuelven contratos source-order/evaluate-once/no implicit allocation/preserve ABI y siempre
  están disponibles. El índice usa cast directo sin guard para enums externos.
- typed pack busca `ReadOnlySpan<` en cualquier posición y `>` final. Null-aware clasifica pointer por sufijo,
  reference por `type:`/string y optional por substring.
- endpoint normal o from-end debe caer en `[0,length]`; half-open aplica defaults y exige start<=end.
- interpolation parser maneja `{{`/`}}`, nesting, quotes, format separator, trimming y offsets; limpia `parts` en
  cada error.
- handler recibe parts en orden y conserva su error o sintetiza uno. Builtin acepta bool o integer decimal con
  D/d/X/x y width <=64; hexadecimal negativo se rechaza; width decimal incluye signo y `-0` pierde signo.

## 11. Genéricos — `CL-LANG-009`

- constraints se ordenan/deduplican, rechazan empty y tres conflictos y crean canonical length-prefixed.
- const generic exige type/value; marker valida prefijo, decimal sin overflow, length no cero, separator y value.
- instantiation identity conserva orden de type/value arguments y kind. Pack vacío pasa, zipped exige longitudes
  iguales y budget.
- inference sólo acepta igualdad sin arbitrary conversion. Alias transparent colapsa a representation; opaque
  usa module/name. Specialization domina si es superconjunto estricto por set.
- budget limita depth total, count acumulativo y recursion activa; reporta hasta 12 frames y `leave` vacío es
  no-op. Count no disminuye al salir.

## 12. Calidad de integración — `CL-LANG-010`

- interaction matrix exige identity, >=2 áreas, seed no cero, identidad única y seis pares críticos; prioritized
  prohíbe Medium, exige impacto/minimization y al menos un triple. Builder canónico produce siete casos.
- minimizer sólo conserva essential areas existentes y al menos dos.
- whole-program base exige corpus no vacío, deterministic/incremental/SDK-independent e identidad única; el
  corpus canónico exige seis kinds, perfiles completos, exit 42 y al menos cuatro partial-noheap.
- budget básico compara cinco métricas frontend. Perfiles canónicos son seis y assessment añade tolerancia,
  samples e identidad/hardware; backend/toolchain permanecen excluidos.
- diagnostic gate exige código `NS3`, bilingüe, span/fix-it/cascade y backtrace; catalog cubre cinco áreas y
  renderer trunca por bytes a terminalWidth.
- debug evidence exige nombres+tipos, scope/sequence points, O0/O2 y ocho kinds, nunca valueInvented.
- hidden-cost rechaza costo no explícito. Zero-cost exige fixture library-free, MIR+assembly, seis kinds y O0/O2
  con excepciones coherentes. Schema fijo: `nsc-semantic-tooling-v1`.

## 13. Iteración y generator ABI — `CL-LANG-011`

- iterator exige item/MoveNext/Current/cleanup y lending sólo por referencia.
- range count rechaza step cero o que se aleja, soporta inclusive y overflow; no materializa colección.
- comprehension nunca asigna implícitamente; structural mutation se prohíbe durante lending, element mutation no.
- legacy frame sólo incluye locals `livesAcrossYield`, valida alignment y usa state byte.
- full generator exige yield, item único, states 1..N, borrow tied to frame, intervals válidos y layout sin
  overflow. Storage nunca se eleva a heap implícito.
- cancellation devuelve slots vivos/droppables en reversa; resume 0→first, yield→next/completed y terminal/invalid
  no resume.
- fusion se rechaza en orden: eager, effects, traversal upgrade, repeated evaluation, lost short-circuit y
  borrowed buffering; result multipass sólo si ambos lo son.

## 14. Superficie histórica — `CL-LANG-012`

- tabla de 16 entradas clasifica Implemented, DeferredTo4 o RejectedFor3 y congela name+rationale.
- lookup lineal de enum externo retorna silenciosamente la primera entrada.
- manifest y diagnostics contienen etiquetas `3.0`/`4.0`; son implementación vigente, no evidencia de que esa
  política sea compatible con la decisión posterior de neutralidad de edición.

## 15. Modelo de memoria — `CL-LANG-013`

- decode acepta valores 0..4. Load prohíbe release/acq-rel; store prohíbe acquire/acq-rel; RMW acepta todos y
  fence prohíbe relaxed.
- compare-exchange restringe failure por success y además a órdenes válidos de load.
- data race es UB; lock-free Win64 sólo 8/16/32/64 naturalmente alineado.
- volatile no sincroniza y puede tear si desalineado o >64. Manifest congela esas reglas en texto.

## 16. Metaprogramación — `CL-LANG-014`

- introspection filtra non-public fuera del módulo y ordena sourceOrdinal; no requiere runtime metadata.
- attributes exigen identity/target y sólo deduplican los no repeatable por `identity@target`.
- derive key ordena field identities y length-prefix; hygiene compara spelling+scope.
- hermetic profile sólo permite archivos/env declarados; input identity ordena por identity pero no rechaza
  duplicados. Package diagnostic exige prefijo package y prohíbe `NS3`.
- comptime budget compara steps/bytes/depth, descuenta steps/bytes y no descuenta ni acumula depth.

## 17. Módulos — `CL-LANG-015`

- identidad Windows concatena package/module, normaliza slash y lowercase byte a byte.
- graph ignora Binding y busca ciclos sólo en Layout mediante DFS recursivo.
- imports validan target, alias policy, deduplican exactos, ordenan y detectan colisión case-insensitive sólo
  para nombres locales; scopes module/extensions/glob sin alias no colisionan anticipadamente.
- access: Private=same declaration/friend, Module=same module/friend, Package=same package/friend, Public=true.
- reexport conserva identidad original; availability compara architecture antes de deprecated.
- API fingerprint filtra public, ordena identity y concatena signature/effects/layout.

## 18. Numéricos y const memory — `CL-LANG-016`

- literals soportan base/suffix/separators, signo externo, contextual type y selección i32/i64/u64; el helper
  legacy magnitude reconoce decimal/0x/0b pero no 0o.
- arithmetic 1..64 usa dominio bit exacto para add/sub/mul/div/shifts, policies checked/wrapping/saturating/
  unchecked y marca UB sólo en overflow unchecked. Division cero y shift checked fuera de width fallan.
- conversions clasifican identity, constant exact, lossless, narrowing, sign, float/integer y enum; overload rank
  y overflow requirement son parte del resultado.
- FP strict/precise/fast congelan signed zero, NaN, contraction, reassociation, payload, denormals y consteval.
- strong numeric exige identity, un field, representation byte-sized <=64, transparent para FFI y Copy⇒Move;
  no wrapper runtime y conversion implícita sólo misma identity.
- constant memory asigna provenance monotónico, valida alignment, offset/deref dentro de allocation viva y evita
  double free. Union sólo lee active member.
- constant arithmetic rechaza UB y loop respeta budget; self-check barre las 65,536 sumas signed i8 contra las
  cuatro policies.

## 19. Ownership — `CL-LANG-017`

- Copy move es no-op. Non-copy whole move exige al menos un field inicializado y los apaga; field move valida
  índice/estado y prohíbe custom destructor.
- value state cuenta fields; drop flags sólo para partial y uno por field.
- places se traslapan si root y prefijo compartido coinciden; exclusive sólo si no overlap.
- output lifetime usa receiver o único input; cadena outlives depende del orden de primera coincidencia.
- cleanup es reversa exacta. Sólo borrow, pinned projection y destroy-in-place preservan pin.
- escape summary `escape-v1` length-prefix identity y enum list; decoder reinicia output y valida versión,
  longitud, separators, clases y trailing grammar.

## 20. Release y promoción — `CL-LANG-018`

- freeze base exige cuatro revisions. Syntax/semantic/Win64 añaden hashes, subsystem revisions y gates completos.
- semantic deferred milestones sólo acepta prefijos 3.1, 3.2 o 4.; Win64 exige target exacto `windows-x64`.
- conformance exige IDs únicos y al menos Mandatory; candidate exige tres tiers y cuatro evidencias de ejecución.
- specification parity exige IDs/requisitos/ejemplos; audit añade compatibility notes, glossary, links, samples e
  indexes.
- RC1 prohíbe feature, RC2 acepta fix/blocker, RC3 sólo blocker; cualquier accepted change reinicia gates.
- promotion exige baseline, issues cero, debug/release, docs, migration, reproducibility y hashes. Official añade
  hashes, branch exacta `v.3.0`, boundaries y smoke/install/samples/artifacts.
- `stableLanguageVersion` retorna `3.0.0`; target normativo `windows-x64`.

## 21. Smart pointers — `CL-LANG-019`

- cinco familias congelan name/thread/null/copy/clone/weak; lookup indexa array sin guard para enum externo.
- Move siempre pasa; implicit copy siempre falla. Clone/downgrade/upgrade validan familia, overflow y no
  resurrection. Release/reset son sólo Unique.
- cross-thread prohíbe Rc/WeakRc, exige payload Send/Sync y producer pinned.
- manifest/fingerprint son deterministas; writer crea parent y verifier normaliza CRLF.

## 22. Sum types y patterns — `CL-LANG-020`

- variant layout elige tag 1/2/4 por case count, valida nombre/alignment, alinea payload y detecta suma overflow.
- OR patterns exigen mismo count y mapa name→type/mode; duplicados en la alternativa izquierda no se rechazan
  explícitamente porque `emplace` conserva el primero.
- closed match marca arms inútiles, ignora guarded para cobertura y wildcard sin guard cubre todo.
- let-else inicializa sólo si else diverge. C union read permite active proof o unsafe context.
- metadata identity conserva qualified name y sólo case names length-prefixed, no payload layout.

## 23. Traits y operadores — `CL-LANG-021`

- trait exige qualified identity, no instance storage y signatures únicas/no vacías.
- coherence exige ownership de trait o type por módulo y un impl por trait+type, independientemente del module
  string del candidato.
- associated projections exigen key única y siguen cadena iterativamente con cycle detection.
- opaque returns deben ser no vacíos y todos iguales; dyn methods no generic ni Self by value.
- trait object Win64 es `{8,8,descriptor-map,null-zero,no-allocation}`; dispatch metadata distingue static/dyn.
- instance member gana; 0/1/>1 extensions produce missing/unique/ambiguous.
- tabla de 20 operadores distingue spelling+arity y ABI method. Lookup inverso y proyecciones son exactos; la
  sobrecarga histórica sin arity prefiere binary. `&&`, `||`, `??` y unknown no son overloadable.

## 24. Dependencias y complejidad

- Dependencias internas directas: `MemoryModel` e `IterationSemantics` desde coroutines.
- Dependencias externas: STL containers/algorithms/charconv/filesystem/streams y target Windows x64.
- La mayoría de validadores son O(n) u O(n log n). Rebuild closure es iterativo O(V·E) en peor caso.
- `moduleGraphResolvable` usa DFS recursivo y su profundidad depende del grafo.
- `integerSemanticsSelfCheck` es una barrida fija de 65,536 pares.
- Registries y state machines mutan estado; los demás helpers son puros salvo writers y error/result buffers.

## 25. Contradicciones y riesgos que no se ocultan

1. `Language3Surface`, `CompatibilitySemantics`, `IntegrationQuality` y `ReleaseSemantics` contienen gates,
   strings o metadata ligados a 3.x. La política posterior del usuario exige neutralidad de edición. La Fase 1
   documenta la discrepancia; no la resuelve ni cambia pruebas.
2. El componente conserva varios conceptos `unsafe`; la decisión posterior de reemplazarlos o relajarlos no se
   aplica retroactivamente durante esta auditoría.
3. La regla general contra recursión contradice el DFS recursivo de `moduleGraphResolvable`.
4. Tres lookups por enum (`expressionLoweringContract`, `smartPointerFamilyContract` y surface fallback) no
   comparten una política segura para valores externos.
5. Varias funciones expresan política declarativa futura; que el helper exista no demuestra integración en el
   compilador completo.

## 26. Reconstructibilidad

La implementación puede reconstruirse sólo si el test plan conserva:

- valores exactos, errores y prioridad de cada rechazo;
- matrices completas de enums, límites, orders, transitions y round-trips;
- orden de colecciones, cleanup, sorting y fingerprints;
- estado mutable antes y después de éxito y error;
- side effects de archivos y normalización CRLF;
- riesgos/fallbacks vigentes, aunque resulten candidatos a rediseño posterior.

La segunda pasada cerró 546 símbolos, 21 contratos y 286 casos lógicos parametrizables. `RECONSTRUIBLE`
describe suficiencia documental para reproducir la implementación vigente; no afirma que los casos estén
implementados, ejecutados ni cubiertos instrumentalmente.
