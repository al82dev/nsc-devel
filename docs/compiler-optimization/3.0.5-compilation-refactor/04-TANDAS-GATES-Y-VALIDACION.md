# 04 — Tandas, gates, medición y validación para 3.0.5

## 1. Metodología de entrega

Cada tanda se implementa, mide y cierra de forma aislada:

1. releer este expediente y declarar SCOPE LOCK de la tanda;
2. verificar rama, HEAD, status y diff; no invadir trabajo concurrente;
3. capturar medición/fixture previo dirigido;
4. hacer un solo cambio causal;
5. construir únicamente targets necesarios;
6. ejecutar pruebas O0 focales a través del `nsc.exe` construido cuando el contrato sea de lenguaje;
7. comparar conducta, métricas y determinismo;
8. escribir expediente y tracking;
9. crear commit local aislado;
10. continuar con la siguiente tanda habilitada.

No se ejecuta baseline completo entre tandas. Al consumir la campaña, el usuario ejecuta desde la
GUI la matriz completa. Un fallo activo abre una tanda correctiva menor; no se amontona con nuevas
optimizaciones.

Los nombres de pruebas incluidos aquí son **PROPUESTAS DE DISEÑO**, no pruebas existentes ni
autorización para crearlas durante esta fase documental.

## 2. Matriz de medición obligatoria

La matriz primaria usa un corpus controlado propio de NSC. Comienza con `Hola mundo` para medir el
costo fijo y añade fixtures de complejidad creciente —múltiples fuentes, genéricos y dependencias—.
La medición de fases que produjo 122/53/9 se conserva sólo como antecedente; no define por sí sola
la causa, el corpus ni el alcance.

| Caso | Estado inicial | Cambio |
|---|---|---|
| COLD | cache vacía controlada | ninguno |
| WARM-0 | segundo build con misma cache | ninguno |
| WARM-ROOT | cache caliente | un cuerpo privado de la unidad raíz |
| WARM-DEP-PRIVATE | cache caliente | un cuerpo privado de dependencia controlada |
| WARM-DEP-PUBLIC | cache caliente | una firma pública compatible de fixture |
| ABI | cache caliente | cambio de layout/ABI de fixture |
| FAIL | cache caliente | error sintáctico/semántico deliberado en fixture |
| J1/JN | misma entrada limpia | sólo `-j1` frente a `-jN` |

Por ejecución se conserva en `A:`:

- comando normalizado y configuración efectiva;
- identidad de compilador y target;
- tiempos de pared y CPU por fase/subfase;
- RSS pico;
- unidades, bytes, nodos y especializaciones;
- hits/misses y razones por capa;
- top de hotspots genéricos y LLVM;
- hashes de outputs y dumps normalizados;
- diagnóstico completo si falla.

Se hacen al menos cinco repeticiones después de una warm-up no contabilizada. Se reporta mediana,
p95 y dispersión. Una mejora inferior al ruido no se declara ganancia.

## 3. Tandas de observabilidad y baseline de rendimiento

### NSC305-P01 — Traza interna de genéricos

**Archivos/símbolos:** `ProjectCompilation::compileProjectGenerics`,
`ProjectGenericModel::build`, resolución, demanda, ensure y clonadores; `CompilationTelemetry`.

**Cambio:** timers jerárquicos y contadores del capítulo 02, sin alterar control de flujo.

**Pruebas focales:** serialización de trace; compilación de fixture genérico válido/inválido;
trace apagada/encendida produce mismos diagnostics y artefactos.

**GATE:** overhead con detalle apagado menor a 1 %; se obtiene top por template/método para el caso
real; suma de subfases compatible con timer padre.

**Rechazo:** si el contador necesita mutar semántica o si el trace cambia orden/scheduling.

### NSC305-P02 — Explicación de caché LLVM

**Archivos/símbolos:** `CompilationArtifactCache`, `ProjectOopObjectGenerator::generate`, reportes
EXE/LIB/DLL.

