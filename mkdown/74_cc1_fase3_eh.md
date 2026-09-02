# CC1 — Fase 3: manejo de excepciones (tareas 32–39)

**Fecha:** 2026-08-31 · **Alcance:** el compilador. Nada más.
**Origen:** `NativeSharp_Milestone_CC1_Compiler_Correctness_Semantic_Consolidation_2026-08-31.md`,
hallazgos EH-001 (§23), EH-002 y EH-003 (§22 y §24).
**Tabla:** `MILESTONE_1_TASKS.md`, fase 3.

---

## 0. Punto de partida: G1 y G2 cerrados

La segunda corrida de Windows (2026-08-31 12:18) salió limpia:

```
100% tests passed out of 242
 57/242 Test  #57: nsc.version.identity ....... Passed
 58/242 Test  #58: nsc.cc1.integer-evaluator .. Passed
 59/242 Test  #59: nsc.cc1.correctness ........ Passed   9.03 sec
127/242 Test #127: nsc.version ................ Passed
```

Checklist **45/45, Failed 0**. Triage 16/16, cero vivos. Las tres fixtures CC1 en 42 a `-O0` y a
`-O2`. Los dos fallos de la corrida de las 12:06 eran expectativas de test mías, no del compilador,
y las tareas 74–76 los cerraron. Con eso G1 (constantes y `char`) y G2 (flujo) quedan cerrados y
esta fase arranca sobre un árbol verde.

---

## 1. Lo que hice antes de escribir una sola línea de backend

Esta fase es la primera que toca `src/LLVM/ProjectOopLlvmLowerer.cpp`. El árbol **no incluye las
cabeceras de LLVM**: `src/LLVM/LlvmCApi.h` declara a mano el subconjunto de la C API que se usa, y
llamar a una función de LLVM no declarada ahí falla en tiempo de COMPILACIÓN como identificador
desconocido. En la ronda de `switch` eso me costó una compilación entera, porque validé mi código
contra un stub que había escrito yo mismo — así que aquí lo primero fue el barrido:

```
usados: 142  declarados: 242
--- usados y NO declarados ---
(vacío)
```

Cero faltantes. Y el código nuevo de esta fase no introduce ni un símbolo `LLVM*` que no estuviera
ya en uso: `LLVMBuildBr`, `LLVMPositionBuilderAtEnd` y las llamadas que pasan por
`buildNoUnwindCall`/`appendBlock`. La comprobación es barata y se hace *antes*, no después.

---

## 2. EH-001 — la tarea 32 estaba mal planteada, y lo comprobé en vez de razonarlo

### El hallazgo

La salida normal de un `catch` cae por el bloque `eh.catch.handled.N`, que llama a
`__nativesharp_exception_clear` y suelta la excepción actual guardada en TLS. Un `break`, un
`continue` o un `return` desde ese mismo cuerpo se bajan directamente a su destino estructurado y
**no pasan por ese bloque**. La misma cláusula termina, entonces, con el TLS limpio o sucio según
cuál de sus salidas legales tomara el programa. Es una divergencia de invariante real.

### Por qué el plan original no la habría cerrado

La tarea 32, tal como yo la había escrito, decía: añadir `bool requiresExceptionClear` a
`FuncletContext` y marcarlo `true` «en los contextos creados para cláusulas `catch`».

Antes de escribirlo fui a ver **dónde se crean esos contextos**. Barrido de todos los `push_back`
sobre `funcletStack_` en el archivo: hay exactamente uno, en `emitExceptionalCleanupChain`, y
construye `FuncletKind::Cleanup`. **Nadie empuja una entrada `Catch`.** Desde el arreglo de los bugs
B-006/B-007, el `catchpad` retorna enseguida y el cuerpo real de la cláusula se ejecuta como bloque
padre ordinario — el propio comentario de `lowerRethrow` ya lo decía, y por eso el rethrow busca su
excepción en `activeCatchExceptions_` y no en la pila de funclets.

