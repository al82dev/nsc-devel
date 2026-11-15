# Asignaciones y llamadas en el borde de la superficie

Fichas LKF-277 a LKF-299, LKF-302 a LKF-306, LKF-308, 309, 311, 312, 313, 316, 321, 323, 326 y 336, y
NSC-NEW-018. Incluye tres regresiones encontradas al ejecutar las pruebas y varias pruebas
desactualizadas corregidas.

## Defectos corregidos

- **LKF-277.** `a[i] ??= "texto"` sobre un `string[]` plano guardaba el literal sin marcar el slot
  como prestado. El `delete[] a` posterior liberaba el literal y el proceso terminaba con corrupción
  de heap (0xC0000374). La asignación simple ya marcaba ese caso, y ahora `??=` también lo hace. Sigue
  rechazado `??=` sobre una propiedad o un indexador, que exigiría capturar una sola vez el receptor
  para el getter y el setter. El mensaje propone guardar antes el valor en un local.
- **LKF-280.** `p?.Campo = valor` no comprobaba la conversión implícita. Un literal de cadena o un
  `long` que estrecha llegaban a un campo `int`, y el programa devolvía basura. La escritura
  condicional a un miembro pasa ahora por la misma comprobación que `p?[i] = valor`.
- **LKF-281.** `resolveTypeIdentifier` alternaba el candidato de respaldo. Con tres tipos homónimos en
  namespaces no importados, el tercero volvía a ser el elegido y `T.V()` devolvía 3. Ahora la
  ambigüedad se conserva. Con un único candidato no importado la búsqueda sigue resolviéndolo, que
  es el comportamiento actual del lenguaje.
- **LKF-283.** Las comprobaciones de orden de memoria y el lowering de las operaciones atómicas leen
  los argumentos por posición. Con argumentos con nombre, un `CompareExchange` válido se rechazaba
  ("failure order cannot be stronger") y un `Store` con nombre también. Las operaciones atómicas
  rechazan ahora los argumentos con nombre con el mensaje "atomic operations take positional
  arguments; write the arguments in parameter order without names".
- **LKF-291.** El diagnóstico de `p?.M()` con resultado de valor recomienda `Optional<T>`, pero un
  método que devolvía `Optional<T>` también se rechazaba. Ahora se admite ese carrier. Si la llamada
  se omite, el resultado es el carrier en ceros, que es el `Optional` vacío.
- **NSC-NEW-018.** Al pasar un arreglo a una llamada, todos los locales arreglo quedaban marcados como
  posibles alias de almacenamiento desconocido antes de las comprobaciones de vistas de esa misma
  llamada. Un arreglo recién creado que se pasaba por primera vez chocaba así con una vista derivada
  de raíz desconocida (`t.Slice(2)` o `t.AsReadOnly()` sobre el resultado de `Spans.OfRange`). Es la
  falla `P4IntSpanUnchanged` de `nsc.freeze30-contracts`. Ahora las marcas que fija la lista de
  argumentos de una llamada no cuentan para las comprobaciones de esa misma llamada. Además, una
  vista de raíz desconocida ya no solapa un arreglo nativo creado por su propio inicializador que
  nunca se nombró en una llamada ni tiene alias.
- **LKF-289.** El seguimiento de préstamos de `TaskScope` tenía tres huecos. Un `Join` limpiaba todos
  los préstamos, fuera del scope que fuera, e incluso el de cualquier método llamado `Join`. Además,
  `Spawn` solo veía una llamada hija directa y emparejaba sus argumentos por posición. Con cualquiera
  de esas formas, un hijo que tomaba `ref x` sobrevivía a `x` sin diagnóstico. Ahora solo participan
  las llamadas de `NativeSharp.Core.TaskScope<T>`, los préstamos se agrupan por el local del scope y
  se emparejan por el binding elegido. También se sigue el `Future` local que se pasa a `Spawn`.
