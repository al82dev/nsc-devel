# Delegados, punteros a marco y delete (LKF-003, LKF-005, LKF-017, LKF-020, LKF-021)

Corrí las 891 sondas y casos negativos del corpus de documentación con el compilador vigente, sobre una
copia en `A:/nsc-fixes-evidencia/batch/probes`. Los resultados están en `probe-results.jsonl` y
`accepted-full.jsonl`. Este cambio corrige los defectos de seguridad que se reproducían sin ambigüedad.

## Defectos y causas

| Sonda | Antes | Causa |
|---|---|---|
| `MovedDelegateRuntimeFailure` | `0xC0000374` (heap corrupto) en O0/O2 | `move` de un delegado `raii` no vaciaba la fuente; la celda se liberaba dos veces. `clearSourceStorage` usaba `isReferenceTypeKey`, que no incluye delegados. |
| `PassModeMismatchAccepted` | `Mutator(ref int)` aceptaba `Read(in int)` | `ref`, `out` e `in` comparten el tipo puntero; la compatibilidad de grupo de métodos no comparaba la palabra clave. |
| `OwnedDelegateCopy`, `CopyArrayOwner` | se aceptaba copiar un delegado o arreglo `raii` a un local simple | la regla de Stage 49 solo miraba referencias de clase y `string`. |
| `ReturnLocalPointerAccepted`, `ReturnStackallocAccepted`, `ReturnPointerAccepted` | devolver `&local` o `stackalloc` se aceptaba | no había regla para punteros, solo para VLA y referencias. |
| `DeleteStackallocAccepted` | `0xC0000374` | `delete` de memoria de marco. |
| `DeleteBorrowedAccepted` | aceptado | `delete` de un parámetro `borrowed`. |
| `ScalarDeleteDynamicArrayAccepted` | aceptado | `delete` escalar de `T[]`. |

## Corrección

- **Movimiento de delegados.** `move` vacía la fuente para todo tipo propietario (`typeKeyIsOwnable`:
  clases, `string` y delegados no `extern C`). Aplica a locales, parámetros, elementos y campos.
- **Compatibilidad de delegados.** Un grupo de métodos se liga a un delegado nombrado solo si cada
  parámetro tiene la misma palabra `ref`, `out` o `in`, que se lee de la marca `pass:` de la
  declaración del delegado.
- **Copias de dueños.** Un delegado o arreglo dinámico `raii` no se copia a un local no dueño; se
  presta con `borrowed` o se transfiere con `move`.
- **Punteros a marco.** `pointerExpressionAddressesCurrentFrame` reconoce:
  - `stackalloc`;
  - `&local`, `&parámetro_por_valor` y `&arreglo_fijo[i]`;
  - aritmética `+`/`-` o un condicional sobre esos valores;
  - un local puntero cuyo inicializador es uno de ellos.

  No sigue un cast desde un operando no puntero, así que `(byte*)direccion` construido desde una
  dirección de montón no se confunde con memoria de marco. Devolver una de esas direcciones se rechaza
  ("a pointer to current-frame storage cannot be returned…").
- **`delete`.** Se rechaza:
  - un local o parámetro `borrowed` o `in`;
  - una dirección de marco;
  - `delete` escalar de un arreglo dinámico.
- **Ajuste de NEW-012.** El campo de valor por defecto no se marca vivo en un constructor que delega
  con `this(...)`. El constructor delegado puede moverlo y los campos de valor no llevan estado de
  movido en tiempo de ejecución. Sin este ajuste, `nsc.compiler.delegated-value-field-proven` lo
  liberaba dos veces.

## Pruebas

`nsc.compiler.frame-pointer-delete-delegate-rules` tiene 30 casos. Cada rechazo tiene un control
aceptado que difiere solo en la propiedad comprobada, y los casos aceptados salen 42 en O0 y O2.

Regresión dirigida, sobre 127 pruebas de arreglos, delegados, lambdas, eventos, movimiento, RAII y cc1,
más los binarios en proceso reconstruidos: quedan 3 fallos, todos registrados antes de este cambio.

- `t32.range-arrays` (LKF-095);
- `stage49.raii-move` (NSC-NEW-014);
- `shared-public-pointers`, con su primer fallo conocido de NSC-NEW-013.

`delegated-value-field-proven` pasa.

## Fuera de este cambio (contrato pendiente)

- Índices dinámicos fuera de rango en arreglos fijos inline (`RuntimeUpperBound`,
  `RuntimeSecondDimension`) no se comprueban. El comentario de `IndexAndRangeExpressions.cpp` los
  define como nivel crudo.
- Desreferencia de puntero nulo, cast desalineado, `LoadAligned` desalineado, `delete` de memoria de
  arena y `delete`/`destroy` de objetos construidos con placement. Son operaciones crudas cuyo
  contrato no está decidido.

## Límites de arreglos, VLA y stackalloc (LKF-004, LKF-012, LKF-022)

El corpus confirma que, en esta revisión, las protecciones dinámicas existen:

- **Arreglos dinámicos.** Atrapan conteo negativo, desborde del producto de dimensiones y conteo
  `long` fuera de rango (134). También atrapan índices negativos o superiores en cada dimensión (132)
  y la longitud de `null` (135).
- **Arreglos fijos.** Un índice constante fuera de rango se rechaza ("constant fixed-array index is
  outside [0, 4)"). Un índice en tiempo de ejecución es nivel crudo por diseño.
- **VLA y `stackalloc`.** Atrapan extensión negativa, por encima de la reserva de pila y desborde de
  producto (134). Un `stackalloc` constante demasiado grande se rechaza al compilar.

Defecto encontrado: la memoria de VLA y de `stackalloc` no se inicializaba. Tras una llamada que deja
datos en la misma zona de pila, `int[count] values; values[0]` devolvía basura en O0 y 0 en O2
(sonda `UninitializedReadAccepted`). `stackAllocationFromParts` ahora pone a cero la reserva, como ya
ocurre con un arreglo fijo local y con un arreglo dinámico.

Prueba `nsc.compiler.array-and-stack-limits`: 26 programas con su código esperado en O0/O2, cada trampa
con un control dentro de rango; incluye la invocación de delegado nulo (135) y su control.