Consecuencia: un campo en `FuncletContext` habría sido **código inalcanzable**. La tarea se habría
cerrado en el papel, el hallazgo habría quedado marcado como resuelto, y no se habría medido nada.
Es exactamente el patrón que dejé anotado en §6 de `73_cc1_fases_0_1_2.md`: verificar lo que
construyo contra un modelo que también construí yo.

Hay una segunda consecuencia, que anoté en el código: el bloque de `catchret` +
`exception_clear` que ya existía dentro del bucle de desenrollado de `lowerStructuredExit` **no se
alcanza hoy**, porque su guarda exige `kind == FuncletKind::Catch`. Lo dejé en pie —es el código
correcto si algún día vuelve a empujarse una entrada `Catch`— pero anotado como lo que es, y no
como algo que esté corriendo.

### Dónde va la obligación entonces

En `activeCatchExceptions_`, que es la pila que sí sigue «estoy bajando el cuerpo de un catch». Le
añadí un segundo campo:

```cpp
struct ActiveCatchBody final
{
    LLVMValueRef exception = nullptr;
    std::size_t loopDepthAtEntry = 0;
};
```

`loopDepthAtEntry` es `loopStack_.size()` en el momento en que empezó a bajarse el cuerpo. Es lo que
distingue las dos cosas que un `break` puede significar dentro de un catch:

- si su marco de bucle **ya existía** cuando el cuerpo empezó, el salto aterriza fuera del cuerpo:
  es una salida del catch;
- si el marco se empujó **dentro** del cuerpo (un `while` o un `switch` escritos ahí), el salto se
  queda dentro: no lo es.

```cpp
bool structuredExitLeavesCatchBody(bool isReturn) const noexcept
{
    if (activeCatchExceptions_.empty()) return false;
    if (isReturn) return true;
    if (loopStack_.empty()) return false;
    return (loopStack_.size() - 1U) < activeCatchExceptions_.back().loopDepthAtEntry;
}
```

`return` sale de todas las cláusulas que lo envuelven por definición, y por eso no consulta
`loopStack_`. Para `break`/`continue` basta mirar la cláusula más interna: una salida que no
abandona la más interna no abandona ninguna, y una sola llamada limpia el único slot TLS por muchas
que abandone.

### Cómo se emite, y por qué en un bloque propio

```cpp
void emitCatchBodyExceptionClear()
{
    LLVMBasicBlockRef leaveBlock = appendBlock("eh.catch.leave");
    LLVMBuildBr(builder_, leaveBlock);
    LLVMPositionBuilderAtEnd(builder_, leaveBlock);
    buildNoUnwindCall(exceptionClearFunctionType_, exceptionClearFunction_, nullptr, 0, "");
}
```

Podría emitirse en línea. Va en un bloque propio para que **lo que produjo la obligación tenga
nombre en el IR** y pueda comprobarse con un test en lugar de razonarse. `-O2` pliega el bloque; la
llamada sobrevive. Ver §4.

La llamada se hace en `lowerStructuredExit` **antes** de las tres formas de salida —la rama simple,
el recorrido de cleanups y el desenrollado de funclets— y en las tres por igual, porque la
divergencia que se está cerrando es precisamente que la simple se la saltaba.

Va **antes** de los cleanups y no después, igual que ya hacía `eh.catch.handled.N` (limpia y luego
entra al `finally`). La razón no es simetría: un destructor que corre de salida puede lanzar y
publicar una excepción actual nueva, y limpiar después borraría esa en lugar de ésta.

### La tarea 34, resuelta midiendo

La tarea 34 se preocupaba de una doble limpieza. No la hay, y no por una guarda nueva:

1. una salida estructurada **termina el bloque**, así que el `if (!handlerTerminated)
   LLVMBuildBr(builder_, handledBlocks[index])` no se emite en ese camino;
