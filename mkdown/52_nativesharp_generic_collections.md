# 52 — NativeSharp Generic Collections

## 1. Objetivo de Stage 52

Stage 52 entrega `NativeSharp.Core.Collections.Generic`: una superficie prácticamente completa del `System.Collections.Generic` de C# real — `List<T>`, `Dictionary<TKey,TValue>`, `HashSet<T>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`/`LinkedListNode<T>`, `SortedList<TKey,TValue>`, `SortedDictionary<TKey,TValue>`, `SortedSet<T>`, más `KeyValuePair<TKey,TValue>` y los delegates de soporte (`HashFunc<T>`, `Comparison<T>`, `Predicate<T>`, `Action<T>`, `KeyValueAction<TKey,TValue>`) — instrucción explícita de Alberto: "no quiero una implementación parcial, quiero prácticamente todos los generics de C# aquí. A menos que alguno no pueda portearse porque depende del CLR o algo así, no hay excusas."

Ningún gap encontrado en este stage dependía del CLR. Los que aparecieron eran, en todos los casos, gaps reales de wiring entre el sistema de ownership (Stage 50) y el sistema de generics/monomorfización (Stage 44) — nunca antes ejercitados juntos con un `T` de tipo valor cerrando sobre un parámetro `owned`/`move()` en la profundidad que una librería de colecciones real exige — y se corrigieron todos, a nivel de compilador, con causa raíz real. La única excepción es una limitación general y preexistente del ABI de structs de Stage 40 (sección 5), que no tiene nada que ver con generics ni con ownership — se documenta y se rodea con el patrón de diseño más cercano, no se ignora.

## 2. Punto de partida verificado

Se continuó directamente sobre el árbol Stage 51 cerrado (`nsc 0.1.0-dev51`, 203/204 con la única falla conocida ya documentada — `nsc.stage50_10.ownership-examples-documentation`). `lib/NativeSharp.Core.Collections.Generic/` no existía todavía; se creó desde cero junto con `lib/NativeSharp.Core/Exception.ns` (necesario porque `throw`/`catch` ya funcionaban desde Stage 45/49 pero nunca tuvieron un tipo raíz `NativeSharp.Core.Exception` real y empaquetado — ver sección 4.3).

## 3. Disciplina de verificación

Cada patrón de ownership no visto antes se probó primero con un proyecto desechable contra `nsc --check-project-method-bodies` / `--verify-project-oop-llvm` / `--emit-project-oop-obj` real, nunca se asumió. La colección completa de descubrimientos de esta ronda:

```text
owned T value (parametro) pasado como argumento PRESTADO (sin move()) a otro
metodo con parametro T plano (no owned)                         OK -- lectura
                                                                  prestada, sin
                                                                  transferencia
int[] grown = new int[n]; (arreglo NO generico, plano)           OK sin const/raii
                                                                  -- la ambiguedad
                                                                  de parser solo
                                                                  aplica a T bare
private Inner inner; (campo de tipo clase, SIN owned)
  inner = new Inner(); ... delete inner;                         OK -- ownership
                                                                  manual explicita,
                                                                  sin raii/owned
raii string s = "literal";                                       ERROR -- raii solo
                                                                  acepta new/null/
                                                                  move/llamada owned
raii string s = String.Copy("literal"); box.Set(move(s));        OK
```

Estos cuatro no exigieron ningún cambio de compilador — son reglas ya existentes desde Stage 49/50/44, simplemente no documentadas hasta ahora porque nunca se habían necesitado en esta combinación exacta. Los que SÍ exigieron un fix real de compilador están en la sección 4.

## 4. Fixes reales de compilador (8, 9, 10a, 10b)

Se conservan también, sin reescribir, los fixes 1–7 ya cerrados antes de este documento (ver el historial de esta sesión / `STAGE52_CHANGED_FILES.txt`): no-op de `delete` sobre T valor, pérdida del calificador de ownership al monomorfizar, validación owned/borrowed demasiado estricta para T valor, `move()` desde un elemento de arreglo, y el crash del lowerer LLVM con `return` dentro de `try/catch`, más el fix cross-project de resolución de generics (namespace de origen vs. namespace de definición del template).

### 4.1 Fix 8 — `move()` como TARGET de una asignación a elemento de arreglo

