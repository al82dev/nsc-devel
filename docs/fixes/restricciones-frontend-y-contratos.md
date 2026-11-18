# Restricciones del front-end y contratos del modelo

Fichas LKF-005 a 008, 015, 016, 026, 072, 154 a 170, 502, 503, 509, 510, 512, 516, 528, 533, 536,
538, 539, 543, 544, 546, 560, 567, 582 a 584 y NSC-NEW-011. Las fichas paraguas (005 a 026) piden
corroborar el comportamiento antes de atribuir un defecto, y las 154 a 170 registran límites
declarados del modelo. Cada contrato se contrastó con un programa que se compila y ejecuta en O0 y
O2 o que se rechaza con el diagnóstico exacto, con su control.

## Defectos corregidos

- **LKF-528.** La desestructuración de tuplas validaba sus descriptores en cada fase y, ante uno
  malformado, empujaba un fallo y seguía: la fase siguiente leía el primer hijo de ese descriptor y
  la pila de la máquina de expresiones quedaba con valores de más. Ahora se validan una vez, antes
  de cualquier operando, y uno malformado termina el frame con un único fallo. No hay entrada desde
  fuente; el productor MIR siempre crea descriptores bien formados.
- **LKF-510.** El drenaje del coordinador de continuaciones del parser solo tenía un `assert` de
  lista no vacía. En release, una lista vacía con el resultado sin publicar leía `work_.back()` de un
  deque vacío. Ningún handler deja un resultado sin publicar (los 75 `WorkKind` tienen su caso),
  pero ahora el drenaje se detiene y la ranura conserva su resultado vacío.
- **LKF-502.** La identidad de caché de una función MIR no escribía la forma del frame de corrutina
  (`asyncFrameSize`, `asyncFrameLocals`, `asyncFrameParameters`, `isGeneratorConstructor`) ni el
  desasignador de un nodo con asignador propio (`allocatorDeallocateMemberOrdinal`). Todo deriva del
  cuerpo y los layouts, que ya forman parte de la identidad, pero un cambio de la derivación (como
  la ranura de `this` de NSC-NEW-029) habría reutilizado código hecho para otro frame. Ahora se
  escriben.
- **LKF-166.** El comentario de `ExpressionResult.h` afirmaba que un parámetro sin calificador
  acepta un `new`. Desde C-01 (`e64631180`) eso se rechaza con LostOwnership. El comentario se
  conserva como historia y lleva la corrección.
- **NSC-NEW-011.** `nsc.sdk.shared-static-project-reference`, `shared-public-counter-limits`,
  `shared-public-counter-race` y `shared_weakcount_during_view` copian una parte del SDK a un
  proyecto. Desde `0f25b9021` `Arc` y `WeakArc` restringen su carga con `Send` y `Sync`, y la copia no
  los incluía ("cannot resolve a unique trait identity for conditional thread-safety coherence").
  Las cuatro copian ahora `Send.ns` y `Sync.ns`.

## Contratos verificados

- **LKF-005.** Un struct con campo `owned` no se copia ("use move(source)") y se mueve; un elemento
  owned se extrae con `move` y se reemplaza liberando el anterior; una propiedad owned se reemplaza
  liberando la anterior. Un alias simple del getter de una propiedad owned es un préstamo sin vida
  comprobada (LKF-155): dentro del scope del dueño es válido; si sobrevive al dueño queda colgante.
  Comprobarlo exige análisis de vida de préstamos y queda como decisión de diseño.
- **LKF-006.** Un `ref` a un local no se devuelve, `delete[]` con una referencia viva a un elemento
  se rechaza, una lambda no captura un dueño RAII sin `move` (LKF-087, LKF-325, LKF-332, LKF-338).
- **LKF-007.** La limpieza corre en `break`, `continue`, `return` y al lanzar, junto con `finally`;
  un `foreach` que sale con `break` ejecuta el `finally` del generador. La destrucción por base o
  interfaz usa el descriptor dinámico (`nsc.stage35.recovery`, `nsc.stage36.recovery`) y transferir
  un dueño exige un parámetro `owned`.
- **LKF-008.** La aritmética entera da la vuelta, `checked` atrapa (147), la división por cero
  atrapa (129), un double fuera del rango de `int` atrapa (133) y los operandos de un operador de
  usuario se evalúan de izquierda a derecha.
- **LKF-015.** Un struct con cola flexible pasado por valor copia el prefijo, como en C. Un struct
  cuyo único miembro es la cola se acepta, como la extensión de GCC y MSVC.
