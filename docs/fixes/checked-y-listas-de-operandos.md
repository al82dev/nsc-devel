# `checked(...)` en todos los enteros y listas largas de operandos

Fichas LKF-418, 419, 420, 421, 453, 454, 463 y 509; NSC-NEW-021 y NSC-NEW-022.

## Defectos corregidos

- **NSC-NEW-021 (LKF-420).** `checked(...)` solo atrapaba el desbordamiento en `i128`. Para `int`,
  `long`, `uint`, `ulong` y los demás enteros incorporados, `checked(int.MaxValue + 1)`,
  `checked(ulong.MaxValue * 2)` y `checked(-int.MinValue)` daban la vuelta en silencio, y las
  conversiones `checked((byte)300)`, `checked((int)4294967296L)` o `checked((uint)-1)` solo se
  comprobaban con origen `i128`/`u128`. `applyIntegerBinaryOperation` usa ahora
  `llvm.{s,u}{add,sub,mul}.with.overflow` para todo entero de ancho fijo, la negación checked vale para
  todo entero con signo, y la conversión checked comprueba cualquier origen entero, también el
  ensanche de con signo a sin signo. La política predeterminada sigue envolviendo.
- **NSC-NEW-022 (LKF-509).** `1fd4d284e` ensanchó a `size_t` el índice de operandos de la llamada
  estática directa, pero diez marcos del autómata de expresiones lo seguían truncando a ocho bits
  (`static_cast<std::uint8_t>(index + 1)`): construcción de struct y de clase, construcción con
  allocator, inicialización de agregados, stackalloc, deconstrucción de tuplas, packs de parámetros y
  tres llamadas núcleo. Con 256 argumentos el índice volvía a cero y la compilación no terminaba
  (construcción de clase: más de 300 s). Los diez usan ya el índice completo.

## Verificado sin cambio

- **LKF-418, LKF-419, LKF-421.** `computeGeneratorFrame`, `evaluateDecimalArithmetic` y
  `ConstantMemory` (`src/Language`) no tienen llamadores en el compilador: solo los usan sus pruebas
  unitarias (`nsc.ns3.t09`, `nsc.ns3.t14.*`, que pasan). Las correcciones `40c706631`, `b6d3645fd`,
  `488cf4eb1` y `d3b45adce` no son alcanzables desde fuente; el plegado real de constantes es el
  evaluador Stage37 y envuelve igual que la ejecución (`const int B = A * 2` da -2).
- **LKF-453 / LKF-454.** `decimal` máximo y mínimo positivo convierten a `double` dentro del margen;
  `0.125` convierte exacto a `decimal`; `1e30` y NaN atrapan con 145 (`DecimalOverflow`).
- **LKF-463.** Un literal decimal de 96 bits (`79228162514264337593543950335m`) inicializa un campo
  estático y compara bien. La aritmética decimal en un campo `const` (`A * 2m`) se rechaza con "const
  field initializer must be a compile-time constant expression": el evaluador Stage37 no tiene tipo
  decimal. Un `const` local con aritmética decimal y un campo estático ejecutan.

## Pruebas

`nsc.compiler.checked-integer-overflow` (19 casos, O0/O2; con el compilador anterior fallaban 13) y
`nsc.compiler.large-argument-lists` (6 formas con 255, 256 y 300 operandos, O0/O2).