`grown[i] = move(items[i]);` — el paso exacto que necesita `EnsureCapacity` en todo contenedor con arreglo propio — se rechazaba con "move(...) is only valid for a direct RAII local or owned parameter..." porque `analyzeAssignment` sólo permitía un RHS `move()`-valuado cuando el TARGET de la asignación era un identificador RAII local nombrado; nunca consideró un elemento de arreglo como target válido (aunque ya era, desde antes, una reubicación segura dentro del mismo arreglo owned — ver el comentario de clase de `List.ns`). Corregido ampliando `allowMove` para cubrir también un target de elemento de arreglo nativo.

### 4.2 Fix 9 — `default(T)` no existía en absoluto

`List<T>.Find` necesita devolver el valor cero-equivalente de T cuando nada matchea, exactamente como el `default(T)` real de C#. No existía en ningún punto del pipeline — ni lexer, ni parser, ni AST, ni HIR/MIR/LLVM. Se implementó de punta a punta como feature real de lenguaje, no como parche local a `List.ns`:

- Lexer: nuevo `TokenKind::KwDefault` + entrada de keyword.
- Parser: bloque `default(Tipo)` justo después del manejo de `sizeof` (misma forma, sin ambigüedad porque `default` siempre toma un tipo).
- AST: nuevo nodo `DefaultValueExpression`.
- Semántica: nuevo caso en `analyzeExpression`, resuelve el tipo vía `canonicalTypeKey`, rechaza `builtin:void` con diagnóstico explícito.
- HIR/MIR: nuevo nodo `DefaultValue`, propagación directa del `typeKey`.
- LLVM: `LLVMConstNull(llvmType(...))` — universalmente correcto para primitivo, referencia nula, o struct de todos los campos en cero.

### 4.3 Fix 10 — `move()` de un parámetro owned de tipo valor, como argumento de llamada

Descubierto con el repro mínimo real: `LinkedList<int>.AddFirst`'s `created[0].SetValue(move(value))`, donde `value` es un parámetro `owned int`. Dos bugs distintos, en dos capas:

**(a) Resolución de overload.** El loop de viabilidad de argumentos de `CallExpression` tenía una cadena if/else-if: `if (Owned && parameterOwnable) ... else if (Borrowed && parameterOwnable) ... else (catch-all: rechaza cualquier argumento de transferencia de ownership hacia un parámetro "no calificado")`. Un parámetro `owned T`/`borrowed T` de tipo VALOR (`parameterOwnable == false`) caía en AMBAS ramas reales y terminaba en el catch-all, rechazado como "no viable overload" — aunque la misma llamada con un argumento de tipo valor SIN `move()` ya se aceptaba correctamente (fix 3). Corregido revisando `!parameterOwnable` PRIMERO, saltando por completo la validación de ownership para un parámetro no-ownable.

**(b) Lowering LLVM.** Una vez que (a) dejaba pasar la llamada, `MoveValue` (el lowering de `move()` desde un local/parámetro nombrado, existente desde antes de Stage 52) cargaba y null-almacenaba la storage de origen SIEMPRE como puntero — correcto para Stage 49/50, donde `move()` sólo apuntaba a un owner RAII de tipo referencia, pero incorrecto una vez que un T genérico puede cerrar sobre un tipo valor: cargar un `alloca` de tipo `i32` como si fuera puntero producía un mismatch de tipo de parámetro de llamada que el verificador de LLVM rechazaba. Corregido cargando/almacenando con el `typeKey` real del move (igual que ya hacía `MoveArrayElement`), y condicionando el null-store a una nueva bandera `elementIsReferenceLike`, ahora calculada para TODA forma de fuente de `move()` (local, parámetro, elemento de arreglo), no sólo elemento de arreglo.

## 5. Limitación preexistente, NO de generics: structs mayores a 8 bytes

Descubierta escribiendo `Dictionary<TKey,TValue>.TryGetValue`, con un diseño inicial que devolvía un struct `{Found, Value}`. El compilador rechazó `KeyValuePair<int, Payload>`/`{bool, TValue}` con:

```text
error: Stage 40 Windows x64 direct struct value ABI supports only 1, 2, 4 or 8-byte
aggregates; 'type:...' has size 16
```

Se aisló con un repro que descarta generics por completo: un struct puramente no-genérico de dos campos `long` (16 bytes, ningún tipo referencia involucrado) reproduce el mismo error, incluso como variable LOCAL sin cruzar ningún límite de llamada/retorno:

```csharp
public struct TwoLongs { public long A; public long B; }
// TwoLongs p; p.A = 1; p.B = 2; return (int)p.A;   -->  mismo error, sin generics
```

Confirmado que es exclusivamente sobre el struct como VALOR de primera clase (local, argumento, o retorno). Como elemento de un ARREGLO, accedido sólo por campo a través del lvalue indexado, funciona sin problema:

