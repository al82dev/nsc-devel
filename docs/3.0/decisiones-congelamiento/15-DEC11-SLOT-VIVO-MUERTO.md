# Congelamiento 3.0 DEC-11 / D-03 — estado vivo/muerto por slot en arreglos de structs con destructor

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `6a5dc1a10`.

## Propósito y problema

C-12 hizo que un arreglo dueño destruya sus elementos struct con destructor, o con campos `owned`, antes de liberar
el bloque. `move(a[i])` y `delete a[i]` dejan el slot en cero, pero el arreglo lo destruía igual al final, y
`a[i] = v` destruía el valor anterior aunque nunca hubiera existido. Hallazgo D-03 de la auditoría:

| Sonda | Correcto | Antes |
|---|---|---|
| `D1DeadSlotDrop` (`delete items[0]` y fin de scope) | 2 destrucciones | 3 (exit 231) |
| `L2ReverseStruct` (`List<Guard>` de 3 valores, `Reverse`) | 3 destrucciones | 10 (exit 1006) |

`List<T>.Add` asigna sobre un slot de capacidad que nunca se escribió. Antes eso corría el destructor sobre el
valor por defecto.

Decisión de Alberto Huerta Luna (2026-10-26). Es el modelo de vida de NativeSharp:

- `new T[n]` reserva e inicializa el almacenamiento, pero los slots de un tipo con destructor nacen muertos.
- La primera escritura completa revive el slot.
- Una escritura de campo deja los demás campos en su valor por defecto antes de marcar vivo el slot.
- `move`, `delete` o una extracción destructiva lo vuelven a marcar muerto.
- Solo los slots vivos ejecutan destructor.
- Un slot muerto no puede leerse como un valor vivo.

Resultados correctos fijados por la decisión: D1 = 2, L2 = 3, C12ListGrow = 12345, C12ListRemove = 2213,
D2MultiDim = 1.

## SCOPE LOCK

- **Objetivo.** Un bit de vida por elemento en los arreglos cuyo elemento requiere drop de valor
  (`typeRequiresValueDrop`), con las seis reglas de arriba.
- **Naturaleza.** Corrección de D-03 por decisión explícita; cambio de layout aislado a esos arreglos.
- **Debe quedar idéntico.**
  - Arreglos de cualquier otro tipo de elemento: layout y código.
  - Arreglos de referencias `owned` (siguen usando el puntero nulo como slot vacío).
  - El corpus de `await` y las sondas de D-01.
- **Modificable.**
  - LLVM: `OopLlvmArraysAndStores.cpp`, `OopLlvmExpressionMachine.cpp`, `OopLlvmCleanup.cpp`,
    `OopLlvmConstruction.cpp`, `OopLlvmFunctionDefinition.cpp`, `OopLlvmStaticState.cpp`,
    `OopLlvmCoroutines.cpp` y `OopLlvmImplementation.h`.
  - Tipos nuevos: `OopLlvmArrayElementAccess.h` y `OopLlvmArrayElementLiveness.h`.
  - `LlvmTrapReason.h`: `DeadElementRead = 18`.
  - MIR: `MirFunctionLowerer.cpp` (slots de D-01).
  - `Freeze30ContractTests.cpp`.
- **Prohibido.**
  - Destruir un slot muerto.
  - Introducir otro tipo de almacenamiento sin inicializar.
  - Tocar la biblioteca.
- **Terminación.** Sondas de abajo en `-O0` y `-O2`.

## Contrato

1. **Layout.** Detrás de los datos de un arreglo cuyo elemento requiere drop de valor va un mapa de bits:
   - empieza en `dataOffset + longitud * tamañoElemento`;
   - mide ⌈n/8⌉ bytes;
   - vale igual para arreglos rectangulares, con la longitud total en el offset 0.

   El bloque se pone en cero al reservar, así que todo slot nace muerto. Un inicializador
   (`new T[] { a, b }`, colecciones con spread) escribe cada elemento, y todos nacen vivos.
2. **Clasificación.** Antes de bajar un cuerpo (y cada inicializador estático),
   `classifyArrayElementAccesses` decide qué hace la expresión con cada acceso a elemento:
   - `WholeStore`: destino de `a[i] = v`;
   - `FieldStore`: base de una cadena de campos bajo un destino de store, `a[i].F.G = v`;
   - `Move`: operando de `move(a[i])`;
   - `Delete`: operando de `delete a[i]`;
   - `Read`: cualquier otro uso, como una carga, la lectura de un campo, un receptor de método o un `ref`.
3. **Efecto de cada acceso.**
   - `a[i] = v`: destruye el valor anterior solo si estaba vivo; después el slot queda vivo.
   - `a[i].F = v` sobre un slot muerto: el slot se pone en cero y se marca vivo antes de escribir el campo.
   - `move(a[i])`: si el slot está muerto, atrapa (`DeadElementRead`, exit 146). Si está vivo, lo pone en cero y lo
     marca muerto.
   - `delete a[i]`: si el slot está vivo, lo destruye, lo pone en cero y lo marca muerto. Sobre un slot muerto no
     hace nada.
   - Leer un slot muerto atrapa (`DeadElementRead`).
   - La destrucción del arreglo (fin de un `raii`/`owned`, `delete[]` de un local dueño) destruye solo los slots
     vivos.