- **LKF-298.** El constructor de colecciones usaba el primer `Add` declarado para todos los elementos.
  Con `Add(float)` antes de `Add(int)`, `[1, 1]` llamaba a `Add(float)`. Ahora es candidato todo `Add`
  de un parámetro que acepte todos los elementos, y gana el que tiene más tipos exactos. Un empate
  entre tipos de parámetro distintos se informa como ambiguo; una colección vacía no es ambigua.
- **LKF-293.** En O0, el backend expandía el `store zeroinitializer` de un agregado local en un store
  por elemento, y `int[100000] a;` hacía caer el compilador (0xC0000005). Los locales agregados se
  ponen en cero ahora con un `memset`. Un local mayor que la pila de 1 MiB desborda en ejecución
  (0xC00000FD), igual que en C. En O2 el optimizador puede eliminarlo.
- **LKF-302.** La comprobación de `stackalloc` y `raw new` miraba solo los campos directos del
  elemento. Un `struct Outer { Inner I; }` con un campo owned o un destructor en `Inner` se aceptaba.
  `elementTypeHasTrivialLifetime` recorre ahora los campos struct.
- **LKF-295 (`nsc.ns3.t03.c-unions`).** La regla de contratos de escape desconocidos de `295f19e94`
  trataba como almacenamiento del marco una unión copy pasada por valor a una función exportada de
  una DLL NativeSharp, y rechazaba `Api.Read(value)`. Un argumento por valor de un tipo sin
  direcciones (escalares, enums y structs o uniones formados solo por ellos) no puede retener el
  marco, así que ya no se comprueba.
- **LKF-312.** Un condicional entre una clase y su base se rechazaba: `c ? (B)new D1() : new D2()`
  daba "incompatible types 'B' and 'D2'". Si una rama de clase convierte implícitamente a la otra
  (sin conversión de usuario), el resultado es la otra, sin conversión de representación. No se
  busca un ancestro común: `c ? new D1() : new D2()` sigue pidiendo escribir una rama como la base.
- **LKF-313.** `wrapping(...)` y `unchecked(...)` se rechazaban para cualquier tipo, y antes de tipar
  el operando, con un mensaje que hablaba solo de decimal. La aritmética entera ya da la vuelta por
  defecto, así que ambas políticas se aceptan. Se rechazan cuando hay aritmética decimal en el
  operando, también dentro de una conversión.
- **LKF-316.** El binder marcaba asignado al entrar el parámetro `out` de una lambda. Un cuerpo que no
  lo escribía, que lo escribía en una sola rama o que salía antes con `return` entregaba al llamador
  un valor sin inicializar. Ahora se comprueba que toda terminación normal lo asigna: un `throw`
  cuenta como asignación, y bucles, `switch` y `try` se toman como capaces de omitir la escritura.
- **LKF-321.** Un local `pin` se rechazaba en cualquier función async con una suspensión, aunque su
  bloque terminara antes. Ahora se rechaza solo si una suspensión sigue a su declaración dentro de su
  bloque, incluidos los bloques anidados.

- **LKF-326.** La propiedad de un `match` se tomaba del primer brazo. Si ese brazo era una llamada
  que solo lanza, o si los brazos mezclaban un `new` con una llamada que devuelve `owned`, el resultado
  perdía el dueño y `raii Payload r = match (k) { 0 => Fail(), _ => new Payload(42) }` se rechazaba
  ("RAII ownership cannot be copied or borrowed"). El brazo de referencia es ahora el primero que
  produce un valor, y unos brazos que entregan todos un dueño nuevo se unen como transferencia owned.
  Si ningún brazo transfiere (todos `new`), la regla anterior no cambia. Se liberan una vez: con el
  `new` elegido el destructor corre una vez y con el brazo que lanza no corre.

## Diagnósticos mejorados

- **LKF-282.** Un dueño en un span `params` expandido producía "no viable overload". Ahora se informa
  que el span expandido solo presta sus elementos durante la llamada, y se proponen dos salidas:
  pasar una referencia no dueña o construir un Span/ReadOnlySpan sobre los dueños.