```csharp
TwoLongs[] arr = new TwoLongs[2];
arr[0].A = 1; arr[0].B = 2;      // OK -- direccionamiento en memoria, nunca
long a = arr[0].A;               // materializa el struct entero como valor
```

Es un límite general de representación de structs de Stage 40, ortogonal a generics y a ownership por la misma razón que el gap de constructores con parámetro `owned` (Stage 51→52, ver sección 6) se juzgó fuera de alcance: no es parte de "generics", y arreglarlo de raíz significa implementar retorno/paso indirecto de structs (convención `sret`), un trabajo de ABI completo aparte. Se documentó y se rodeó con el patrón más cercano ya establecido en el resto del paquete:

- `Dictionary<TKey,TValue>.TryGetValue`/`SortedList<TKey,TValue>.TryGetValue` usan el mismo idioma de "arreglo de un elemento como salida" que el paquete ya usa en otros lados como sustituto de `out` (que tampoco existe en NativeSharp — limitación de lenguaje general y preexistente, no de este stage): `bool TryGetValue(TKey key, TValue[] result)`.
- `ToArray()` sigue devolviendo `KeyValuePair<TKey,TValue>[]` (un ARREGLO del struct es válido — es memoria direccionable, nunca un valor de primera clase), pero se escribe y se debe consumir por acceso a campo (`result[i].Key`/`result[i].Value`), nunca copiando un elemento a un local `KeyValuePair<TKey,TValue>` standalone.
- `ForEach` usa un delegate de dos parámetros propio, `KeyValueAction<TKey,TValue>(TKey key, TValue value)`, en vez de `Action<KeyValuePair<TKey,TValue>>` — dos valores planos nunca topan con el límite de tamaño, sin importar cuán grandes sean `TKey`/`TValue`.

Regresión cubierta en `tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp::testOversizedStructWorksAsArrayElementButNotAsAStandaloneValue`: confirma que el patrón de arreglo compila y lowera limpio, y que el patrón de local standalone sigue siendo rechazado con el diagnóstico esperado (no falla silenciosamente).

## 6. Decisiones de alcance deliberadas (no bugs)

- **Constructores nunca reciben `owned T`.** Confirmado por grep exhaustivo contra todo el código NativeSharp existente antes de este stage: ningún constructor, en ningún stage anterior, toma un parámetro `owned`. `ObjectCreationExpression` nunca permite `allowMove=true` en sus argumentos, y `ProjectConstructionModel` no tiene ninguna validación de transferencia de ownership (sólo `classifyImplicit` para conversión de tipos) — permitir `move()` como argumento de constructor sin esa validación sería un agujero sin guardas. Se mantuvo la restricción tal cual: cada clase de este paquete transfiere ownership después de construir, vía un método dedicado (`list.Add(move(x))`, nunca `new List(move(x))`).
- **No hay `ref`/`out` reales.** Limitación de lenguaje general, preexistente, sin relación con generics — no se implementó como parte de este stage. `TryGetValue` usa el idioma de arreglo-de-un-elemento (sección 5) en su lugar.
- **`SortedDictionary<TKey,TValue>`/`SortedSet<T>` usan arreglo ordenado, no árbol balanceado.** Real C# los implementa con un árbol rojo-negro (O(log n) inserción/borrado). Este paquete los construye como wrappers delgados sobre `SortedList<TKey,TValue>` (arreglos paralelos ordenados, búsqueda binaria, inserción/borrado por desplazamiento — O(n)) por la misma razón que `List<T>.Sort()` usa insertion sort en vez de introspective sort: confianza en la corrección por encima de la optimalidad asintótica, dado que este entorno no puede ejecutar el binario compilado para observar comportamiento real. Documentado explícitamente, no oculto — el resto de la superficie (orden garantizado, API completa) es real.
- **`HashSet<T>` es un wrapper delgado sobre `Dictionary<T, bool>`**, y `SortedSet<T>` sobre `SortedList<T, bool>` — el `bool` es un placeholder fijo, nunca expuesto por la API pública de estas dos clases. Evita duplicar la lógica de la tabla hash / arreglo ordenado una segunda vez.

## 7. Ownership: resumen de la disciplina aplicada en todo el paquete

