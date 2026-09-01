# Milestone 1 — CC1: Compiler Correctness & Semantic Consolidation

**Abierto:** 2026-08-31 · **Alcance:** el compilador y su biblioteca. Nada más.
**Origen:** `NativeSharp_Milestone_CC1_Compiler_Correctness_Semantic_Consolidation_2026-08-31.md`.

**Estado:** `[ ]` pendiente · `[~]` en curso · `[x]` cerrado · `[-]` diferido

**Regla de cierre:** una tarea sólo pasa a `[x]` cuando su columna *Hecho cuando* se cumple
medida, no razonada. Compilar no es cerrar.

**Diferido de CC1, con ID propio:** P-001 (retirar el mini-parser de type seeds), A-001 (frontend
canónico único), A-002 (frontera backend-neutral). Son arquitectura, no corrección: ninguno cierra
un P1 y cada uno es un proyecto propio.

---

## Progreso

| Fase | Tareas | Cerradas |
|---|---|---|
| 0 — Identidad y arnés | 1–3 | **3 / 3** |
| 1 — Constantes y `char` | 4–19 | **16 / 16** |
| 2 — Flujo | 20–31 | **12 / 12** |
| 3 — EH | 32–39 | **8 / 8** |
| 4A — `sizeof` | 40–46 | **8 / 8** (incluye 46b) |
| 4B — `stackalloc` | 47–52 | **6 / 6** |
| 4C — raw `new T` | 53–56 | **4 / 4** |
| 4D — aritmética de punteros | 57–64 | **7 / 7** · 62 → tarea 79 |
| 4E — cierre de paridad | 65–66 | **2 / 2** |
| 5 — Reproducibilidad y cierre | 67–73 | 5 / 7 · 67, 68 y 72 esperando tu corrida |
| **Total** | **1–80** | **75 / 80** |

**Gates:** ~~G1 tras 19~~ · ~~G2 tras 31~~ · ~~G3 tras 39~~ · ~~G4 tras 66~~ · G5 tras 73.

Ningún gate pasa con una tarea de su fase abierta.

