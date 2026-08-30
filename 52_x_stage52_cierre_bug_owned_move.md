# Stage 52 — Cierre real: el bug de `owned` guardado sin `move()`

## 1. Contexto

Este documento continúa `52_x_nativesharp_foreach.md` y responde directamente al documento de
cierre que Alberto subió (`STAGE52_FALTANTES_Y_BLOQUEOS_PARA_CIERRE.md`). Cubre:

- la corrección del bug P0 #2 (`emitOwnedParameterDestroy` para tipos de valor) — ya estaba hecha
  antes de este documento, se re-verifica aquí con Wine real;
- la regresión focal P0 #3 (`owned int` + llamada anidada), como test C++ dedicado con ejecución
  real;
- la re-validación P0 #4 de los 4 binarios del checklist;
- el hallazgo, bisección y corrección de un SEGUNDO bug real, no descrito en el checklist
  original (porque el checklist se escribió antes de que este bug pudiera aislarse) — el que de
  hecho era la causa de que `Stage52Collections.exe` siguiera crasheando después de arreglar el
  bug #2;
- la auditoría P1 #5/#6 (helper centralizado `requiresOwnedDestruction`).

## 2. El bug: `owned` guardado con `=` en vez de `move()`

### 2.1 Síntoma

`Stage52Collections.exe`, después de corregir `emitOwnedParameterDestroy` para tipos de valor
(bug ya documentado en `52_x_nativesharp_foreach.md`), seguía crasheando bajo Wine con un page
fault en una dirección de ejecución inválida (`0x00007ffffe220528`), aislado por bisección a
`RunList()`, específicamente a la sección que usa `List<Payload>` (un tipo referencia) — no a
`List<int>` (que ya funcionaba correctamente).

### 2.2 Bisección (resumen del proceso real, no simplificado a posteriori)

La bisección inicial documentada en `52_x_nativesharp_foreach.md` especulaba que el problema
estaba en `List<T>.Sort()`/`Exists()`. Esa hipótesis resultó **incorrecta**. La bisección real,
continuada en esta sesión, siguió estos pasos, cada uno con ejecución real bajo Wine (no solo
compilación):

1. **¿Es `GetEnumerator()`/`ListEnumerator<T>` (referencia circular genérica)?** Se quitó
   `GetEnumerator()` de `List<T>` temporalmente y se recompiló. El crash persistió, en la misma
   dirección exacta. Hipótesis descartada.
2. **¿Es específico de `List<T>`?** Se aisló el patrón mínimo: `raii List<Payload> people; raii
   Payload alice; people.Add(move(alice));` seguido de la destrucción de `people` al final de
   `Main()`. Crasheaba.
3. **¿Es el propio destructor (`~List()`)?** Se quitó el destructor de `List<T>` por completo.
   El programa corrió limpio (exit code correcto). Esto confirmó que el destructor era el punto
   donde el crash se manifestaba (no necesariamente donde se originaba).
4. **¿`delete[] items` o el loop `delete items[i]`?** Se probó cada mitad del destructor por
   separado. Solo el loop `delete items[i]` (borrar un elemento individual leído del arreglo)
   reproducía el crash; `delete[] items` solo (liberar el arreglo completo) no.
5. **¿Requiere que `delete` esté dentro de un destructor?** Se movió el mismo `delete items[i]`
   a un método normal (`ClearAll()`), llamado explícitamente desde `Main()` en vez de disparado
   por RAII. Seguía crasheando — no es específico de destructores.
6. **¿Requiere índice variable (loop) o también falla con índice constante?** `delete items[0]`
   (constante) crasheaba igual que la versión con loop. Descartado.
7. **¿Requiere genéricos?** Se escribió una clase NO genérica (`ConcreteHolder`, con
   `owned Payload[] items` directo, sin `T`) con el mismo patrón. **Crasheó igual.** Esto
   descartó por completo cualquier hipótesis relacionada con monomorfización/generics — el bug
   es genérico en el sentido de "afecta a cualquier tipo referencia", no en el sentido de
   "requiere `T` genérico".
