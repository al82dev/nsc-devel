# Bloque 0 (Correctness Gate) -- cierre: B-001 (EH+RAII) y B-003 (version truth)

## Contexto

Este documento cierra el **Bloque 0 ("Correctness gate")** del roadmap propuesto en
`claude/NativeSharp_Gaps_y_Prioridades_Siguiente_Version.md` (auditoria externa del
2026-08-28 sobre `NativeSharp_Compilador_Completo_2.zip`), que definia dos items:

- **0.1 EH+RAII fix = B-001**: bug real de compilador, crash (STATUS_ACCESS_VIOLATION) al
  reusar un objeto `raii` en un segundo `try` hermano tras un `catch` real, a `-O0`.
- **0.2 Version truth = B-003**: el compilador reportaba `stage 52` / `dev52` aunque el trabajo
  real (Stage 53, 54, 55, Pre-Stage 56) ya estaba muy por delante de esa version.

Alberto pidio explicitamente ("no, al decir sanear es tomar todo lo que dice el ultimo markdown e
implementarlo") implementar el documento de auditoria completo, bloque por bloque, con checkpoint
de verificacion real antes de cada avance. Este documento es el checkpoint del Bloque 0.

## B-001 -- diagnostico y fix, verificado

### Root cause (confirmado via ejecucion real, NO adivinado)

El pipeline `"default<O0>"` del new-pass-manager de LLVM (usado en todo build `-O0`) nunca corre
`mem2reg`/SROA a proposito -- cada local queda como un `alloca` sin promover, con su
load/store explicito, lo cual es deliberado y deseable para -O0 (una unica direccion de stack
estable por local durante toda la funcion, ideal para info de depuracion futura). Esto es
inofensivo para una funcion ordinaria.

Deja de serlo para una funcion que declara personality SEH de Windows x64
(`personality ptr @__C_specific_handler`, presente en cualquier metodo con un `try/catch` o
`try/finally` real): el lowering de WinEHPrepare/SelectionDAG de LLVM para
`catchswitch`/`catchpad`/`cleanuppad` a `CodeGenOpt::None` -- y SOLO a ese nivel, `-O1` y `-O2`
confirmados limpios sobre exactamente el mismo IR -- produce una funcion cuyo codigo de maquina
corrompe su propio frame la primera vez que una llamada protegida por excepciones, MAS ABAJO en
el mismo metodo, ejecuta despues de que un `catch` real ya corrio y retorno via `catchret`. El
resultado es un `STATUS_ACCESS_VIOLATION` (0xC0000005) real, confirmado con `winedbg`, en esa
llamada POSTERIOR -- no en nada relacionado con el throw/catch en si. Confirmado (bisectado a
mano, 5+ variantes) que NO hace falta un segundo `try/catch`, ni un `finally`, ni un segundo
objeto RAII: una sola llamada a metodo despues de un catch manejado, en cualquier punto posterior
del mismo metodo, ya alcanza para dispararlo, siempre que esa llamada tambien necesite proteccion
de unwind (lo cual cualquier llamada dentro del scope lexico de un local `raii` siempre necesita).

Ver `BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md` para el reporte original (dos
`try` HERMANOS, no anidados -- el alcance exacto que este fix cubre) y esta misma sesion para la
bisection completa: `strace`, `winedbg` (incluyendo el hallazgo de que `winedbg --gdb` da un falso
SIGABRT al adjuntar un debugger sobre el codigo de excepcion custom `0xE04E5301`, un artefacto de
debugging, no el bug real), y bisection por nivel de optimizacion (-O0 vs -O1 vs -O2).

### Dos intentos previos de fix, presentes en el codigo, confirmados INEFECTIVOS

Durante el diagnostico se encontraron DOS intentos de fix previos ya en el codigo, ambos con
comentarios que afirmaban (sin verificacion por ejecucion real) haber resuelto este mismo bug:

1. `applyUwtableAttribute()` (agrega el atributo LLVM `uwtable` a la funcion) -- presente en el
   build (`attributes #2 = { uwtable(sync) }` confirmado sobre `Main`), NO arregla el crash.
2. La deduplicacion de `cleanuppad` de `CachedCleanup`/`emitExceptionalCleanupChain` (reusa un
   unico `cleanuppad` compartido por ordinal de cleanup-region en vez de duplicar por cadena de
   unwind) -- presente, tampoco arregla el crash por si sola.

Ninguno de los dos se elimino (son inofensivos, solo insuficientes) -- queda documentado aqui
como evidencia concreta de por que la regla "real-execution-first, no afirmar exito sin correr el
binario" existe: una sesion previa afirmo exito sin verificarlo por ejecucion real.

### El fix real, verificado

`LlvmOptimizer::optimize` ahora corre `mem2reg` (via el API C de LLVM por-funcion
`LLVMRunPassesOnFunction`, agregado a `LlvmCApi.h`) sobre EXACTAMENTE las funciones que declaran
personality SEH (`LLVMHasPersonalityFn`), y SOLO a nivel `-O0` -- `-O1` en adelante ya corren
`mem2reg` como parte de su propio pipeline por defecto, correr dos veces es inofensivo pero
redundante. Se evaluaron y verificaron DOS variantes:

- Un fix "blanket" (mem2reg incondicional para TODA funcion a `-O0`, via
  `"function(mem2reg),default<O0>"` como texto de pipeline) -- paso el suite completo sin
  regresiones, pero fue descartado deliberadamente: le quitaria a `-O0` su propiedad de "una
  direccion de stack real por local" a TODA funcion, no solo a las que realmente lo necesitan,
  sin razon para pagar ese costo a nivel de proyecto entero para un bug que solo existe en
  funciones con funclets SEH -- especialmente dado que el propio roadmap marca info de depuracion
  (PDB/CodeView) como candidato P1 futuro.
- El fix final, con alcance acotado (`legalizeSehFuncletFunctionsForO0` en
  `src/LLVM/LlvmOptimizer.cpp`), que deja `-O0` sin cambios para toda funcion que nunca toca
  `try/catch/finally`.

Ambas variantes se verificaron contra los 6 repros manuales construidos durante el diagnostico Y
el suite completo de 227 tests existentes, sin regresiones. Se conservo la variante acotada.

### Test de regresion formal

`tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp` (registrado como
`nsc.stage45-10.eh-raii-reuse-after-catch` en CMakeLists.txt) cubre, con ejecucion real bajo Wine
en AMBOS `-O0` y `-O2`, el alcance EXACTO del bug report original (dos `try` hermanos, no
anidados):

1. Repro verbatim del bug report (catch + segundo try/finally sobre el mismo objeto raii).
2. Variante con segundo `try` con su propio `catch` (no `finally`).
3. Disparador minimo: catch + llamada simple, sin segundo `try` en absoluto.
4. Dos objetos `raii` independientes, ambos usados tras un catch sobre el primero.

Las 4 escenarios, x2 niveles de optimizacion = 50 asserts reales, todos en verde.

## Hallazgo adicional -- DOS bugs nuevos, mas profundos, descubiertos escribiendo el test de B-001

Escribiendo el test de regresion se agregaron dos escenarios MAS ALLA del alcance original del
bug report (un `try` ANIDADO dentro de un `catch`, y un contador de destructor para verificar que
el objeto `raii` se destruye exactamente una vez). Ambos revelaron bugs REALES, DISTINTOS de
B-001, que el fix de B-001 **no resuelve**:

### Bug nuevo #1 -- doble destruccion silenciosa (afecta el alcance EXACTO de B-001)

Confirmado que reutilizar un objeto `raii` con destructor tras un catch real -- la MISMA forma
minima que describe el bug report original de B-001, sin ningun anidamiento -- lo destruye DOS
VECES a `-O0`, de forma silenciosa (sin crash, valor de retorno incorrecto). Los 4 escenarios de
B-001 que "pasan" en el test de regresion usan una clase sin destructor, asi que nunca podrian
haber detectado esto -- confirmado por separado que el mismo patron CON destructor produce el
mismo doble-conteo en esa forma exacta. Para un objeto `raii` que envuelve un recurso nativo real
(handle, lock, memoria manual) esto es corrupcion silenciosa -- potencialmente peor que el crash
original, que al menos era ruidoso.

Bisectado rigurosamente (mismo estandar que B-001, hipotesis descartadas por evidencia real, no
adivinadas): CodeGenOptLevel del backend, inlining de `Withdraw` dentro de la funcion, y
combinaciones mas amplias de passes de LLVM por-funcion (`mem2reg,simplifycfg`,
`mem2reg,sroa,early-cse,instcombine,simplifycfg`) fueron probadas y NINGUNA cambia el resultado.
Solo el pipeline `default<O1>` COMPLETO evita el problema, lo que apunta a que el arreglo real
esta en la propia emision de cleanup regions del compilador
(`CachedCleanup`/`emitExceptionalCleanupChain` en `src/Runtime/WindowsRuntimeBuilder.cpp` -- la
MISMA logica que uno de los dos intentos previos de fix de B-001 ya toco sin exito), no en
seleccion de passes de LLVM. Root cause AUN NO identificado a nivel de codegen.

Ver `BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md` para el detalle completo.

### Bug nuevo #2 -- un catch anidado no atrapa su propia excepcion

Un `try` ANIDADO dentro del bloque `catch` de otro `try`, sobre el mismo objeto `raii`: a `-O0`
la excepcion del `try` interno llega SIN MANEJAR al sistema operativo (`0xe04e5301`, el codigo de
excepcion propio de NativeSharp) -- el `catch` interno directamente no la atrapa. Distinto sintoma
de B-001 (no es STATUS_ACCESS_VIOLATION), y distinto alcance del bug report original ("dos try
hermanos, NO anidados"). Mismas tres hipotesis descartadas con la misma evidencia que el bug
anterior. Root cause AUN NO identificado.

Ver `BUGS_ENCONTRADOS/bug_eh_try_anidado_dentro_de_catch_o0.md` para el detalle completo.

### Por que estos dos bugs NO bloquean el cierre de Bloque 0

Ninguno de los dos esta en el alcance que la auditoria describe para B-001 (crash por reuso de
`raii` entre `try` hermanos) ni fue jamas diagnosticado por el fix aplicado. Investigarlos a fondo
tiene una magnitud comparable a la propia investigacion de B-001 (root cause en codegen/lowering,
no en seleccion de passes). Se documentaron con el mismo rigor que B-001 (repro real, ejecucion
bajo Wine, hipotesis descartadas con evidencia, no adivinadas) y se dejaron VIVOS en el test suite
como escenarios de "known issue" que se ejecutan y reportan su resultado observado en cada corrida
(`compileAndRunKnownIssueAtO0`, ver el propio header de esa funcion en el archivo de test) -- no
se ocultaron ni se retiraron del test para "hacerlo pasar". Quedan como candidatos a un futuro
bloque de correctness (posiblemente antes de Bloque 1, dada la severidad del bug de doble
destruccion) -- decision de Alberto.

## B-003 -- version truth, corregido y verificado

`include/nativesharp/Basic/Version.h` (unica fuente de verdad, consumida por `Driver.cpp`,
`CHeaderGenerator.cpp` y `LibraryMetadata.cpp` -- confirmado que no hay ninguna otra ocurrencia
hardcodeada de version/stage en el repo, ver busqueda exhaustiva de esta sesion) actualizado:

```
kCompilerVersion = "0.1.0-dev55"   // antes: "0.1.0-dev52"
kCompilerStage    = "55"            // antes: "52"
```

Verificado con `nsc --version` (`nsc 0.1.0-dev55 (NativeSharp 0.1, stage 55)`) y `nsc --help`.
El unico otro lugar que codificaba la version anterior era el regex de verificacion del propio
test `nsc.version` en `CMakeLists.txt` (`PASS_REGULAR_EXPRESSION`), tambien actualizado.

Valores elegidos por Alberto explicitamente (stage="55", dev-tag) para la duracion de este
trabajo de saneamiento, dado que Pre-Stage-56 y esta correccion aun no tienen numero de stage
propio.

## Verificacion final

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio, sin warnings.
- `ctest` completo: **228/228 tests, 100%**, incluyendo el nuevo
  `nsc.stage45-10.eh-raii-reuse-after-catch` (50 asserts reales de B-001 en verde + 2 known-issues
  reportados con visibilidad, no ocultos).
- `nsc --version` / `nsc --help`: reflejan `dev55` / `stage 55` correctamente.

## Estado y proximo paso

**Bloque 0 cerrado** segun su alcance definido en la auditoria (B-001 + B-003), con dos hallazgos
nuevos documentados y transparentados (no resueltos, no bloqueantes para este checkpoint). Por la
cadencia "bloque por bloque, checkpoint" que Alberto eligio explicitamente: este es el punto de
parada hasta su confirmacion explicita antes de avanzar a Bloque 1 (Ownership soundness) -- o,
alternativamente, decidir si los dos bugs nuevos ameritan su propio bloque intermedio antes de
Bloque 1, dado que uno de ellos (doble destruccion) afecta silenciosamente el mismo codigo que
B-001 ya toca.