2. la limpieza del `catchret` es inalcanzable hoy (§2);
3. y aunque las dos coincidieran, `__nativesharp_exception_clear` sólo escribe null en un slot TLS
   —lo comprobé en `src/Runtime/WindowsRuntimeBuilder.cpp`, `buildExceptionClear`—, así que dos
   limpiezas son idempotentes entre sí.

Eso convierte «limpiar dos veces» en un no-problema, no en algo contra lo que haya que blindarse.

---

## 3. Honestidad sobre qué se puede medir de EH-001

**No hay hoy un fallo observable desde NativeSharp que esta corrección arregle.** Lo digo aquí
antes de las fixtures para que nadie lo lea al revés.

`__nativesharp_throw` publica la excepción nueva en el mismo slot TLS *antes* de levantarla
(`TlsSetValue(index, exception)` incondicional), así que un slot sucio queda pisado por el
siguiente `throw`. Los dos únicos lectores del slot —el filtro RTTI de cada cláusula tipada y la
lectura de `caughtException` justo tras el `catchpad`— corren siempre después de una publicación.
El documento de auditoría lo dice con el mismo cuidado: «aunque todavía se necesite fixture runtime
para medir la consecuencia observable exacta, estáticamente la diferencia existe».

Se arregla igual, y por dos razones que no son «por si acaso»:

- el slot es un recurso del programa, y su estado al terminar una cláusula no puede depender de por
  qué puerta salió;
- cuesta una llamada que `-O2` ni siquiera conserva en los caminos donde no hace falta.

Lo que sí es medible se mide, y en el sitio donde es medible: **en el IR** (§4), no en la ejecución.

---

## 4. Cómo se mide EH-001 — en el IR, no ejecutando

Desde NativeSharp no hay forma de leer el slot TLS, así que ninguna fixture de ejecución puede
distinguir «limpio» de «sucio». Sí es observable el IR emitido. `nsc.cc1.correctness` gana tres
comprobaciones nuevas que bajan un programa a LLVM con `ProjectCompilation` +
`ProjectOopLlvmLowerer` y cuentan los bloques `eh.catch.leave.N` **cuyo cuerpo contiene de verdad la
llamada** —un bloque vacío cuenta 0, que es lo que hace útil contarlos en vez de buscar el nombre
de la función suelto por el módulo, cosa que pasaría igual por los `eh.catch.handled.N` que ya la
llamaban antes de este cambio:

| Caso | Forma | Esperado |
|---|---|---|
| `cc1-eh-structured-exit-clears` | un catch con `continue`, `break` y `return` | **3** |
| `cc1-eh-fallthrough-unchanged` | un catch que sólo termina por fallthrough | **0**, y la limpieza de la salida normal sigue presente |
| `cc1-eh-inner-loop-break-is-not-an-exit` | un `break` de un `while` abierto dentro del catch | **0** |

Los dos negativos son los que convierten al positivo en una medida y no en una coincidencia. El
tercero es el que mide `loopDepthAtEntry`: sin él, «emitir siempre que haya un catch activo»
pasaría el primero y sería incorrecto.

---

## 5. EH-002 y EH-003 — cinco fixtures de ejecución

Todas devuelven **42** cuando el compilador está sano, y un código distinto por propiedad rota.
Todas se ejecutan a `-O0` y a `-O2`.

### `tests/cc1/c4_catch_continue/` — tarea 35

`while` → `try`/`catch` cuyo catch termina en `continue`, y una segunda excepción **de otro tipo**
en una vuelta posterior, que es donde una excepción vieja que sobreviviera al catch tendría cómo
confundirse con la nueva. Incluye un `while` abierto **dentro** del catch cuyo `break` no debe
contar como salida del catch, y una segunda vuelta con los dos tipos lanzados en orden inverso, para
que la cláusula que atrapa dependa del filtro RTTI y no de quién lanzó primero.

### `tests/cc1/c5_catch_break/` — tarea 36