- Storage propio siempre `owned T[]` (o `owned TKey[]`/`owned TValue[]`) — funciona uniformemente para T valor (no-op) y T referencia (transferencia real), gracias a los fixes 1–3 del historial previo.
- `Add`/`Enqueue`/`Push`/etc. toman `owned T value`; el caller transfiere con `move(...)` sólo cuando T es referencia (T valor no necesita ceremonia).
- El indexer siempre es de sólo lectura (`get`) — un setter de indexer en NativeSharp tiene un parámetro `value` implícito que no puede declararse `owned`, así que todo write path pasa por un método explícito (`SetAt`, `AddOrUpdate`, `SetValue`) que sí puede forzar la transferencia real.
- Un valor único poseído (nodo de lista enlazada, backing dictionary de `HashSet<T>`) se guarda como campo `owned X[1]` (arreglo de un elemento) cuando necesita reasignación con `move()` a través de llamadas, o como campo plano no-owned + `delete` manual cuando no hay ese patrón de reasignación (ver `HashSet.items`/`SortedSet.items`, probado directamente contra el compilador antes de usarse).
- `ToArray()`/`Keys()`/`Values()` devuelven arreglos NO owned deliberadamente — snapshots que aliasan el storage real del contenedor, exactamente como el `ToArray()` real de C# alias-ea referencias trackeadas por el GC; aquí el tipo del arreglo devuelto (`T[]`, no `owned T[]`) es lo que hace ese alias seguro sin GC.

## 8. Verificación real

```text
GCC 13.3.0    ctest: 203/204 (misma única falla preexistente que Stage 51, sin tocar)
tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp: 88 checks, todos PASS
```

Cada clase nueva (`Dictionary<TKey,TValue>`, `HashSet<T>`, `SortedList<TKey,TValue>`, `SortedDictionary<TKey,TValue>`, `SortedSet<T>`, más `List<T>`/`Queue<T>`/`Stack<T>`/`LinkedList<T>` heredadas del trabajo previo de esta sesión) se instanció con T valor (`int`) Y T referencia (una clase propia) en un proyecto de prueba real (`Stage52Test`/`examples/project_stage52`) con `<ProjectReference>` real a la librería, verificado hasta:

```text
project syntax check: ok
project method-body semantic check: ok
project OOP HIR check: ok
project OOP MIR check: ok
project OOP LLVM verification: ok
project OOP object emission: ok (machine=0x8664, COFF real)
```

**[ESTADO AL MOMENTO DE ESCRIBIR ESTA SECCIÓN, YA SUPERADO — ver sección 10]** No se pudo
ejecutar el objeto/`.exe` resultante — target Windows PE, sin Wine en este entorno (misma
limitación que Stage 51). El comportamiento en runtime de cada método quedaba verificado hasta
"compila, lowera limpio, y emite un objeto COFF válido", no hasta "corrí el binario real". Esto
cambió por completo en la ronda de cierre siguiente (sección 10) — la verificación puramente
estructural de este punto NO detectó un bug real de doble-free que sí apareció al ejecutar de
verdad.

## 9. Cierre formal (superado por la sección 10 — Stage 52 cierra formalmente recién ahí)

`nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)`. SOURCE-ONLY exacto, FULL congelado, fresh extraction, clean rebuild/test desde esa extracción, SHA-256, handover 52→53 — ver `STAGE52_CHANGED_FILES.txt` y `HANDOVER_STAGE52_TO_STAGE53.md`.

## 10. Actualización — ejecución real (Wine 9.0, luego Windows real) y foreach (52.x, 52.x.2)

Esta sección documenta todo lo que pasó DESPUÉS del "cierre formal" de la sección 9, en rondas
posteriores, y es la que efectivamente cierra Stage 52. La versión de la sección 9 se mantiene
sin editar como registro histórico honesto de lo que se sabía en ese momento.

### 10.1 Wine 9.0 instalado — primera ejecución real de binarios NativeSharp

Se instaló Wine 9.0 en el sandbox (nunca antes disponible). Un programa NativeSharp trivial corre
correctamente (exit code confirmado), validando el entry point/exit-code del compilador de punta
a punta. Pero la ejecución real de `Stage52Collections.exe` reveló un **bug real invisible a toda
la verificación estructural anterior**: doble-free por asignación plana en vez de `move()` al
guardar un parámetro `owned` en el storage de un contenedor.

### 10.2 El bug de doble-free `owned`/`move()` — causa raíz y corrección (14 sitios)