**Cambio:** `CacheLookupResult` con razón de miss por code unit; tiempo de fingerprint/restore/
lower/optimize/emit/store.

**GATE:** dos builds idénticos explican cada unidad; un objeto corrupto se reporta y reconstruye;
salida sin modo detallado conserva compatibilidad.

### NSC305-P03 — Corpus de rendimiento reproducible

**Cambio:** manifiesto sólo para medir el proyecto real y microfixtures de cardinalidad controlada.

**GATE:** COLD/WARM/J1/JN reproducibles desde A:, sin depender de timestamps ni paths temporales.

## 4. Tandas algorítmicas de genéricos

### NSC305-G01 — Índice por sintaxis y clave canónica

**Cambio:** construir `bySyntax` y `byCanonicalKey`; reemplazar `stateForSyntax` y la siembra
`find_if` sin tocar reglas de resolución.

**Complejidad:** `O(N×T)` a `O(N+T)` promedio en esos sitios.

**GATE:** mismos template records, ordinals, specializations y diagnostics; contador de scans
lineales de esos sitios igual a cero.

### NSC305-G02 — Índices de tipos

**Cambio:** índices módulo/qualified-name y visible-name/arity; `resolveTypeTemplate` devuelve
triestado.

**GATE:** corpus de homónimos, nested types, packs, aliases y cross-module mantiene resultados;
candidatos examinados depende de homónimos, no del total T.

### NSC305-G03 — Resolución única de métodos

**Cambio:** `GenericMethodResolution`; `processGenericCall`, dependencia y diagnóstico comparten el
resultado; índices por owner/name/arity/argc.

**GATE:** overloads, receiver-value, first winning rung, aliases/imports, owner ambiguity y
controlled-specialization equivalentes; una llamada no construye más de una ladder.

### NSC305-G04 — Scopes persistentes

**Cambio:** `ScopeId` y frames delta; eliminar copias profundas de `GenericLookupScope`.

**GATE:** mismo sombreado; conteo de copias de mapas/sets durante traversal igual a cero; memoria y
pared no empeoran.

### NSC305-G05 — Traversals iterativos de catálogo/dependencias

**Cambio:** `collectTemplates`, alias expansion, discovery y dependency collection sin recursión.

**GATE:** AST profundo controlado no desborda stack; orden de records/dependencies idéntico; gate
estático no detecta self-call en esas funciones.

### NSC305-G06 — Clonadores iterativos

**Cambio:** `cloneType`, `cloneNode`, pack collector y associated projection con frames.

**GATE:** golden estructural de cada clase de nodo/tipo, origins/ranges exactos, packs y errores
idénticos; cero recursión directa/indirecta en el cloner.

### NSC305-G07 — Planner transaccional

**Cambio:** `SpecializationSlot`, context por demanda, commit/abort; unificar ensure type/method.

**GATE:** no cambia nombres/ordinals; una falla no deja record/clon provisional; límites 64/16384
conservan diagnóstico; replay determinista.

### NSC305-G08 — Demand collector separado

**Cambio:** retirar `processNode`; discovery inmutable + rewrite map + commit.

**GATE:** mismo conjunto de demandas y AST final que G07; ninguna mutación durante discovery.

### NSC305-G09 — Ondas paralelas

**Cambio:** especializaciones independientes en scheduler con arenas/resultados privados.

**GATE:** hashes y diagnostics iguales entre j1/jN y 20 schedulings perturbados; speedup medible;
ThreadSanitizer equivalente si está disponible en toolchain futura, pero no sustituye pruebas.

## 5. Tandas de caché incremental

### NSC305-C01 — Cache de templates parseados

**Cambio:** C1 privada por hash de `templateSourceText`; serialización relocatable.

**GATE:** WARM-0 reporta hit para todas las plantillas sin cambios; AST dump idéntico; corrupción
produce miss/quarantine; no se modifica NSMETA.