8. **¿Requiere la llamada anidada dentro de `Add()` (`EnsureCapacity`)?** Se quitó por completo
   la llamada anidada, dejando `Add()` como `items[count] = value; count = count + 1;` sin
   ninguna otra llamada. Seguía crasheando. Esto descartó el mecanismo del bug #1 original
   (Stage 50.8) como causa de este segundo crash — son bugs relacionados en espíritu pero
   mecánicamente distintos.
9. **¿Requiere `delete` en absoluto, o el elemento nunca llega a ser válido?** Se probó
   `delete items[0]` sobre un arreglo recién creado (`new Payload[4]`) sin asignar nada — es
   decir, sobre un puntero `null` legítimo. **No crasheó.** Esto confirmó que el problema
   necesita que el elemento haya sido realmente asignado antes.
10. **¿Es la asignación en sí, independientemente de `owned`/parámetros?** Se probó
    `items[0] = new Payload();` directo (sin parámetro, sin `move()`, sin `raii`) seguido de
    `delete items[0]`. **No crasheó.** Esto acotó el bug específicamente a la combinación
    "parámetro" + "asignación al arreglo".
11. **¿Requiere que el parámetro sea `owned` específicamente, o cualquier parámetro?** Se probó
    con un parámetro PLANO (no `owned`) asignado al arreglo, seguido de `delete`. **No
    crasheó.** Esto confirmó que `owned` es la pieza necesaria.
12. **La prueba decisiva**: se cambió `items[0] = value;` por `items[0] = move(value);` dentro
    de `Add(owned Payload value)`, sin tocar nada más. **El crash desapareció por completo.**

### 2.3 Causa raíz

Un parámetro `owned T value` que NO se consume con `move()` en el punto donde se guarda
(`items[count] = value;` en vez de `items[count] = move(value);`) dispara, al retornar del
método, el cleanup automático de "parámetro `owned` no consumido" (`emitOwnedParameterDestroy`
en `src/LLVM/ProjectOopLlvmLowerer.cpp`) — cuyo comportamiento es, en sí mismo, **correcto**: si
un parámetro `owned` sigue teniendo un puntero no-nulo cuando el método termina, ese cleanup lo
destruye, exactamente como el destructor de fin-de-scope de una variable `raii` local.

El problema es que `items[count] = value;` es una asignación de puntero PLANA — copia el bit
pattern del puntero dentro del arreglo, pero no le dice al compilador "la propiedad ya se
transfirió". El storage propio del parámetro `value` sigue apuntando al mismo objeto. Así que:

1. `Add()` guarda el puntero en `items[count]`.
2. `Add()` retorna. Su cleanup automático ve que `value` sigue siendo no-nulo y lo destruye
   (llama al destructor del objeto y libera su memoria).
3. `items[count]` ahora es un puntero colgante (dangling) hacia memoria ya liberada.
4. La próxima vez que algo lea `items[count]` y lo trate como objeto vivo (otro `Add()` que
   reubica el arreglo, `~List()`, `delete items[i]`, un `foreach`...) — en este caso, tres pasos
   después, dentro de `RunList()` — se carga el descriptor de clase (vtable) desde memoria ya
   liberada, que contiene basura, y el intento de despachar el destructor virtual salta a una
   dirección de código inválida. De ahí el "Unhandled page fault on execute access" de Wine: no
   es un fallo al leer datos, es un salto a una dirección que nunca fue código real.

`move(value)` anula el storage propio del parámetro como parte de la transferencia — así que el
cleanup automático de fin-de-método ve `null` y correctamente no hace nada. Exactamente un dueño
en todo momento, tal como exige el modelo de ownership de NativeSharp.

### 2.4 Por qué esto NO es un bug de compilador

A diferencia del bug #2 original (P0 #2 del checklist, `emitOwnedParameterDestroy` tratando un
tipo de valor como puntero), este es un bug de **la librería**, no del compilador: el compilador
está haciendo exactamente lo que el modelo de ownership de NativeSharp dice que debe hacer. El
código de `List.ns`/`Queue.ns`/etc. estaba usando la sintaxis incorrecta (`=` en vez de
`move()`) para el patrón "recibir un parámetro `owned` y guardarlo en un slot propio" — el mismo
patrón que `LinkedList.ns` (escrito más tarde en el proyecto) ya usaba correctamente en todos
sus sitios (`created[0].SetValue(move(value))`).