Tres bloques: `break` simple desde el catch; `break` desde un catch **con `finally`**, contando que
el cleanup corre exactamente una vez por vuelta (ni cero —salida saltándose el cleanup— ni dos); y
un `switch` dentro del catch cuyo `break` local no abandona la cláusula, comprobado por lo que se
ejecuta *después* del switch dentro del cuerpo.

`break` y `continue` no comparten destino ni profundidad de funclet en el backend —`LoopTargets`
guarda las dos por separado— así que medir sólo una dejaría la otra sin cubrir.

### `tests/cc1/c6_catch_return/` — tarea 36

`return` desde el catch cruzando un `finally`, y desde un catch **anidado** cruzando dos, en orden
de dentro afuera. Es la salida que abandona todas las cláusulas a la vez, y la única en la que el
valor tiene que derramarse en el hueco de retorno antes de recorrer los cleanups o se pierde por el
camino. Termina lanzando y atrapando de nuevo, para fijar que volver por esa ruta no deja el estado
de excepción del hilo en una forma que estorbe a la siguiente.

### `tests/cc1/c7_finally_switch/` — tarea 37, **la corrección del auditor**

Yo había propuesto `bucle → catch → switch` como la prueba de que `funcletDepth` y
`continueFuncletDepth` son de verdad dos números distintos. **No lo demuestra**, y la corrección es
del auditor externo: desde B-006/B-007 el cuerpo de un catch es bloque padre ordinario, así que un
switch escrito ahí no está a mayor profundidad de funclet que el bucle que lo envuelve. Los dos
valores coinciden.

El caso quirúrgico es un `switch` dentro de un funclet de **cleanup**, que es lo único que hoy sí
empuja en `funcletStack_`:

```
bucle exterior
  try
    try { throw }                                  <- entra por el camino excepcional
    finally { switch { ... break ... }  resto }    <- el finally corre COMO FUNCLET
  catch { ... }                                    <- y la excepción llega hasta aquí
```

Se fija, en este orden: que el `break` del switch **no** abandona el finally; que lo que sigue al
switch dentro del finally sí se ejecuta; que la excepción sigue propagándose y la atrapa el catch
exterior; y que el bucle exterior da todas sus vueltas. La fixture incluye además la misma forma
**sin** excepción, porque el cuerpo del `finally` se emite dos veces en el código generado (una
copia para la salida normal y otra como funclet) y sólo la segunda vive dentro de un funclet: sin
las dos, un fallo en una se confundiría con la otra.

### `tests/cc1/c8_loop_catch_switch/` — tarea 38

`bucle → catch → switch` con `break` local y `continue` al bucle exterior. Sigue siendo
indispensable —es la forma en la que los dos destinos van a sitios distintos y sólo uno abandona el
catch— pero su cabecera dice explícitamente qué **no** demuestra, para que nadie la lea como la
prueba de EH-003. Lo que se cuenta es lo que hay *después* del switch dentro del catch, porque
confundir un destino con el otro es un fallo silencioso: el programa sigue corriendo, sólo que
ejecuta o se salta el resto del cuerpo.

Repite la forma con los dos destinos intercambiados (`default` sale por `break`), de modo que un
cruce entre `break` y `continue` daría cuentas distintas en los dos bloques y no la misma.

### Tarea 39 — registro

Las cinco quedan registradas en los dos sitios: en `run_windows_checklist.bat` (ejecución real en
Windows, comparando código de salida) y en `nsc.cc1.correctness` (compilación y enlace, más
ejecución bajo Wine donde lo haya).

---

## 6. Archivos tocados