4. **Interacción con D-01.** La dirección de un elemento ya no se guarda entera en un slot de repetición. Se
   guardan sus operandos (el arreglo y los índices) y la dirección se recalcula en la repetición. Así el store, move
   o delete siempre ve el bit calculado junto a ella. Funciones: `assignReplaySpillSlotsBeside` y
   `replaySpillDescends` en MIR, y `markCoroutineReplayOperands` en LLVM.

## Límites

- Una escritura a través de una referencia o un `Span` (`ref a[i]`, `Spans.OfRange`) no cambia el bit. Permutar
  elementos vivos dentro del rango vivo (como `Algorithm.Sort` sobre `List<T>`) es coherente. Escribir por esa vía
  en un slot muerto no lo revive.
- Un receptor de método es una lectura: `a[i].Metodo()` sobre un slot muerto atrapa, aunque el método solo
  escriba.

## Cambio

- Archivos de tipo nuevos: `OopLlvmArrayElementAccess.h` (enum) y `OopLlvmArrayElementLiveness.h` (byte y máscara del
  bit).
- `OopLlvmArraysAndStores.cpp`:
  - `lowerArrayAllocate` y `lowerRangeArrayAllocate` reservan el mapa. Un inicializador marca todo vivo.
  - `lowerRangeArrayElementAddress` aplica el acceso.
  - `storeFromParts` destruye condicionalmente (`dropReplacedArrayElement`) y marca vivo.
  - Funciones nuevas:
    - `arrayElementLivenessTracked`, `arrayAllocationElementType`, `arrayAccessElementType`;
    - `classifyArrayElementAccesses`, `arrayElementAccessFor`;
    - `arrayLivenessByteCount`, `arrayElementLiveness`;
    - `arrayElementIsAlive`, `setArrayElementAlive`;
    - `applyArrayElementAccess`, `reviveArrayElement`;
    - `takeArrayElementLiveness`, `dropReplacedArrayElement`.
- `OopLlvmExpressionMachine.cpp`: `finishArrayElementFrame` aplica el acceso, y `MoveArrayElement` marca muerto.
- `OopLlvmCleanup.cpp`:
  - `emitValueElementDelete` borra solo un slot vivo;
  - `emitNativeArrayValueElementDrop` salta los muertos.
- `OopLlvmConstruction.cpp`: `lowerDeallocation` pasa el bit.
- `OopLlvmFunctionDefinition.cpp` y `OopLlvmStaticState.cpp`: clasificación antes de bajar.
- `Freeze30ContractTests.cpp`:
  - `testArrayElementLiveness`, con 16 ejecuciones;
  - `C12AssignAndMove` pasa de 10302 a 132;
  - `C12ListRemove` pasa de 22013 a 2213.

  Los valores viejos contaban destrucciones de slots muertos. Cada cambio queda comentado en la prueba.

## Validación

nsc Release recompilado (exit 0). Sondas en `A:\temp\nsc-dec\d11`. Destructor `Drops = Drops * 10 + Id + 1`,
salvo en las sondas C12, que usan el preludio de la prueba (`Drops * 10 + Id`).

| Sonda | Antes | Ahora (`-O0` y `-O2`) |
|---|---|---|
| D1DeadSlotDrop | 231 (3 destrucciones) | 23 (2) |
| D2MultiDim | 6 | 1 |
| L2ReverseStruct | 1006 | 306 (3 destrucciones, suma 6) |
| P1 (`a[0].Id = 1; a[0] = move(g)`) | 2118 | 28 |
| P2 (`List.Add` ×2) | 1142 | 42 |
| P3 (`move(a[1])`) | 312 | 32 |
| P4 (delete y revivir con store) | 2136 | 236 |
| P5 (`owned`, `delete a[1]`, `delete[]`) | 312 | 32 |
| P6 (campo `owned Guard[]`, move) | 231 | 23 |
| P7 (leer slot muerto) | — | 146 |
| P8 (mover slot muerto) | — | 146 |
| P9 (inicializador) | — | 32 |
| P10 (delete doble y de slot muerto) | — | 2 |
| P11 (receptor muerto) | — | 146 |
| C12RaiiStruct, C12Delete, C12OwnedField | 21 | 21 |
| C12AssignAndMove | 10302 | 132 |
| C12ListGrow | 12345 | 12345 |
| C12ListRemove | 22013 | 2213 |
| C12LinkedList (con `try` alrededor de `RemoveFirst`) | — | 1123 |
| R14 (`a[Next()].Id = await Slow()`) | — | 1506 |
| R15 (`a[Next()] = await Make()`) | — | 1506 |

- Los valores de «Antes» en D1, D2, L2 y P1–P6 se midieron el mismo día con el compilador anterior a este cambio (`ba91ef419`; `6a5dc1a10` solo movió un
  struct de archivo).
- Las sondas de D-01 (R1–R13) dan lo mismo.
- Corpus de `await` (51 programas): mismos resultados de compilación y de salida que `d04`.

## Hallazgo (no corregido aquí)

`C12LinkedList` de `Freeze30ContractTests` ya no compila en `HEAD`. P-1 hizo que `LinkedList.RemoveFirst` declare
`throws InvalidOperationException`, y la prueba lo llama sin `try` ni `throws`. Es previo a este cambio. La sonda
`C12LinkedListT` envuelve la llamada en `try` para verificar DEC-11.

## No verificado

- `Freeze30ContractTests` no está configurado en los árboles locales; sus casos nuevos son las sondas de arriba.
- `Dictionary`, `HashSet`, `SortedSet`, `SortedList` y `PriorityQueue` con valores destructibles: elementos 91–96 del
  plan.
- Batería completa: queda para el cierre final.