### NSC305-C02 — Cache de catálogo/dependencias

**Cambio:** C2; snapshot sin punteros y validación de grafo visible.

**GATE:** WARM-0 omite collection/dependency scan de dependencias estables; cambio público invalida
sólo snapshots dependientes.

### NSC305-C03 — Cache de especialización semántica

**Cambio:** C3 por template fingerprint, argumentos, ABI y closure de interfaces.

**GATE:** un cambio exclusivo de la unidad raíz reutiliza las especializaciones de dependencias sin
cambios; un cambio en un template afectado invalida su cierre; ningún hit cruza argumentos o targets.

### NSC305-C04 — MIR por callable/partición

**Cambio:** C4 con interface/implementation fingerprints.

**GATE:** cambio privado recompila sólo el callable y sus dependientes reales; dumps restaurados idénticos;
el objeto existente recibe claves estables de partición.

### NSC305-C05 — Política, prune y recuperación

**Cambio:** presupuesto unificado, GC por publicaciones completas, lock multi-process y recovery.

**GATE:** kill durante store no deja hit parcial; dos procesos no corrompen store; límite se cumple.

## 6. Tandas LLVM

### NSC305-L01 — Perfil por code unit y balance

Medir costos reales; añadir peso histórico bounded. **GATE:** cada uno de los 53 s queda atribuido
a fingerprint/restore/lower/optimize/emit/verify/store y unidad.

### NSC305-L02 — `nodeCost` y walkers iterativos

Eliminar recursión de partición. **GATE:** mismas particiones con el modo legacy y misma salida.

### NSC305-L03 — Extraer runtime/types/debug/metadata

Movimiento puro del lowerer, un componente por commit. **GATE:** texto LLVM normalizado idéntico.

### NSC305-L04 — Expression machine iterativa

Migrar families de menor a mayor riesgo: literals/names; unary/binary; member/index; calls;
aggregate/construction; cleanup/exceptions/async. **GATE por familia:** fixtures focales y LLVM
idéntico; no avanzar con diferencia no explicada.

### NSC305-L05 — Particiones balanceadas y cache-aware

Planear unidades para maximizar reutilización y balance sin cambiar símbolos. **GATE:** ninguna
partición reconstruida si su MIR key no cambió; jN mejora pared y no aumenta RSS fuera del budget.

## 7. Tandas HIR/MIR y monolitos restantes

### NSC305-H01/H02 — Extraer HIR y migrar lowering iterativo

Primero movimiento puro, luego máquina de frames. Gates: dumps HIR, diagnostics y hashes.

### NSC305-M01/M02 — Extraer MIR y migrar lowering iterativo

Incluye tuple discovery sin lambdas recursivos. Gates: dumps MIR y LLVM posterior.

### NSC305-B01..B08 — Method bodies por familias

Una tanda por coordinator, scopes/flow, expression dispatch, calls, assignments/members,
construction/patterns, ownership/escape y statements iterativos. Cada tanda usa fixtures del
contrato que toca y compara el modelo/dump.

### NSC305-D01..D04 — Driver

Invocation, action dispatch, public-source emitter y artifact publication. No cambiar textos ni
exit codes en las tandas de movimiento.

### NSC305-R01..R03 — Parser

Separación física, types/patterns iterativos, expresiones Pratt iterativas. Sólo después de que el
perfil confirme que el riesgo se justifica.

## 8. Gates globales por tanda

Cada commit debe cumplir:

- worktree inicial auditado y cambios ajenos preservados;
- target focal construye con toolchain oficial;
- pruebas focales O0 correctas;
- salida/diagnósticos comparados con el commit padre;
- no se añadió recursión en producción;
- ninguna escritura fuera de A:;
- documentación con antes/después, alcance, limitaciones y comandos reproducibles;
- si hay rendimiento: cinco muestras, mediana/p95 y hardware/configuración;
- commit local aislado; sin push/pull/fetch.