### 2.5 Auditoría y corrección en toda la librería

Una vez identificado el patrón exacto ("un parámetro `owned` guardado en un slot de arreglo/campo
con `=` en vez de `move()`"), se auditó sistemáticamente cada colección genérica de
`lib/NativeSharp.Core.Collections.Generic/` buscando la misma forma. Se encontraron y corrigieron
7 sitios adicionales, todos con el mismo patrón y la misma corrección:

| Archivo | Método(s) | Antes | Después |
|---|---|---|---|
| `List.ns` | `Add` | `items[count] = value;` | `items[count] = move(value);` |
| `List.ns` | `SetAt` | `items[index] = value;` | `items[index] = move(value);` |
| `List.ns` | `Insert` | `items[index] = value;` | `items[index] = move(value);` |
| `Queue.ns` | `Enqueue` | `items[tail] = value;` | `items[tail] = move(value);` |
| `Stack.ns` | `Push` | `items[count] = value;` | `items[count] = move(value);` |
| `Dictionary.ns` | `InsertNew` | `entryKeys[slot] = key; entryValues[slot] = value;` | `move(key)`/`move(value)` |
| `Dictionary.ns` | `AddOrUpdate` | `entryValues[index] = value;` | `move(value)` |
| `Dictionary.ns` | `SetValue` | `entryValues[index] = value;` | `move(value)` |
| `SortedList.ns` | `InsertAt` | `keys[index] = key; values[index] = value;` | `move(key)`/`move(value)` |
| `SortedList.ns` | `AddOrUpdate` | `values[insertAt] = value;` | `move(value)` |
| `SortedList.ns` | `SetValue` | `values[index] = value;` | `move(value)` |
| `PriorityQueue.ns` | `Enqueue` | `elements[count] = element; priorities[count] = priority;` | `move(element)`/`move(priority)` |
| `LinkedListNode.ns` | `SetValue` | `slot[0] = value;` | `move(value)` |
| `LinkedListNode.ns` | `SetNext` | `nextSlot[0] = node;` | `move(node)` |

**No afectados** (verificado leyendo cada uno): `HashSet.ns`, `SortedSet.ns`,
`SortedDictionary.ns` (todos ya llamaban `move(...)` correctamente al delegar en su colección
interna), y los swaps internos de `List.ns`/`PriorityQueue.ns` (`Swap`/`SwapAt`: relocación
dentro del MISMO arreglo entre variables locales `const`, no parámetros — patrón ya documentado
como seguro en el comentario de clase de `List.ns`).

## 3. Re-validación real (Wine) tras la corrección

| Binario | Esperado | Obtenido | Determinista (3 corridas) |
|---|---|---|---|
| `repro_1_minimal.exe` | 42 | **42** | sí |
| `repro_2_owned_no_nested_call.exe` | 5 | **5** | sí |
| `repro_3_owned_nested_call.exe` | 1, sin crash | **1, sin crash** | sí |
| `Stage52Collections.exe` | sin crash (ver nota) | **149, sin crash** | sí (3/3 corridas idénticas) |
| `Stage52xForeach.exe` (foreach, entrega anterior) | 91 | **91** | sí |

**Nota sobre el "exit code 0" de `Stage52Collections.exe`**: el checklist de Alberto lista
"exit code 0" como resultado esperado para este binario. Se investigó de dónde viene ese valor:
`Main()` de `Stage52Collections` no es una función de tipo pass/fail (no devuelve 0 en éxito y
distinto de 0 en fallo) — es la SUMA de 13 funciones `Run*()` que cada una devuelve un checksum
acumulado (conteos, sumas, índices) de sus propias operaciones sobre cada colección. No hay
ninguna razón de diseño para que esa suma dé exactamente 0, y `WINE_CHECKLIST_STAGE52.md` (el
documento de preparación del propio ciclo de cierre anterior) ya lo dejaba explícito: "no hay
valor fijo esperado documentado aquí -- lo importante es que NO crashee". El "exit code 0" del
checklist más reciente parece haber sido un placeholder escrito antes de tener el valor real
verificado. El valor real, determinista y sin crash, es **149** (confirmado en 3 corridas
independientes bajo Wine 9.0).

Suite completa: `ctest` **208/208 PASS** (207 previos + `nsc_stage52_4_owned_cleanup_runtime_tests`,
el nuevo test de regresión con ejecución real, ver sección 4).

## 4. Regresión: ejecución real, no solo estructural (P0 #3 y P1 #11)

`tests/unit/Stage52_4OwnedCleanupRuntimeTests.cpp` (nuevo) cierra la brecha que dejó que ambos
bugs pasaran desapercibidos: **todo el resto de la suite de Stage 52 verifica hasta "compila +
LLVM verifica + COFF es válido", nunca ejecuta el binario real**. Este archivo:

1. Emite un `.exe` de Windows x64 real a través del `Driver` real (el mismo código que corre
   `nsc --emit-project-oop-exe ... -o out.exe`), no una versión reducida ad-hoc.
2. Cuando Wine está disponible (detectado dinámicamente; se degrada a solo-estructural sin fallar
   si no lo está, para no romper CI en un host sin Wine), lo EJECUTA de verdad y verifica el
   exit code real.
3. Cubre tres casos:
   - **`Holder.Consume(owned int value) { NestedCall(); }`** (el repro exacto del checklist P0
     #3): sema acepta la firma, el IR de `Consume()` no contiene ningún bloque
     `owned.param.destroy`/`owned.param.descriptor` (prueba mecánica de que el parámetro entero
     no se trata como referencia), COFF válido, y ejecución real retornando **16** (esperado a
     mano: `Consume(7)` dos veces, cada una hace `NestedCall()` (+1) y luego `+= value` (+7) →
     `(1+7)+(1+7)=16`).
   - **Caso negativo**: la MISMA forma que tenía el bug (`items[0] = value;` sin `move()`) se
     mantiene como fixture intencionalmente buggy — debe seguir COMPILANDO (no es un error que
     el análisis semántico pueda rechazar en general) pero DEBE crashear al ejecutarse. Esto es
     deliberado: confirma que el peligro es real (no solo teórico) y sirve de contraste directo
     con el caso positivo.
   - **Caso positivo (el fix)**: la misma forma con `move(value)` — compila, no crashea, y
     devuelve el valor exacto esperado (**30**) bajo ejecución real.

Total: 24 verificaciones, todas en verde, incluyendo las 6 verificaciones de ejecución real bajo
Wine (2 por caso × 3 casos).

## 5. Endurecimiento del lowerer (P1 #5 / P1 #6)

### 5.1 Auditoría P1 #5

Se revisaron los puntos de cleanup nombrados por el checklist en
`src/LLVM/ProjectOopLlvmLowerer.cpp`:

- **`OwnedParameterCleanup` (`emitOwnedParameterDestroy`)**: ya corregido (bug #2 original).
- **RAII local cleanup (`emitRaiiDestroy`)**: NO tenía guardia de tipo. Investigado si esto era
  explotable: `raii` está restringido en `ProjectMethodBodyModel.cpp` a
  "class/interface reference type or native string" — un `raii` de tipo valor se rechaza en
  análisis semántico, antes de llegar a HIR/MIR/LLVM, incluso para un `T` genérico (el chequeo
  corre sobre el `typeKey` ya cerrado/monomorfizado). No es explotable hoy. Aun así, se agregó
  una guardia defensiva (ver 5.2) — el bug #2 original es precisamente la lección de que una
  invariante en la que "todos confiaban" puede no sostenerse.
- **Array element cleanup**: cubierto por `lowerDeallocation`, que ya tenía su propia
  clasificación value-type-no-op (`node->valueTypeNoOpDeletion`, calculada en
  `ProjectOopHirModel.cpp`) desde el cierre original de Stage 52 (fix 1 del archivo
  `Stage52GenericCollectionsCompilerFixesTests.cpp`).
- **Owned return temporary cleanup**: no existe como ruta de código separada en el lowerer hoy —
  no hay una función dedicada a esto; no se encontró nada que auditar bajo ese nombre.
- **Exceptional cleanup / cleanup funclets**: `emitExceptionalCleanupChain` no calcula su propia
  clasificación owned==ptr; delega en `emitCleanupAction`, que a su vez delega en
  `emitRaiiDestroy`/`emitOwnedParameterDestroy`/`emitUsingDispose` — es decir, hereda
  automáticamente cualquier corrección hecha en esos tres, sin tener su propia copia divergente
  del chequeo.

### 5.2 Helper centralizado (P1 #6)

Se agregó `requiresOwnedDestruction(typeKey)` en `ProjectOopLlvmLowerer.cpp`, la respuesta única
a "¿este tipo cerrado posee memoria propia que un cleanup debe liberar?" (verdadero para
clase/interfaz o `builtin:string`; falso para todo lo demás). `emitOwnedParameterDestroy` y
`emitRaiiDestroy` ahora usan este helper en vez de repetir la condición cada uno por su cuenta.
`lowerDeallocation`/`ProjectOopHirModel.cpp` mantienen su propia clasificación equivalente en la
capa HIR (`isReferenceType`/`nativeString`) — unificar HIR y LLVM en una sola función compartida
entre archivos queda como una oportunidad de deduplicación futura, fuera del alcance de este
pase, que se limita a los tres sitios que ya vivían en el mismo archivo.

Verificado: `ctest` completo sigue en 208/208 tras el refactor, y los 5 binarios de la sección 3
se re-verificaron bajo Wine con el compilador reconstruido, con resultados idénticos.

## 6. Archivos modificados en esta corrección

```text
Compilador:
  src/LLVM/ProjectOopLlvmLowerer.cpp
    - requiresOwnedDestruction(typeKey) -- helper centralizado (P1 #6)
    - emitOwnedParameterDestroy / emitRaiiDestroy -- usan el helper en vez de condiciones propias

Librería (lib/NativeSharp.Core.Collections.Generic/):
  List.ns            -- Add/SetAt/Insert: move(value)
  Queue.ns            -- Enqueue: move(value)
  Stack.ns            -- Push: move(value)
  Dictionary.ns       -- InsertNew/AddOrUpdate/SetValue: move(key)/move(value)
  SortedList.ns       -- InsertAt/AddOrUpdate/SetValue: move(key)/move(value)
  PriorityQueue.ns    -- Enqueue: move(element)/move(priority)
  LinkedListNode.ns   -- SetValue/SetNext: move(value)/move(node)

Tests:
  tests/unit/Stage52_4OwnedCleanupRuntimeTests.cpp (nuevo) -- ejecución real bajo Wine
  CMakeLists.txt -- registro de nsc.stage52-4.owned-cleanup-runtime

Documentación:
  52_x_nativesharp_foreach.md -- sección 6b corregida (causa raíz real, ya no Sort()/Exists())
  52_x_stage52_cierre_bug_owned_move.md (este documento, nuevo)
```

## 7. Estado frente al criterio de CLOSED del checklist

1. ✅ el bug de cleanup de `owned` value-type está corregido (ya lo estaba, re-verificado aquí);
2. ✅ existe una regresión que impide reintroducirlo (`Stage52_4OwnedCleanupRuntimeTests.cpp`,
   con ejecución real, no solo estructural) — y además se descubrió y corrigió un SEGUNDO bug
   real que bloqueaba el mismo criterio, con su propia regresión;
3. ✅ el ejemplo completo de Generic Collections ejecuta sin crash (`Stage52Collections.exe`,
   verificado 3/3 bajo Wine 9.0);
4. ✅ las suites estructurales siguen en verde (208/208, antes 207/207 + 1 test nuevo);
5. ⚠️ documentación: corregida la sección más importante (la que tenía una causa raíz
   INCORRECTA) en `52_x_nativesharp_foreach.md`, y agregado este documento. Pendiente: aplicar
   la misma distinción "structural conformance" vs "runtime execution conformance" a
   `STAGE52_CHECKPOINT.md`, `52_nativesharp_generic_collections.md`,
   `52_nativesharp_generic_collections_implementacion_paso_a_paso.md`,
   `PART5_CONFORMANCE_STAGE52.md` y el handover hacia Stage 53 (P1 #7-10 del checklist) —
   trabajo real de escritura que no se completó en esta entrega, ver la comunicación de cierre
   para el detalle;
6. ⏳ FULL/SOURCE-ONLY/manifests/hashes: pendiente de regenerar desde este árbol corregido;
7. ⏳ fresh package: pendiente de reproducir tras el punto anterior.
