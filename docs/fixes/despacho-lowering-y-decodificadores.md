# Despacho, lowering, almacenamiento y decodificadores

Fichas LKF-391, 392, 396 a 402, 412, 413, 422, 423, 434, 435, 441, 442, 444, 465, 480 y
NSC-NEW-028. Cada ficha se contrastó con programas que se compilan y ejecutan en O0 y O2, o, si es
una función interna sin entrada desde fuente, con una prueba unitaria.

## Defectos corregidos

- **LKF-391.** Para implementar un método de interfaz, la búsqueda tomaba el primer miembro con la
  misma firma. Si ese miembro no podía implementarlo (privado, estático, otro retorno o lifetime),
  la búsqueda terminaba y nunca llegaba a la implementación pública de la base: `class D : B, I
  { private int F() ... }` se rechazaba con "does not implement". Ahora ese miembro se salta, como en
  el mapeo de interfaces de C#. Una redeclaración abstracta sigue terminando la búsqueda.
- **LKF-392.** La definición de una función de namespace conservaba los parámetros de su prototipo.
  `int F(int x);` seguido de `int F(int y) { return y; }` fallaba con "unknown identifier 'y'". El
  cuerpo enlaza ahora los nombres, lifetimes y restricciones de su propia declaración. La clave de
  firma ya garantiza que tipos y modos de paso coinciden.
- **LKF-442.** La ranura del valor gobernante de un `match` se reservaba donde estaba el `match`.
  Dentro de un bucle largo la pila crecía en cada vuelta y se desbordaba (0xC00000FD, en O0 y en O2
  con un struct). La ranura vive ahora en el bloque de entrada.
- **NSC-NEW-028.** Un VLA declarado en el cuerpo de un bucle (`for`, `while`, `do`, `loop`,
  `foreach`) hacía crecer la pila en cada iteración y un bucle de 200000 vueltas la desbordaba en O0.
  El puntero de pila se guarda ahora antes del bucle y se restaura al empezar cada iteración y a la
  salida (`llvm.stacksave`/`llvm.stackrestore`). Solo se acota el almacenamiento VLA, que HIR marca.
  `stackalloc` conserva la vida de la función, como en C#. Las corrutinas y los funclets mantienen la
  forma anterior.
- **LKF-444.** Una asignación condicional nula escribe la ranura directamente. A un elemento de un
  arreglo owned o a un campo owned, la asignación ordinaria libera antes el valor anterior; esta ruta
  no lo hacía: `a?[0] = new P()` dos veces perdía el primer objeto. Además, los clasificadores de
  destino owned solo reconocían `a[i]` y `p.F`. Ahora la forma condicional nula se clasifica con el
  receptor y el miembro registrados, y se rechaza con la alternativa de comprobar el receptor y
  asignar directamente. Los arreglos de enteros, de strings y los campos no owned siguen admitiendo
  la forma condicional.

## Endurecimiento sin entrada desde fuente

Estas funciones no reciben datos que un programa fuente controle. Se corrigen igual porque el
cálculo era incorrecto, y la prueba unitaria `nsc.compiler.length-prefixed-decoders` las ejercita.

- **LKF-399.** `tupleElementTypeKeys` comparaba `cursor + length` con el tamaño. Con una longitud
  cercana a SIZE_MAX la suma desbordaba. Con `2^64 - 22` el cursor desbordado volvía a la primera
  coma y, según el código, el lector anterior volvía a leer el segundo elemento sin fin (no se
  ejecutó con el compilador anterior). Ahora se compara con lo restante.
- **LKF-413.** `consumeLengthPrefixed` (identidades de tipos función) tenía la misma suma. La
  comprobación posterior del separador ya rechazaba la entrada, así que la prueba fija la frontera.
- **LKF-423.** `decodeEscapeSummary` calculaba `nameSize + 1`, que daba cero con SIZE_MAX, y leía
  fuera del final. Ahora se compara `nameSize` con el tamaño.
- **LKF-412.** `validateSimd` multiplicaba `laneBits * laneCount`. Con 2^61 lanes el producto daba
  cero y se aceptaba el contrato, también desde `parsePortableSimdType`. Ahora se divide.
- **LKF-400.** El lector de snapshots convertía bytes a enumeraciones sin validar el dominio. Una
  clase de escape fuera de rango se lee como `Unknown`, la más conservadora. Un modo de paso o una
  propiedad fuera de rango rechazan el snapshot, que vuelve a la fuente (regla de respaldo 0.4).
  `nsc.precompiled-*` y `nsc.language-service.snapshots` aprueban.
- **LKF-401.** `HirVerifier::verifyCallNode` desreferenciaba el tipo del símbolo sin comprobarlo.
  Ahora un símbolo sin tipo produce el mensaje del verificador.
- **LKF-402.** En el lowering HIR general, un `else` que fallaba no hacía fallar el `if`. Ahora sí.

## Fronteras verificadas

- **LKF-396 / LKF-397.** Un resultado `some IValue` ejecuta, también con un trait homónimo en otro
  namespace.
- **LKF-398.** Un indexador no se puede llamar con argumentos `ref` (`g[ref x, ref y]` no se parsea),
  así que la ruta de varios argumentos ref no es alcanzable.
- **LKF-422.** `moveWholeValue` es un contrato de biblioteca que no usa el compilador. El análisis de
  cuerpos rechaza mover entero un valor tras mover uno de sus campos ("cannot move complete value
  's' after one of its fields was moved").
- **LKF-434 / LKF-435.** Un enum `long` con valor de 5000000000 y uno `byte` con valor 200 conservan
  ancho y signo. `p - 0` sobre un puntero ejecuta.
- **LKF-441.** `Dispose` no puede declarar `throws` (contrato de `IDisposable`), así que el orden
  entre `Dispose` y la liberación ante una excepción de `Dispose` no es alcanzable.
- **LKF-465.** La división decimal sigue el contrato histórico (`decimal-division-wide-sign.md`):
  escala objetivo `max(sa, sb) + (sa > sb ? 1 : 0)` y redondeo half-up. Por eso `10m / 4m` da `3`, no
  `2.5`. La igualdad y el orden entre escalas distintas, y el resto, son exactos. Si se quiere la
  división de C# (hasta 28 dígitos), es un cambio de contrato y queda para decisión.
- **LKF-480.** La división entera da la vuelta por defecto: `MIN / -1` vale `MIN`, también en i128,
  y `MIN % -1` vale 0.

## Prueba

`nsc.compiler.lowering-and-dispatch-boundaries`: 38 casos, O0/O2, cada rechazo con su control. Las
sondas con el compilador anterior daban esto:

- LKF-391 y LKF-392 se rechazaban.
- LKF-442 y NSC-NEW-028 desbordaban la pila.
- LKF-444 perdía el primer objeto: un destructor de dos.

`nsc.compiler.length-prefixed-decoders` cubre LKF-399, 412, 413 y 423.