## 9. Criterios de rechazo inmediato

Se revierte o corrige antes de avanzar si ocurre cualquiera:

- cambia sintaxis, ABI, NSMETA público o conducta 3.0.4;
- una caché acepta entrada sin verificar dependencias;
- warm build es rápido porque omitió diagnóstico o fase requerida;
- jN produce orden/hashes distintos a j1;
- aparece race, deadlock, lock global dominante u oversubscription;
- se mueve un monolito entero en un commit imposible de revisar;
- se declara mejora usando sólo una ejecución;
- se “soluciona” profundidad aumentando stack o límite recursivo;
- se desactiva verificación COFF/LLVM o se reduce cobertura;
- un hit/miss no puede explicar su causa.

## 10. Checkpoints de la campaña

### Checkpoint A — verdad del perfil

P01–P03 terminadas. Debe saberse exactamente dónde se consumen 122 s y 53 s. Si la evidencia
contradice este plan, se reprioriza antes de refactorizar.

### Checkpoint B — complejidad genérica

G01–G08 terminadas en j1. Objetivo: reducir el cold build sin depender de caché y eliminar
recursión/barridos globales. No se activa paralelismo hasta tener modelo transaccional.

### Checkpoint C — paralelismo

G09 y L01/L02. Objetivo: escalar con cores manteniendo determinismo y memoria.

### Checkpoint D — warm incremental

C01–C05. Objetivo: WARM-0 y WARM-ROOT en segundos, con razones de invalidación.

### Checkpoint E — backend y estructura

L03–L05, HIR/MIR y monolitos priorizados por perfil. Objetivo: reducir LLVM y dejar fronteras
mantenibles sin reescritura total.

### Checkpoint F — baseline final

El usuario corre desde la GUI la suite O0 y después el baseline O0/O1/O2/O3. Los fallos se
atienden en tandas correctivas; no se agregan optimizaciones mientras haya regresiones activas.

## 11. Definición de terminado para NSC 3.0.5

3.0.5 puede declararse estabilizable cuando:

1. el baseline de 3.0.4 fue preservado como salvavidas y no recibió features;
2. la GUI termina su matriz completa sin fallos atribuibles a NSC 3.0.5;
3. COLD, WARM-0 y WARM-ROOT tienen mediciones reproducibles desde `Hola mundo` hasta los fixtures
   controlados de complejidad creciente;
4. `generics` informa hotspots, evita barridos globales, no usa recursión y emplea jN de forma
   determinista;
5. una compilación warm de la unidad raíz no reparsea ni remonomorfiza dependencias sin cambios;
6. LLVM restaura toda partición elegible sin cambios y explica cada miss;
7. j1/jN producen mismos diagnostics, dumps y artefactos;
8. la caché sobrevive corrupción, cancelación y concurrencia sin devolver falsos hits;
9. los monolitos tocados quedaron divididos en componentes con contratos y tests focales;
10. cada tanda tiene expediente, tracking y commit local aislado;
11. no se introdujo sintaxis, cambio de ABI ni versionado NSMETA independiente;
12. los objetivos de pared acordados se cumplen o cualquier desviación queda respaldada por un
    perfil que identifica el siguiente cuello, no por una explicación genérica.

## 12. Primer paso de implementación recomendado

No empezar por cache ni por mover archivos. Empezar por **NSC305-P01 y P02 en un solo ciclo de
medición, pero en commits separados**. Sin hotspots internos y razones de miss, cualquier cambio
posterior podría optimizar el lugar equivocado o esconder trabajo detrás de una cache incompleta.

Después, ejecutar G01–G03: son cambios de complejidad local, comprobables y de bajo riesgo relativo.
Sólo con esas mediciones se decide si C1 (reparse de templates), G04/G05 (traversals/scopes) o una
plantilla específica merece la siguiente tanda.
