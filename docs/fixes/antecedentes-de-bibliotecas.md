# Antecedentes de bibliotecas contrastados con el compilador vigente

Fichas LKF-176, 177, 179, 180, 184, 186, 191, 193, 195, 199, 204, 205 y 210. Cada una parte de un
comentario de biblioteca que registraba un límite del compilador. La prueba construye primero Core y
Collections.Generic desde el repositorio y después compila cada caso contra esas bibliotecas.

## Defectos corregidos

- **LKF-186.** `delete[] a` sobre un parámetro `owned T[]` liberaba el bloque y la salida del
  parámetro lo liberaba otra vez (`0xC0000374`, corrupción de heap), con o sin elementos y también
  para `owned int[]`. `consumeDeletedOwnedParameter` solo consumía el `delete` escalar de clases y
  cadenas. Ahora también consume el `delete[]` de un parámetro arreglo `owned`: el parámetro queda
  movido y el HIR vacía su slot antes de liberar, como ya hacía con un local `owned`. Usar el
  parámetro después o borrarlo dos veces se rechaza ("use of moved-from parameter 'a'").
- **LKF-210.** `&s[0]` sobre una cadena se rechazaba con "bit fields have no address". Una lectura
  indexada tiene ahora su propio diagnóstico: un carácter indexado es un valor y
  `String.CharacterPointer(text, index)` da su dirección; el resultado de un indexador por valor es
  un temporal y solo se direcciona si el getter devuelve `ref`. El texto de campos de bits se conserva
  para los campos de bits.

## Verificado sin cambio de compilador

- **LKF-176.** `Algorithm.Find(v, 9)` infiere `T`; la evidencia contradictoria se rechaza con
  "cannot be inferred … write them explicitly".
- **LKF-177.** Un grupo de métodos pasado directo como predicado compila, con y sin argumentos de tipo.
- **LKF-179.** Structs de 32 y 40 bytes pasan y vuelven por delegados y lambdas con captura.
- **LKF-180.** Un getter de indexador con `throws` funciona; lanzar sin declararlo se rechaza (C-13).
- **LKF-184.** Asignar a un campo `owned` simple y por el indexador `owned` de `List` transfiere la
  propiedad y libera el valor reemplazado.
- **LKF-191.** Una llamada a método sobre un receptor struct temporal compila y ejecuta.
- **LKF-193.** Un parámetro genérico como retorno de delegado funciona en O0 y O2.
- **LKF-195.** Un campo `borrowed` se rechaza; una referencia de clase sin calificador, como la de
  `ScopedLock`, es el control aceptado.
- **LKF-199.** `Add` mientras vive una vista de `AsSpan` se rechaza ("cannot call potentially
  mutating method while a borrowed field view is alive"); terminado el ámbito de la vista, se acepta.
- **LKF-204.** `Optional<int>.None()` parsea; falla solo porque `Optional<T>` no declara `None`. Los
  miembros estáticos de genéricos cerrados se resuelven por tipo cerrado.
- **LKF-205.** `Optional<StringView>` se rechaza porque un campo no puede tener tipo vista.

Los comentarios de `Algorithm.ns`, `Delegates.ns`, `LinkedListNode.ns`, `List.ns`, `Stopwatch.ns`,
`ByteBuffer.ns` y `Optional.ns` conservan su texto y añaden el resultado de este contraste.

## Correcciones al validar regresiones

- **Referencia a elemento y `delete[]`.** La regla de `1b122e6cd` (LKF-063) consideraba viva una
  referencia `ref` a un elemento hasta el final de su ámbito y rechazaba `P3MoveOnlyRefLocal` de
  `nsc.freeze30-contracts`, cuyo último uso precede al `delete[]`. Ahora la referencia está viva solo
  si puede leerse después: un identificador posterior con su nombre, o un uso dentro de un bucle que
  contiene el `delete[]` pero no la declaración (la siguiente iteración la lee después). La misma
  comprobación de vida se aplica a una vista local en el rechazo "cannot delete[] array while a
  borrowed view is alive". `historical-language-boundaries` añade el caso aceptado y el del bucle.
- **Elemento 141 en fixtures.** `nsc_stage49_raii_move_tests` (NSC-NEW-014) y
  `nsc_stage50_11_ownership_execution_tests` esperaban que `Resource a = new Resource()` no fuera
  dueño. Los negativos usan ahora un alias de un parámetro prestado o un destino `null`, y conservan
  el diagnóstico que prueban. El criterio "sin reflexión del host" de stage49 busca la entrada de
  liberación del descriptor (`raii.release.entry`) y admite solo los valores `destruction.base.*`
  del propio descriptor.
- **`P3SortedDictionaryGet`** no referenciaba Collections.Generic; ahora sí.
- **Modo archivo de `C05FileMode`.** `findCoreProject` toma el Core más cercano al compilador; el
  árbol de evidencia conservaba una copia de `lib/NativeSharp.Core` del 2026-11-08 sin los punteros
  compartidos. Se sincronizó con el repositorio (solo archivos añadidos y actualizados).

## Prueba

`nsc.compiler.library-antecedent-boundaries`: 31 casos, cada rechazo con su control, O0/O2.