| Archivo | Qué |
|---|---|
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | `ActiveCatchBody`; `activeCatchExceptions_` cambia de tipo; `structuredExitLeavesCatchBody()` y `emitCatchBodyExceptionClear()` nuevas; llamada en `lowerStructuredExit`; `loopStack_.size()` registrado en el push de `lowerTryRegion`; `lowerRethrow` lee `.back().exception`; anotación sobre la limpieza inalcanzable del `catchret` |
| `tests/unit/Cc1CorrectnessTests.cpp` | `lowerToLlvmText()` y `countCatchLeaveBlocksThatClear()`; cinco `checkFixture` nuevas; tres comprobaciones de IR |
| `tests/cc1/c4_catch_continue/` | fixture nueva (`Main.ns` + `.nsproj`) |
| `tests/cc1/c5_catch_break/` | fixture nueva |
| `tests/cc1/c6_catch_return/` | fixture nueva |
| `tests/cc1/c7_finally_switch/` | fixture nueva |
| `tests/cc1/c8_loop_catch_switch/` | fixture nueva |
| `run_windows_checklist.bat` | cinco `run_case` nuevos en la sección CC1 |

Sin cambios en `CMakeLists.txt`: las fixtures se leen desde `NSC_SOURCE_DIR` y no necesitan target
propio.

---

## 7. G3 cerrado — la corrida

**Corrida de Windows del 2026-08-31 13:06.**

```
Passed: 55
Failed: 0
```

El checklist pasa de 45 a 55 casos: las diez nuevas son las cinco fixtures de esta fase a `-O0` y
a `-O2`, **todas en 42**.

```
[ OK ] cc1_c4_catch_continue    -O0 / -O2 -- exit 42
[ OK ] cc1_c5_catch_break       -O0 / -O2 -- exit 42
[ OK ] cc1_c6_catch_return      -O0 / -O2 -- exit 42
[ OK ] cc1_c7_finally_switch    -O0 / -O2 -- exit 42
[ OK ] cc1_c8_loop_catch_switch -O0 / -O2 -- exit 42
```

`ctest` **242/242, 100%**. `nsc.cc1.correctness` pasa y sube de **9.03 s a 22.95 s**, consistente
con cinco fixtures nuevas más tres bajadas a IR. El log de build confirma que los dos archivos
tocados se recompilaron —`[8/92] Cc1CorrectnessTests.cpp.obj`, `[13/92]
ProjectOopLlvmLowerer.cpp.obj`— sin errores ni avisos. Triage 16/16, cero vivos.

### Los tres riesgos, medidos

Los anoté en §7 antes de conocer el resultado, que es la única forma de que una predicción valga
algo. Ninguno se materializó:

1. **`switch` dentro de un funclet de cleanup** (c7) — la combinación que este backend nunca había
   emitido. `LLVMVerifyModule` la acepta y el programa ejecuta en 42 a los dos niveles. El `break`
   se queda dentro del funclet, el resto del `finally` corre, y la excepción llega al catch
   exterior.
2. **`break`/`continue` desde un catch con `finally`** (c5, segundo bloque) — la ruta `complexExit`
   con ordinales de cleanup, hasta ahora sólo ejercida con `return`. Correcta: el cleanup corre
   exactamente una vez por vuelta.
3. **Cinco programas nuevos de NativeSharp** — ninguno se topó con un límite del frontend. No hay
   nada que anotar en `69_huecos_de_runtime_y_lenguaje.md` por esta fase.

### Lo que la corrida NO midió, y queda anotado

`TestContext::result()` imprime `PASS: <n> checks` en stdout, pero `ctest` no lo captura en
`02_ctest.log`. Desde los logs se ve que `nsc.cc1.correctness` **no falló ninguna** comprobación,
no **cuántas corrió**. El salto de 9.03 s a 22.95 s y las líneas de recompilación son evidencia
fuerte de que el bloque nuevo está dentro, pero son evidencia indirecta: un bloque de tests que
desapareciera de un `.cpp` pasaría igual de verde.

Es el mismo patrón que llevo tres rondas anotando, en su versión menos grave: la señal que tengo
no es exactamente la propiedad que quiero. Queda como **tarea 77, fase 5**, con ID propio y no
como arreglo «de paso» — hacer que el checklist ejecute los binarios de test directamente y
registre el conteo en `SUMMARY.txt`.

### Estado

Fase 3 cerrada, 8/8. **G3 cerrado.** Milestone 43/77. Sigue la fase 4A (`sizeof`, tareas 40–46).