- **LKF-016.** Un índice negativo de cadena atrapa (132), `Length` de un string nulo atrapa (135),
  un string `owned` local se libera, un NUL interior cuenta en la longitud y la longitud de UTF-8
  cuenta bytes.
- **LKF-026.** `Environment.Exit` termina el proceso sin limpieza de scope; un destructor no puede
  lanzar (LKF-452). El hilo y el export C son interoperabilidad o runtime de hilos y quedan fuera.
- **LKF-072 / LKF-158.** Un arreglo fijo (`int[4]`) y un puntero crudo pertenecen al nivel raw: sin
  comprobación dinámica de límites ni de nulidad, por diseño (`IndexAndRangeExpressions.cpp`). Los
  índices constantes se comprueban en compilación y el arreglo nativo (`int[]`) se comprueba en
  ejecución. Añadir comprobaciones dinámicas al arreglo fijo y al VLA es un cambio de contrato y
  queda para decisión.
- **LKF-154.** Al llegar a 100 errores, el compilador informa "diagnostic limit reached after 100
  errors; 50 additional diagnostics suppressed".
- **LKF-155, 156, 159 a 165, 167, 168, 171 a 175.** Son límites declarados en cabeceras del modelo
  (préstamos sin vida, orígenes aproximados de punteros, identidades abreviadas, catálogos solo de
  compilación). No cambian la ejecución; se confirman los observables: LKF-168 rechaza el productor
  conocido que suspende y entra en pánico con uno dinámico (LKF-483).
- **LKF-169.** Los campos de un agregado empiezan en cero; un local escalar exige asignación.
- **LKF-170.** Una asignación compuesta a una propiedad evalúa receptor e índices dos veces (getter
  y setter), así que exige un local, un parámetro, `this` o una constante; el mensaje propone el
  local.
- **LKF-503.** `loadFile` rechaza fuentes de más de 64 MiB; `addBuffer` no tiene ese límite. El
  registro de acceso de `getFile` es `noexcept`: un agotamiento de memoria termina el proceso.
- **LKF-509.** La fase de argumentos es `size_t`: llamadas estáticas, virtuales y constructores con
  300 argumentos compilan y ejecutan.
- **LKF-512.** `MirBuilder` pertenece a la ruta procedural MIR/SSA, que el driver no usa.
- **LKF-516.** 300 argumentos genéricos anidados y un inicializador de agregados anidado 300 veces
  compilan y ejecutan (con la pila de 128 MiB de LKF-457).
- **LKF-533.** La captura `..var rest` de un list pattern se construye con `Slice(prefijo, Length -
  (prefijo + sufijo))` solo después de la comprobación de longitud del patrón, así que el conteo nunca
  es negativo ni excede. Un arreglo corto no entra al brazo.
- **LKF-536.** En NSCOST 1.0, `arena-sites` cuenta las construcciones en almacenamiento existente
  (`PlacementConstruct`), sea pila, arena o estático; no son reservas de heap. La prueba
  `NativeSharpP08P10ExplainTests` fija esa categoría.
- **LKF-538, 539, 543, 544, 546, 560, 567.** Restricciones con diagnóstico y alternativa: `static`
  en namespace (usar estado de namespace), offset explícito mayor que 16 MiB, carga `borrowed` de un
  variant, `pack` en un variant, método genérico de trait (mover el parámetro al trait), parámetro
  promovido `ref`/`out`/`in`/`params`, y formatos de interpolación en ejecución (D/X con ancho, F con
  precisión, G sin precisión; ancho hasta 64).
- **LKF-582.** Una cadena interpolada rechaza un byte de control y una secuencia UTF-8 inválida, como
  una cadena ordinaria.
- **LKF-583.** Los identificadores son ASCII (`[A-Za-z_][A-Za-z0-9_]*`); una letra no ASCII se rechaza.
  Admitir identificadores Unicode es un cambio de lenguaje y queda para decisión.
- **LKF-584.** Una unión formada solo por bitfields tiene alineación 1, como el oráculo MSVC de
  `nsc.item328.msvc-bitfield-layout`.

## Pruebas

- `nsc.compiler.frontend-restrictions-boundaries`: 31 casos (509, 516, 533, 538 a 546, 560, 567,
  582, 583).
- `nsc.compiler.ownership-control-contracts`: 34 casos (005 a 026, 154, 166, 169, 170).
- `nsc.sdk.shared-static-project-reference`, `shared-public-counter-limits`,
  `shared-public-counter-race`, `shared_weakcount_during_view` (NSC-NEW-011).

`nsc.sdk.shared-public-pointers` (NSC-NEW-013) y `nsc.stage56.c1-interop-completion`
(NSC-NEW-027, interoperabilidad) siguen fallando por causas previas e independientes.