`List<T>.Add(owned T value)` guardaba el parámetro con `items[count] = value;` (asignación
PLANA) en vez de `items[count] = move(value);`. Eso deja el storage del parámetro sin anular, así
que el cleanup automático de owned-parameter al retornar de `Add()` destruye el mismo objeto que
el arreglo acaba de guardar — un doble-free real, cuyo síntoma (page fault) aparece varios pasos
después, lejos del sitio real del bug. Bisección exhaustiva de 12 pasos, cada uno con ejecución
real bajo Wine, descartando `GetEnumerator`/generics/nested-calls/loop-vs-const-index uno por
uno. Corregido en los 14 sitios del mismo patrón en toda la librería: `List`, `Queue`, `Stack`,
`Dictionary`, `SortedList`, `PriorityQueue`, `LinkedListNode`. `HashSet`/`SortedSet`/
`SortedDictionary` ya estaban correctos (delegan con `move()` a su colección interna).

Se agregó `requiresOwnedDestruction(typeKey)` en `ProjectOopLlvmLowerer.cpp`, usado ahora por
`emitOwnedParameterDestroy` y `emitRaiiDestroy` en vez de que cada uno repita su propia
condición — auditados los demás puntos de cleanup (RAII local, array element, exceptional/
funclet), ninguno tenía el mismo agujero, pero `emitRaiiDestroy` recibió una guardia defensiva
de todos modos.

Resultado verificado por ejecución real: `Stage52Collections.exe` → **149, sin crash**,
determinista (3/3 corridas bajo Wine 9.0). `ctest` completo: 208/208 en ese momento.

### 10.3 `foreach` — rondas 52.x y 52.x.2 (posteriores)

`foreach` real sobre los diez contenedores (desugaring estructural, sin necesitar una interfaz
`IEnumerable<T>`), agregado en dos rondas:

- **52.x**: desugaring inicial + `GetEnumerator()`/`MoveNext()`/`Current` para `List`/`Array`.
- **52.x.2** (revisión técnica dedicada, después de que Alberto subió un documento de revisión):
  cuatro gaps semánticos P0 corregidos (higiene de temporales con prefijo `$`, fuente
  ownership-aware para `foreach (var x in Factory.CreateOwnedList())`, elemento no-reasignable,
  `const` como error honesto en vez de aceptarse en silencio), dos gaps P1 (`Current` como
  propiedad get-only en vez de método, `GetEnumerator()` extendido a `Queue`/`Stack`/
  `LinkedList`/`HashSet`/`SortedSet`/`PriorityQueue` — deliberadamente NO a los tres
  `Dictionary`-like por el mismo límite de ABI de structs de la sección 5).

Esta ronda destapó un SEGUNDO bug real (no relacionado con el de 10.2): `analyzeForeach` nunca
cerraba la cleanup region de la fuente cuando ésta se vuelve `raii` (el nuevo mecanismo de doble
parseo para sondear ownership de la fuente lo hizo posible por primera vez), causando un error de
compilador en cualquier `return`/`break`/`continue` posterior en el mismo método. Corregido con
teardown simétrico al que `analyzeFor` ya tiene para su propio scope.

`nsc.stage52-5.foreach-semantic-gaps`: 24/24 PASS (ejecución real para los casos
runtime-observables). `nsc.stage52-6.foreach-collections-enumerators`: 16/16 PASS (ejecución
real verificando ORDEN correcto por contenedor). `ctest` completo: **210/210 PASS**.

### 10.4 Bug no relacionado, encontrado escribiendo los ejemplos académicos (tutorial_19), y CERRADO

Al escribir ejemplos didácticos sobre Stage 52 (paquete separado de 23 tutoriales) apareció un
crash real (SIGSEGV bajo Wine) reusando un objeto `raii` después de un `catch` real. Investigado
a fondo en una sesión dedicada: **confirmado como bug de Wine, NO del compilador** — el mismo
binario de `nsc` corre perfecto en Windows real. Ver
`claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md` en el proyecto para el análisis completo.
No relacionado con `NativeSharp.Core.Collections.Generic` en sí, pero relevante para Stage 52
porque se descubrió usando sus tipos (`Exception`, `raii`) y porque establece un precedente
metodológico: de ahora en más, un crash bajo Wine se confirma contra Windows real antes de
tratarlo como bug de compilador.

### 10.5 Estado final verificado de Stage 52

```text
nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)
ctest: 210/210 PASS (100%)
Stage52Collections.exe (Wine 9.0): 149, sin crash, determinista
Stage52xForeach.exe (Wine 9.0): 91, sin crash, determinista
```

Todos los contenedores genéricos + `foreach` verificados por EJECUCIÓN REAL bajo Wine 9.0, no
solo estructuralmente. Verificación en Windows real (no solo Wine) de la suite específica de
Stage 52: entregada como paquete de verificación aparte en el cierre final (ver
`HANDOVER_STAGE52_TO_STAGE53.md`).