- **LKF-278.** La deconstrucción de una tupla con elementos move-only se rechaza (tres rutas:
  asignación, declaración y `Deconstruct()`). El mensaje añade ahora que esos elementos no pueden
  separarse de la tupla y que se devuelven por parámetros `out` o se conserva el valor completo.
  El intento de mover un campo de un struct con destructor también se rechaza ("cannot partially
  move").
- **LKF-284.** La captura de resto de un arreglo nativo, de un arreglo fijo o de un string ejecuta con
  NativeSharp.Core. Sin Core, el mensaje indica ahora que hay que referenciarlo, porque Core declara
  las vistas que usa la captura.

- **LKF-336.** Un `return ref` dentro de una lambda se rechaza. El mensaje añade ahora que la lambda
  devuelve por valor y propone declarar un método con retorno `ref`.

## Fronteras verificadas

- **LKF-279.** Reasignar una vista a un dueño de vida más corta, también desde una rama, se rechaza
  ("view borrowed from local 'b' cannot be stored in a longer-lived local"). Con dos dueños vivos, la
  reasignación también se rechaza de forma conservadora ("its lifetime cannot be proven").
- **LKF-287.** Invocar dos veces un FnOnce a través de un parámetro se rechaza (NS3020). Moverlo a un
  campo o invocar una lambda que captura sin destino también se rechaza con un diagnóstico explícito.
  Las formas locales, condicionales y de alias ya tienen pruebas registradas (`nsc.compiler.fnonce-*`).
- **LKF-285.** `derive(Clone)` rechaza un campo con destructor también dentro de un arreglo fijo
  ("field 'Items[]' has a destructor"). Un `Clone` escrito por el usuario sobre un struct con campo
  owned es válido, y el `Clone` generado de un registro plano ejecuta.
- **LKF-292.** En una asignación compuesta se evalúan una sola vez el índice con efectos y el
  operador de usuario (`b.Data[Next()] += add`).
- **LKF-294.** El move-spread libera cada recurso una vez. Un spread sin `move` de un agregado
  move-only se rechaza con la forma requerida.
- **LKF-296.** `?` propaga el error de un `Result` y, en un método que no devuelve un `Result`
  compatible, se rechaza ("must return a compatible Ok/Error variant").
- **LKF-297.** `b?[i]` sobre un indexador de usuario ejecuta. El mensaje que menciona solo arreglos
  nativos sale únicamente cuando el receptor no es arreglo ni tiene indexador.
- **LKF-299.** El spread de un arreglo nativo en una colección ejecuta. Un elemento de otro tipo se
  rechaza ("must exactly match target element type"), y un arreglo nulo atrapa con
  NullReference (135).
- **LKF-303.** `destroy` sobre un local raii se rechaza ("lexical cleanup owns destruction"), y copiar
  el dueño a un alias también.
- **LKF-304.** Los casts de una referencia a un entero, entre clases sin relación y de `void*` a una
  clase se rechazan.
- **LKF-305 / LKF-306.** El rango implícito sobre un arreglo o un string se rechaza con la
  alternativa Span/StringView. El protocolo `Length`/`Slice` de usuario ejecuta, y en su forma
  virtual se rechaza.
- **LKF-308.** Un local `volatile` de arreglo fijo se rechaza ("volatile local must be a native
  scalar, enum, or raw pointer"). La dirección de un elemento por un `const int*` ejecuta.
- **LKF-309.** Mover un elemento de un arreglo owned, con índice constante o variable, lo libera una
  vez. `delete[]` no vuelve a liberarlo.
- **LKF-311.** `with` sobre un struct trivial ejecuta. Sobre un valor con destructor se rechaza con la
  alternativa de una copia explícita.
- **LKF-323.** La división por un `const` local igual a cero se rechaza al compilar. Un `const` derivado
  de otros `const` locales no se pliega (frontera documentada en `divisorIsConstantZero`), y la
  división atrapa en ejecución con DivideByZero (129).
- **LKF-288.** Con argumentos con nombre fuera de orden, el diagnóstico de propiedad nombra el
  parámetro correcto: "owned parameter 'item' … requires an ownership transfer" en el constructor y
  "use move(p)" en el método. El control con `move` ejecuta.

## Regresiones encontradas al ejecutar las pruebas

- `nsc.item341.exception-safe-com-thunk` y `CallbackExceptionTraps` de `nsc.stage56.c1-interop-completion`: un delegado
  `extern C` acepta un grupo de métodos que lanza, porque el thunk atrapa la excepción
  (`extern-c-trap`). La comparación exacta de firmas exigía también el tipo de error y rechazaba el
  enlace. Para un delegado `extern C` el tipo de error ya no cuenta.
- `nsc.item326.anonymous-member-promotion` (`T[N]` por valor): la regla de contratos de escape desconocidos de LKF-295 trataba
  un arreglo fijo de escalares como si pudiera retener el marco. `valueTypeCarriesNoAddress` acepta
  ahora los arreglos fijos cuyo elemento cumple la misma condición.
- `nsc.integration.shared_binary_pattern_field`: ver `pruebas-de-integracion-registradas.md`.

`TwoLibrariesSameSymbol` (`nsc.stage56.c1-interop-completion`) sigue fallando. Es previo a esta
campaña y queda fuera de alcance porque es interoperabilidad C: desde `7cdabd0cc`, una unidad de
código particionada solo declara las funciones de las que depende, y la unidad dueña de los globales
no declara el import de enlace en tiempo de ejecución que debe definir ("C1.2 runtime-bound import
lost its declaration").

## Pruebas desactualizadas corregidas

- `nsc.ns3.t09.foreach-protocol`: desde `80b186758` (Congelamiento 3.0 P4) el `foreach` sobre un Span
  lee cada elemento con `RefAt`. El Span mínimo de la prueba no lo declaraba y ahora lo declara.
- `nsc.ns3.t11.f07.comptime-budget`: desde `7e270a01f` (NS304-T14-F07) una macro que se expande a sí
  misma se informa como ciclo antes de agotar el presupuesto. La prueba acepta ambos resultados
  acotados, siempre que nombren la macro.
- `nsc.stage56.c7-definite-assignment`: con LKF-293 el local agregado se pone en cero con
  `llvm.memset` y no con un `store zeroinitializer`. La prueba busca el memset.
- `nsc.stage52-5.foreach-semantic-gaps`: el Congelamiento 3.0 (C-11) admite enumeradores struct con
  vida no trivial. El caso que esperaba rechazo espera ahora aceptación.
- `nsc.item320.object-storage-ownership`: el caso de `new` sin dueño pasa ahora el objeto a un
  parámetro `owned` (`Release(new Widget(43))`), porque un `new` que no llega a un dueño se rechaza.
- `nsc.item321.noheap-context`: el elemento 323 promueve a la pila un objeto que no escapa y mide como
  mucho 1 MiB, así que `new` de una clase pequeña ya no es una asignación de heap. El caso usa una clase
  de 2 MiB.
- `nsc.item331.variadic-c-declarations` y `nsc.item344.vectorcall`: comprobaban que un NSMETA de versión anterior rechazaba la
  forma nueva. Desde `2320596aa` NSMETA se lee y escribe por contenido, sin versión. Las pruebas
  comprueban ahora el viaje de ida y vuelta del contenido.
- `nsc.ns3.t33.package-coherence`: desde `16625fd68` NativeSharp.Core.IO referencia Core como
  proyecto, así que su biblioteca incluía una segunda copia de Core y el enlace fallaba con símbolos
  duplicados. La prueba compila Core.IO desde una copia que referencia `NativeSharpCore.lib`.

## Prueba

`nsc.compiler.assignment-and-call-boundaries`: 68 casos, cada rechazo con su control, O0/O2. Antes del
cambio fallaban treinta y tres: treinta por los defectos y tres por el texto nuevo de los
diagnósticos. También
se ejecutaron `nsc.ns3.t01..t03`, `t08`, las pruebas atómicas, `fnonce`, de vistas, de patrones y
`nsc.freeze30-contracts`.