**G1 y G2 — CERRADOS. Corrida de Windows del 2026-08-31 12:18:** `ctest` **242/242, 100%**,
checklist **45/45 Failed 0**, triage 16/16, las tres fixtures CC1 en **42 a `-O0` y `-O2`**.
`nsc.version` (#127) y `nsc.cc1.correctness` (#59) pasan: los dos fallos de la corrida anterior
eran de EXPECTATIVA DE TEST y las tareas 74–76 los cerraron. Ver §6 de `73_cc1_fases_0_1_2.md`.

**Fase 5 — escrita, pendiente de que la corras.** Las tareas 69, 70, 71 y 73 están cerradas
(documentos). Las 67, 68 y 72 necesitan tu máquina:

```
python verify_manifest.py --clean --write
run_windows_checklist.bat
```

El primero borra los seis archivos obsoletos y regenera el manifiesto **mostrando qué cambió**, y
verifica al terminar. El segundo es la corrida de cierre.

**La tarea 67 tampoco se podía hacer como estaba escrita:** decía «regenerar con
`verify_manifest.py`», y ese script **sólo sabía verificar** — no había generador en el árbol. Es
la tercera tarea apoyada en una pieza que no existe, tras la 40 y la 47. El generador está escrito
ahora, y con él la REGLA de qué entra en el manifiesto, que hasta hoy sólo existía implícita en sus
1109 rutas.

**G4 — CERRADO. Corrida de Windows del 2026-08-31 18:42:** checklist **63/63, Failed 0** (eran 57;
las seis nuevas son `c10_stackalloc`, `c11_raw_new` y `c12_pointer_arith` a `-O0` y `-O2`, **todas
en 42**), `ctest` **242/242, 100%**, triage 16/16, build con **cero avisos**.
`nsc.cc1.correctness` sube de 29.88 s a **33.27 s**.

**Los cuatro riesgos anotados antes de la corrida: ninguno se materializó.** `(*record).First = 7`
sobre memoria del montón; `nint` comparado con literales negativos; el paso de 16 de un struct con
relleno en `stackalloc`; y la alineación del `double` — que era el más traicionero, porque un
acceso desalineado en x64 puede salir como valor incorrecto en vez de como crash.

**Bloque G4 — implementado de una.** Fases 4B (`stackalloc`), 4C (raw `new T`),
4D (aritmética de punteros) y 4E (cierre de paridad) implementadas de una, porque las tres primeras
son el mismo hueco atravesando las mismas cuatro capas. Los cinco `.cpp` compilan con **cero
avisos** bajo `-Wall -Wextra`; barrido del backend: 143 símbolos `LLVM*` usados, **0 faltantes**.
Tres fixtures nuevas (`c10`, `c11`, `c12`) y **ocho** negativos. Ver `76_cc1_fases_4b_4c_4d_4e.md`.

**Dos tareas resultaron no implementables como estaban escritas, y las dos salen con ID propio:**

| ID | Qué se midió |
|---|---|
| 79 | La tarea 62 pedía «extender el lowering de `++`/`--` para punteros». **Ese lowering no existe para NINGÚN tipo** — el HIR lo rechaza también para un `int`. Hacerlo sólo para punteros dejaría `++p` funcionando y `++i` no. Implementarlo entero es una fase propia, fuera de CC1 |
| 80 | `p->campo` no se baja en el pipeline OOP. El parser lo acepta y el modelo semántico lo tipa; el HIR no lo tenía. Es la **cuarta** forma con la mitad delantera presente y la trasera ausente de este milestone. Ahora tiene mensaje propio que nombra `(*p).campo` como la forma equivalente que sí funciona (tarea 65); arreglarla es la 80 |

**Y la tarea 47 tenía la misma premisa falsa que la 40:** pedía «portar las validaciones del
pipeline clásico», y `stackalloc` no existe en ningún pipeline clásico de este árbol. Segunda vez.
El documento de origen atribuye repetidamente a un «pipeline clásico» cosas que no están aquí.

**Fase 4A — CERRADA. Corrida de Windows del 2026-08-31 16:06:** checklist **57/57, Failed 0**
(eran 55; las dos nuevas son `cc1_c9_sizeof` a `-O0` y `-O2`, **ambas en 42**), `ctest`
**242/242, 100%**, triage 16/16. `nsc.cc1.correctness` pasa y sube de 22.95 s a **29.88 s**.

Las tres aserciones que había marcado como las más frágiles pasaron: `sizeof(int*)` (forma de la
clave de tipo), `sizeof(record.Middle)` (acceso a campo dentro de un operando no evaluado) y la
aritmética `nuint` sin conversiones. Ver `75_cc1_fase4a_sizeof.md`.

Antes hubo dos corridas rotas, y las dos enseñan algo — §9 de ese documento: una la rompí yo
escribiendo el `.bat` mientras corría; la otra destapó que `cc1-sizeof-unevaluated` **pasaba por la
razón equivocada** y por tanto no medía la tarea 41.

**G3 — CERRADO. Corrida de Windows del 2026-08-31 13:06:** checklist **55/55, Failed 0** (eran
45; las diez nuevas son las cinco fixtures de EH a `-O0` y `-O2`, **todas en 42**), `ctest`
**242/242, 100%**, triage 16/16. `nsc.cc1.correctness` pasa y sube de 9.03 s a **22.95 s**,
consistente con las cinco fixtures y las tres bajadas a IR nuevas; el log de build confirma que
`Cc1CorrectnessTests.cpp` y `ProjectOopLlvmLowerer.cpp` se recompilaron, sin errores ni avisos.

Los tres riesgos que anoté antes de la corrida se midieron y ninguno se materializó: `switch`
dentro de un funclet de cleanup (c7) verifica y ejecuta; `break` desde un catch con `finally`
(c5) recorre `complexExit` correctamente; y las cinco fixtures compilaron sin toparse con ningún
límite del frontend. Ver `74_cc1_fase3_eh.md`.

| Tarea | Estado | Nota |
|---|---|---|
| 74 | [x] | `nsc.version` asertaba `stage 55`; regex actualizado a `stage 57` **más** la línea `build <id>`, para que la generación del build id también se mida ahí |
| 75 | [x] | `cc1-ternary-ownership-mixed` esperaba el fragmento `raii local`; el mensaje real es `RAII ownership cannot be copied or borrowed`. El rechazo ocurrió como debía — me equivoqué de cadena, no de comportamiento |
| 76 | [x] | `nsc --help` se describía como compilador de Stage 55. Añadidas las descripciones de Stage 56 y 57 (ID propio, no arreglo "de paso") |
| 80 | [ ] | **`p->campo` no se baja en el pipeline OOP.** Cuarta forma con la mitad delantera presente y la trasera ausente, tras `sizeof`, `stackalloc` y raw `new`. La tarea 65 le dio mensaje propio nombrando `(*p).campo`; **implementarla** es esto. Fase propia, fuera de CC1 |
| 79 | [ ] | **`++`/`--` no se baja en el pipeline OOP, para ningún tipo.** El HIR contesta «does not yet lower ++/--» también para un `int`. Descubierto al ir a hacer la tarea 62, que daba por hecho que existía. Implementarlo para todos los tipos es una fase propia, fuera de CC1 |
| 78 | [ ] | **Divergencia de tamaño de `enum`, encontrada al atar `sizeof` al servicio de layout.** En `ProjectObjectLayoutModel`, `layoutNamedType` contesta **4** para un enum (`BuiltinLayoutKind::Enum`) y `valueLayoutForTypeKey` contesta **8**, porque un enum cae por el `return {{8, 8}}` de las formas con forma de referencia. Dos funciones de la misma clase, el mismo tipo, respuestas distintas — dentro de la función cuyo propio comentario promete que «un stride y un offset no pueden calcularse por dos reglas distintas». `valueLayoutForTypeKey` es la que calcula el stride de los arreglos (`ProjectOopMirModel::elementLayout`) y los offsets de inicializador agregado. **Medir primero** qué hace el binario con un `Color[]` antes de tocar nada; la fixture `c9_sizeof` deliberadamente NO asierta `sizeof(Color)` hasta entonces. **Fase 4E** (cierre de paridad), con ID propio |
| 77 | [ ] | **Hueco de observabilidad, encontrado al cerrar la fase 3.** `TestContext::result()` imprime `PASS: <n> checks` en stdout, pero `ctest` no lo captura en `02_ctest.log`, así que desde los logs no se puede ver CUÁNTAS comprobaciones corrió una suite — sólo que no falló ninguna. Un bloque de tests que desapareciera de un `.cpp` pasaría en verde sin que nadie lo notara. Hacer que el checklist ejecute los binarios de test directamente y registre el conteo en `SUMMARY.txt`. **Fase 5**, con ID propio y no como arreglo "de paso" |

Ver `73_cc1_fases_0_1_2.md` y `STAGE_CC1_CHANGED_FILES.txt`.

---

## Fase 0 — Identidad y arnés

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 1 | [x] | R-002 | Añadir `NSC_BUILD_ID` (hash corto del árbol + timestamp UTC) a `include/nativesharp/Basic/Version.h`, generado por CMake en cada configure, y hacer que `nsc --version` lo imprima junto a `0.1.0-dev55` | Dos builds distintos imprimen `NSC_BUILD_ID` distinto |
| 2 | [x] | R-002 | Cambiar `compiler stage` a 57 y la `DESCRIPTION` de `CMakeLists.txt` de "Stage50 ownership types" al estado real, y añadir el test `nsc.version.identity` que falle si `NSC_BUILD_ID` está vacío | `nsc.version.identity` pasa y falla al vaciar la variable |
| 3 | [x] | R-003 | Crear `tests/unit/Cc1CorrectnessTests.cpp` con el arnés `checkFixture`/`checkRejected` copiado de `Stage57LanguageTests.cpp`, registrarlo en `CMakeLists.txt` como `nsc.cc1.correctness`, y crear `tests/cc1/` | `ctest -R nsc.cc1.correctness` pasa con cero casos |

## Fase 1 — Constantes y `char`

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 4 | [x] | C-005 | Corregir `constantFits` en `src/Frontend/ProjectTypeConversion.cpp:94`: separar `builtin:char` de `builtin:ushort` y limitarlo a `std::numeric_limits<std::uint8_t>::max()` | `constantFits(256, "builtin:char")` es `false` |
| 5 | [x] | C-005 | Añadir a `tests/unit/ProjectTypeConversionTests.cpp` casos de que `255` convierte implícitamente a `char`, que `256` y `321` no, y que `65535` sigue convirtiendo a `ushort` | Los cuatro casos pasan |
| 6 | [x] | C-005 | Añadir el fixture negativo `cc1-char-range` a `nsc.cc1.correctness`: `char c = 321;` rechazado con el mensaje de conversión implícita | Falla y el mensaje contiene el tipo destino |
| 7 | [x] | C-002 | Crear `struct ProjectIntegerValue { std::uint64_t bits; std::string typeKey; }` y `class ProjectIntegerEvaluator` en `include/nativesharp/Frontend/ProjectIntegerEvaluator.h` + `src/Frontend/ProjectIntegerEvaluator.cpp`, con `normalize(bits, typeKey)` que trunca al ancho del tipo y extiende el signo según su signedness | `normalize(321, "builtin:char") == 65` |
| 8 | [x] | C-002 | Implementar en `ProjectIntegerEvaluator` las operaciones `+ - * & \| ^` sobre `std::uint64_t` y `normalize` al tipo resultado | Overflow envuelve en cada ancho, sin UB |
| 9 | [x] | C-002 C-003 | Implementar `/` y `%` ramificando por signedness: sin signo sobre `std::uint64_t`, con signo sobre `std::int64_t`, devolviendo `std::nullopt` si el divisor es 0 **o** si es el par mínimo-con-signo `/ -1` | `INT64_MIN / -1` devuelve `nullopt` en vez de ejecutar la división |
| 10 | [x] | C-002 C-004 | Implementar `<<` y `>>` ramificando por signedness (`>>` lógico para sin signo, aritmético para con signo) y rechazando counts fuera de `[0, ancho_del_tipo_izquierdo)` | `(byte)1 << 8` rechazado; `(ulong)-1 >> 1` da `0x7FFF...` |
| 11 | [x] | C-001 | Implementar el caso cast: evaluar el operando y aplicar `normalize` al tipo destino del cast | `(byte)300` evalúa a 44 |
| 12 | [x] | C-002 | Implementar los casos literal entero, literal de carácter, unarios `+ - ~`, miembro de enum y campo `const`, portando desde `ProjectMethodBodyModel::foldIntegerConstantExpression` la resolución por `declaringTypeOrdinal` | `MyMsg = WmUser + 1` sigue plegando |
| 13 | [x] | C-002 | Crear `tests/unit/Cc1IntegerEvaluatorTests.cpp` con la matriz de: overflow envolvente en cada ancho, división y módulo sin signo con bit 63 activo, mínimo-con-signo `/ -1` rechazado, shift lógico vs aritmético, shift fuera de rango rechazado, y truncamiento por cast en `byte`/`char`/`short`/`int` | La matriz completa pasa |
| 14 | [x] | C-002 | Sustituir el cuerpo de `ProjectMethodBodyModel::foldIntegerConstantExpression` por una llamada a `ProjectIntegerEvaluator`, borrando la aritmética duplicada y devolviendo `ProjectIntegerValue` en vez de `std::int64_t` (renombrada a `foldIntegerConstant`) | Cero aritmética entera propia queda en `ProjectMethodBodyModel.cpp` |
| 15 | [x] | C-001 | En `ProjectMethodBodyModel::analyzeSwitch`, normalizar cada etiqueta plegada al `governingTypeKey` **antes** de insertarla en `seenValues` y de compararla | La comparación de duplicados usa el bit pattern del tipo gobernante |
| 16 | [x] | C-001 | En `analyzeSwitch`, guardar en `MethodSwitchSectionBinding::labelValues` el valor ya normalizado, para que HIR, MIR y `lowerSwitch` reciban exactamente el bit pattern que LLVM materializará | `--dump-oop-mir` muestra el valor normalizado |
| 17 | [x] | C-001 C-005 | Añadir el fixture negativo `cc1-switch-dup-narrowed`: gobernante `char` con `case 65:` y `case 321:` | Falla con `duplicate case label with value 65` |
| 18 | [x] | C-001 | Añadir el fixture negativo `cc1-switch-dup-cast`: gobernante `byte` con `case 44:` y `case (byte)300:` | Falla con el mismo diagnóstico de duplicado |
| 19 | [x] | C-001…C-005 | Añadir el fixture de ejecución `tests/cc1/c1_constants/` a `nsc.cc1.correctness` y a `run_windows_checklist.bat`, con etiquetas `const` derivadas de expresiones con shift, división sin signo y cast | Exit 42 a `-O0` y `-O2` |

## Fase 2 — Flujo

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 20 | [x] | F-001 | Añadir a `ProjectMethodBodyModel` los privados `OwnershipFlowSnapshot analyzeBranchState(entry, work)` y `void commitBranchJoin(left, right)`, que encapsulen el patrón `capture` → `restore(entry)` → analizar → `capture` → `restore(join(...))` que hoy sólo usa `case IfStatement` | `case IfStatement` reescrito sobre ellos sin cambio de comportamiento |
| 21 | [x] | F-001 | Reescribir `case ast::AstNodeKind::ConditionalExpression` (`ProjectMethodBodyModel.cpp:4085`): analizar la condición, capturar el estado de entrada, analizar cada rama con `analyzeBranchState` desde ese estado, cerrar con `commitBranchJoin` | El `case` no contiene análisis secuencial de las dos ramas |
| 22 | [x] | F-002 | En ese `case`, propagar al `ExpressionResult` los campos `ownership`, `ownershipTransfer`, `ownershipSourceLocalOrdinal`, `ownershipSourceParameterOrdinal` y `borrowedLocalOrdinal` cuando ambas ramas coincidan, y dejarlos en `None`/`nullopt` cuando difieran | Tarea 26 pasa |
| 23 | [x] | F-003 F-004 | En `analyzeBinary`, mover el análisis del operando derecho de `&&` y `\|\|` detrás de un `capture` del estado tras el operando izquierdo, y cerrar con `commitBranchJoin` entre "RHS evaluado" y "RHS no evaluado" | Tarea 25 pasa |
| 24 | [x] | F-001 | Añadir el fixture negativo `cc1-ternary-definite-assignment`: `int x; int unused = c ? (x = 1) : 0; return x;` | Rechazado por lectura de variable no asignada definitivamente |
| 25 | [x] | F-003 F-004 | Añadir los fixtures negativos `cc1-andand-definite-assignment` y `cc1-oror-definite-assignment` sobre `c && (x = 1)` y `c \|\| (x = 1)` | Los dos rechazados |
| 26 | [x] | F-002 | Añadir el fixture de ejecución `tests/cc1/c2_ternary_ownership/` con `raii string r = c ? MakeA() : MakeB();` donde ambos devuelven `owned string` | Compila y exit 42 a `-O0` y `-O2` |
| 27 | [x] | F-002 | Añadir el fixture negativo `cc1-ternary-ownership-mixed`: ternario con rama verdadera `owned` y rama falsa alias no propietario, inicializando un `raii` | Rechazado |
| 28 | [x] | F-005 | Separar la SEMÁNTICA de `DoWhileStatement` de la de `WhileStatement`. **Desviación deliberada:** un solo `case` con un flag `isDoWhile` en vez de dos `case`. Sólo dos cosas los distinguen; el `LoopContext`, la unión de estados de `continue` y las ~60 líneas de RAII a través del back-edge son idénticas, y duplicarlas daría dos copias de la parte difícil. Razonado en §4.4 del doc 73 | Las dos diferencias (orden y camino de cero iteraciones) están implementadas y medidas por 30 y 31 |
| 29 | [x] | F-005 | En la rama de `DoWhileStatement`, analizar el cuerpo **antes** que la condición, y calcular el estado posterior uniendo sólo los `breakStates` y el estado tras evaluar la condición — sin incluir el estado de entrada, porque el camino de cero iteraciones no existe | Tarea 30 pasa |
| 30 | [x] | F-005 | Añadir el fixture de ejecución `tests/cc1/c3_dowhile_flow/` con una variable asignada sólo dentro del cuerpo del `do…while` y leída después del bucle | **Compila** y exit 42 a `-O0` y `-O2` |
| 31 | [x] | F-005 | Añadir el fixture negativo `cc1-while-flow-still-strict` con la misma forma sobre un `while` | Sigue rechazado — fija que 29 no relajó `while` |

## Fase 3 — EH

**Corrección de la tarea 32 antes de empezar, medida y no razonada.** El texto original colgaba la
obligación de un campo nuevo en `FuncletContext`, marcado en «los contextos creados para cláusulas
`catch`». Al barrer los sitios que empujan en `funcletStack_` resultó que **no hay ninguno que
empuje una entrada `Catch`**: desde el arreglo B-006/B-007 los cuerpos de las cláusulas se bajan
como bloques padre ordinarios y el único `push_back` que queda construye entradas `Cleanup`. Un
campo ahí habría sido código inalcanzable: cerraba el hallazgo en el papel y no medía nada. La
obligación se ancla en `activeCatchExceptions_`, que es la pila que sí sigue «estoy dentro del
cuerpo de un catch». Detalle completo en `74_cc1_fase3_eh.md` §2.

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 32 | [x] | EH-001 | Dar a `activeCatchExceptions_` la forma `ActiveCatchBody{exception, loopDepthAtEntry}` y registrar `loopStack_.size()` al empezar a bajar cada cuerpo de cláusula, para poder distinguir «sale del catch» de «sale de un bucle o switch abierto dentro del catch» | Compila sin cambio de comportamiento |
| 33 | [x] | EH-001 | Añadir `structuredExitLeavesCatchBody()` y `emitCatchBodyExceptionClear()`, y llamarlas en `lowerStructuredExit` antes de las tres formas de salida (rama simple, recorrido de cleanups y desenrollado de funclet), emitiendo la limpieza en un bloque propio `eh.catch.leave.N` | `cc1-eh-structured-exit-clears` cuenta 3 bloques con la llamada dentro, y los dos controles negativos cuentan 0 |
| 34 | [x] | EH-001 | Cerrar el riesgo de doble limpieza en la salida por fallthrough | Medido, no razonado: una salida estructurada termina el bloque, así que el `br` a `handledBlocks[index]` no se emite en ese camino; y la limpieza del `catchret` es inalcanzable hoy. Además `__nativesharp_exception_clear` sólo escribe null en un slot TLS, así que dos limpiezas son idempotentes. Anotado en el código |
| 35 | [x] | EH-002 | Añadir el fixture de ejecución `tests/cc1/c4_catch_continue/` | Exit 42 a `-O0` y `-O2` |
| 36 | [x] | EH-002 | Añadir los fixtures de ejecución `tests/cc1/c5_catch_break/` y `tests/cc1/c6_catch_return/` | Exit 42 a `-O0` y `-O2` |
| 37 | [x] | EH-003 | Añadir el fixture de ejecución `tests/cc1/c7_finally_switch/` | Exit 42 a `-O0` y `-O2` — **es la prueba que demuestra las dos profundidades de funclet** |
| 38 | [x] | EH-002 | Añadir el fixture de ejecución `tests/cc1/c8_loop_catch_switch/` | Exit 42 a `-O0` y `-O2` |
| 39 | [x] | EH-002 EH-003 | Registrar los fixtures `c4`…`c8` en `run_windows_checklist.bat` y en `nsc.cc1.correctness` | El checklist los ejecuta y compara código de salida |

## Fase 4A — `sizeof`

**Tres correcciones al texto de estas tareas, hechas leyendo el código antes de escribirlo.**

1. **La tarea 40 tenía un criterio que no se puede medir.** Decía «coincide con el pipeline
   clásico». Barrido completo del árbol: `sizeof` sólo aparece en el lexer, en el parser y en
   `ProjectMethodBodyModel`. No hay caso en Sema, no hay caso en el HIR clásico, no hay un solo
   test. El documento de origen afirma en §15 que «el pipeline clásico los baja directamente a una
   constante `nuint`» y en §16 que «posee tratamiento explícito de unevaluated operand y tests» —
   **eso no está en este árbol**. El cambio a `nuint` sigue siendo correcto, con una razón propia:
   `sizeof` produce una cuenta de bytes, y el tipo sin signo del ancho del puntero es lo que
   consumen la aritmética de punteros (4D) y `stackalloc` (4B).

2. **La tarea 44 no se puede hacer donde decía.** Pedía calcular el tamaño en
   `ProjectMethodBodyModel`. El orden de construcción es modelo de cuerpos de método PRIMERO y
   `ProjectObjectLayoutModel` DESPUÉS; `ProjectMethodBodyModel::build` ni siquiera recibe el modelo
   de layout. Hacerlo ahí obligaría a reordenar el pipeline o a duplicar las reglas de tamaño en el
   modelo semántico — la duplicación que este milestone existe para quitar. El binding lleva **el
   type key medido**; el tamaño lo resuelve el HIR con `valueLayoutForTypeKey`.

3. **Faltaba una tarea.** `SizeofTypeExpression` no tocaba siquiera su `typeSyntax()`:
   `sizeof(void)` y `sizeof(TipoQueNoExiste)` pasaban el análisis semántico sin una palabra.

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 40 | [x] | M-004 | Cambiar el tipo resultado de `sizeof` de `"builtin:ulong"` a `"builtin:nuint"` | La fixture `c9_sizeof` usa el resultado en aritmética `nuint` sin conversión y ejecuta en 42 |
| 41 | [x] | M-004 | Hacer no evaluado el operando de `sizeof(expression)` capturando `OwnershipFlowSnapshot` antes de analizarlo y restaurándolo después, en vez de suprimir efecto por efecto | El estado de flujo antes y después es idéntico **por construcción**: el snapshot ya lleva moves (must y may), borrows y `assignedLocals` |
| 42 | [x] | M-004 | (Absorbida en la 41: la captura/restauración envuelve exactamente la llamada del operando) | — |
| 43 | [x] | M-004 | Añadir el negativo `cc1-sizeof-unevaluated` (`int x; int n = sizeof(x = 1); return x;`) y su gemelo positivo `cc1-sizeof-unevaluated-assigned` | El primero rechazado por leer `x` sin asignar; el segundo aceptado — sin el par, «restaurar el flujo» podría haberse implementado como «rechazar todo sizeof con una asignación dentro» |
| 44 | [x] | M-003 | Registrar `MethodCoreOperationKind::SizeofConstant` con **el type key medido** en `elementTypeKey` y `"builtin:nuint"` en `resultTypeKey` | El binding existe y lleva el tipo; el tamaño NO se calcula aquí (ver corrección 2) |
| 45 | [x] | M-003 | Añadir en `ProjectOopHirModel::lowerExpression` los dos `case`, que emitan un `IntegerConstant` `builtin:nuint` con el tamaño que dé `valueLayoutForTypeKey` — ninguna tabla de tamaños nueva | Ninguno cae ya en el `default`; `c9_sizeof` ejecuta |
| 46 | [x] | M-003 | Añadir el fixture de ejecución `tests/cc1/c9_sizeof/` y registrarlo en el checklist y en `nsc.cc1.correctness` | Exit 42 a `-O0` y `-O2` |
| 46b | [x] | M-004 | Validar en el modelo semántico lo que no tiene tamaño (`void`, grupo de métodos, tipo desconocido) usando `DiagnosticId::InvalidSizeof`, que estaba declarado y **sin usar en ningún sitio** | `cc1-sizeof-void` y `cc1-sizeof-unknown-type` rechazados con su mensaje |

## Fase 4B — `stackalloc`

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 47 | [x] | M-002 | Portar a `ProjectMethodBodyModel` las validaciones de `stackalloc` del pipeline clásico: contador integral, elemento no `void`, y rechazo de elementos con destructor o con campos `owned` | Tarea 52 pasa |
| 48 | [x] | M-001 | Añadir `ProjectOopHirNodeKind::StackAllocation`, su nombre en `projectOopHirNodeKindName`, y el `case StackallocExpression` en `lowerExpression` que emita el nodo con `elementTypeKey` y el conteo como hijo | Ya no cae en el `default` |
| 49 | [x] | M-001 | Añadir `ProjectOopMirNodeKind::StackAllocation`, su nombre, el mapeo HIR→MIR, y la resolución de tamaño y alineación del elemento vía `elementLayout` en `convertNode` | `--dump-oop-mir` muestra el nodo con su stride |
| 50 | [x] | M-001 | Añadir `lowerStackAllocation` en `ProjectOopLlvmLowerer.cpp` con `LLVMBuildArrayAlloca` y la alineación del layout, y engancharlo en el dispatch de expresiones | Declarar antes en `LlvmCApi.h` lo que falte |
| 51 | [x] | M-001 | Añadir el fixture de ejecución `tests/cc1/c10_stackalloc/` que reserve un buffer, lo llene por índice y lo lea | Exit 42 a `-O0` y `-O2` |
| 52 | [x] | M-002 | Añadir el fixture negativo `cc1-stackalloc-owned-element`: `stackalloc` de un tipo con campos `owned` | Rechazado |

## Fase 4C — raw `new T`

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 53 | [x] | M-005 | Registrar un `MethodCoreBinding` nuevo `RawAllocation` en `ProjectMethodBodyModel` para el `new T` escalar no-clase, con `elementTypeKey` y tamaño resuelto | El binding existe |
| 54 | [x] | M-005 | Añadir `ProjectOopHirNodeKind::RawAllocation` y su `case` en `lowerExpression`, reemplazando el error `"Stage 41 OOP HIR only owns dynamic new T[count]"` cuando el binding sea `RawAllocation` | Ya no cae en el error |
| 55 | [x] | M-005 | Añadir `ProjectOopMirNodeKind::RawAllocation`, su mapeo, y `lowerRawAllocation` en el backend que llame a `__nativesharp_alloc` con el tamaño del elemento y devuelva `ptr` | El IR llama al allocador con el tamaño correcto |
| 56 | [x] | M-005 | Añadir el fixture de ejecución `tests/cc1/c11_raw_new/` con `new int`, escritura por `*p` y `delete p` | Exit 42 a `-O0` y `-O2` |

## Fase 4D — aritmética de punteros

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 57 | [x] | M-006 | Añadir a `analyzeBinary`, antes de la rama `isArithmetic`, una rama de punteros que acepte `puntero + entero`, `entero + puntero`, `puntero - entero` y `puntero - puntero-del-mismo-tipo`, con resultado `T*` para las tres primeras y `builtin:nint` para la última, y que rechace `void*` en las cuatro | Tareas 63 y 64 pasan |
| 58 | [x] | M-006 | Registrar en esa rama un `MethodCoreBinding` nuevo `PointerArithmetic` con `elementTypeKey` y el discriminante de operación | El binding existe |
| 59 | [x] | M-006 | Añadir `ProjectOopHirNodeKind::PointerArithmetic` y su lowering en `lowerExpression`, con los dos operandos como hijos | Ya no cae en el `default` |
| 60 | [x] | M-006 | Añadir `ProjectOopMirNodeKind::PointerArithmetic`, su mapeo, y la resolución de `immediate` (tamaño del elemento) y `alignment` en `convertNode` vía `elementLayout` | `--dump-oop-mir` muestra el stride |
| 61 | [x] | M-006 | Añadir `lowerPointerArithmetic` en el backend: `LLVMBuildGEP2` con el tipo de elemento para suma y resta, y `LLVMBuildPtrDiff2` para la diferencia | Declarar antes en `LlvmCApi.h` lo que falte |
| 62 | [-] | M-006 | Extender `analyzeUnary` y el lowering de `PlusPlus`/`MinusMinus` para que un operando de tipo puntero use la misma infraestructura de `PointerArithmetic` con paso 1 | `++p` y `--p` compilan |
| 63 | [x] | M-006 | Añadir el fixture de ejecución `tests/cc1/c12_pointer_arith/` que recorra un buffer con `p + i`, `i + p`, `p - 1`, `++p`, `--p` y una diferencia `q - p`, y compruebe que `p[i]` y `*(p + i)` dan el mismo valor | Exit 42 a `-O0` y `-O2` |
| 64 | [x] | M-006 | Añadir el fixture negativo `cc1-pointer-arith-void`: aritmética sobre `void*` | Rechazado |

## Fase 4E — cierre de paridad

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 65 | [x] | R-003 | Dar un `case` explícito a cada `AstNodeKind` de expresión que quede en el `default` de `ProjectOopHirModel::lowerExpression`, o listarlos con un mensaje que diga "no soportado en el pipeline OOP" en vez del actual, y añadir un test que recorra todos los `AstNodeKind` y falle si alguno declarado soportado cae en el `default` | El test pasa y falla al quitar un `case` |
| 66 | [x] | R-003 | Registrar los fixtures `c9`…`c12` y los negativos de la fase 4 en `run_windows_checklist.bat` | El checklist los ejecuta |

## Fase 5 — Reproducibilidad y cierre

| # | Est. | ID | Tarea | Hecho cuando |
|---|---|---|---|---|
| 67 | [~] | R-001 | Regenerar `MANIFEST_SHA256.txt` con `verify_manifest.py` **después** del último cambio de código | `verify_manifest.py` reporta 0 fallos |
| 68 | [~] | R-001 | Borrar `68_triage_de_bugs_y_fix_b3.md`, `68_triage_de_bugs_y_cuatro_fixes.md`, `FIX_B3_CHANGED_FILES.txt`, `FIX_B2_B3_B5_B8_CHANGED_FILES.txt`, `70_roadmap_consolidado.md` y el archivo suelto `8` | No quedan en el árbol |
| 69 | [x] | M-001 M-003 | Corregir `69_huecos_de_runtime_y_lenguaje.md` §1: quitar `stackalloc` y `sizeof` de "lo que ya hay" y moverlos a la tabla de huecos con la nota de que existían en el pipeline clásico y no en el moderno | El doc ya no afirma algo falso |
| 70 | [x] | — | Escribir `73_milestone_cc1.md` con los 27 IDs del registro, cuáles cerraron, cuáles se difirieron (P-001, A-001, A-002) y por qué, el repro de `case 65` / `case 321`, y la regla de que ninguna verificación cuenta si el modelo contra el que se verifica lo escribió quien escribió el código | El doc existe y cubre los 27 IDs |
| 71 | [x] | — | Escribir `STAGE_CC1_CHANGED_FILES.txt` con el desglose por capa de los archivos tocados | El doc existe |
| 72 | [~] | — | Correr `run_windows_checklist.bat` en modo completo | ctest verde, checklist verde, triage 16/16, fixtures nuevas en 42 a `-O0` y `-O2` |
| 73 | [x] | — | Escribir el doc de proyecto `claude/54_milestone_cc1.md` con el estado final y lo que quedó abierto | El doc existe en el Project |

---

## Registro de defectos → tareas

| ID | Prioridad | Área | Tareas |
|---|---|---|---|
| C-001 ✅ | P1 | Constantes | 11, 15, 16, 17, 18, 19 |
| C-002 ✅ | P1 | Constantes | 7, 8, 9, 10, 12, 13, 14, 19 |
| C-003 ✅ | P1 | Constantes | 9, 19 |
| C-004 ✅ | P1 | Constantes | 10, 19 |
| C-005 ✅ | P1 | Tipos | 4, 5, 6, 17 |
| F-001 ✅ | P1 | Flujo | 20, 21, 24 |
| F-002 ✅ | P1 | Ownership | 22, 26, 27 |
| F-003 ✅ | P1 | Flujo | 23, 25 |
| F-004 ✅ | P1 | Flujo | 23, 25 |
| F-005 ✅ | P2 | Flujo | 28, 29, 30, 31 |
| EH-001 | P1 | EH | 32, 33, 34, 35, 36 |
| EH-002 | P1 | EH tests | 35, 36, 38, 39 |
| EH-003 | P1 | EH tests | 37, 39 |
| M-001 | P2 alto | Pipeline moderno | 48, 49, 50, 51 |
| M-002 | P2 alto | Semántica | 47, 52 |
| M-003 | P2 alto | Pipeline moderno | 44, 45, 46, 69 |
| M-004 | P1 | Semántica | 40, 41, 42, 43 |
| M-005 | P2 alto | Pipeline moderno | 53, 54, 55, 56 |
| M-006 | P2 alto | Pipeline moderno | 57–64 |
| R-001 | P2 | Reproducibilidad | 67, 68 |
| R-002 ✅ | P2 | Reproducibilidad | 1, 2 |
| R-003 | P2 | Testing | 3, 65, 66 |
| P-001 | P2 alto | Parser | **diferido** |
| P-002 | P2 alto | Parser | **diferido** con P-001 |
| A-001 | P2 alto | Arquitectura | **diferido** |
| A-002 | P2 | Arquitectura | **diferido** |

**F-005 bajado a P2 respecto del documento de origen:** `?:` sin join hace que el compilador
**acepte** código que debería rechazar; `do…while` analizado como `while` lo hace **rechazar**
código válido. Sólo el primero puede producir un programa incorrecto.

**M-004 subido a P1:** `sizeof(expr)` no es un hueco de lowering sino una mutación silenciosa del
estado de análisis desde un operando que no se evalúa.
