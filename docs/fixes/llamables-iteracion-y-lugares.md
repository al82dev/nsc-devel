# Llamables, iteración, lugares y retornos en el borde de la superficie

Fichas LKF-286, 290, 301, 310, 314, 320, 324, 325, 327 a 332, 337 a 345. Cada ficha se contrastó
con programas que se compilan y ejecutan en O0 y O2. Las que mostraron un defecto se corrigieron y
las demás quedan como fronteras verificadas con su control.

## Defectos corregidos

- **LKF-286.** Un tipo función (`fn*`, `fn&` o el tipo de una lambda inferida) no declara
  propiedad, así que presta todos sus parámetros y no devuelve dueño. Había cuatro huecos:
  - Un `new` pasado a uno de sus parámetros no llegaba a ningún dueño y se perdía. La comprobación
    existente solo veía delegates declarados. Ahora se rechaza con el mismo diagnóstico de pérdida de
    propiedad.
  - Un grupo de métodos se enlazaba sin comparar la propiedad. `fn*(Payload) -> int f = Take;`, con
    `Take(owned Payload p)`, liberaba otra vez el objeto del llamador y el proceso terminaba con
    corrupción de heap (0xC0000374). Un método con retorno `owned` enlazado a un destino sin dueño
    perdía el objeto. Lo mismo pasaba con un delegate declarado cuyo parámetro o retorno no
    coincidía. Ahora el enlace exige que cada parámetro y el retorno coincidan en propiedad, y el
    mensaje nombra el parámetro y el destino.
  - `owned` escrito en un parámetro o en el resultado de un tipo función se descartaba en silencio.
    Ahora se rechaza y se propone un delegate.
  - Un delegate declarado con retorno `owned` no entregaba el resultado al llamador:
    `raii Payload p = f();` se rechazaba. Ahora el resultado es una transferencia owned. Con un raii
    o descartado, el destructor corre una vez.
- **LKF-290.** El recorrido Send/Sync de las capturas `ref` de un hijo de un executor de hilos no
  encontraba campos en un delegate ni en una lambda, y los daba por Send. Su entorno capturado no
  forma parte del tipo, así que ahora no son demostrablemente Send ni Sync. Un puntero a función
  (`fn*`) y un delegate `extern C`, que no llevan entorno, siguen siendo Send. Arreglos fijos,
  genéricos y structs con punteros ya se rechazaban.
- **LKF-310.** `move(it.Current)` marcaba el elemento como movido y la marca no se quitaba nunca,
  ni al avanzar con `MoveNext`. Mover el siguiente elemento tras un `MoveNext` se rechazaba ("cannot
  move from the iterator item twice"). La marca se quita ahora cuando `MoveNext` o `MoveNextAsync` se
  llama en el scope del movimiento o en uno que lo contiene. Un `MoveNext` dentro de una rama que no
  contiene al movimiento conserva la marca, porque esa rama puede no ejecutarse. Las marcas se
  reinician además al empezar cada callable. El resultado de la expresión, que quedaba marcado como
  inválido, ahora es válido.
- **LKF-314.** Un handler de interpolación llamaba al primer `AppendFormatted` declarado para todos
  los huecos. Con `AppendFormatted(bool)` antes de `AppendFormatted(int)`, un hueco int llegaba al
  parámetro bool. Tampoco se comprobaban `AppendLiteral` ni `Finish`. Ahora cada hueco elige la
  sobrecarga de un parámetro cuyo tipo es exactamente el suyo. Un hueco sin sobrecarga se rechaza con
  su tipo. `AppendLiteral` debe tomar un string y `Finish` no debe tomar parámetros y debe devolver
  el tipo del handler. Frontera: el parser convierte una interpolación con huecos de ejecución en
  `String.Concat`. El handler solo recibe interpolaciones cuyos huecos son constantes, así que el
  protocolo completo con huecos de ejecución sigue pendiente junto con LKF-567.
- **LKF-324.** Desde un puntero one-past-end, `end - 1` conservaba la marca de centinela y no podía
  desreferenciarse, aunque vuelve al último elemento. La marca queda ahora solo si el desplazamiento
  es una constante que no retrocede (cero o hacia delante). Un desplazamiento hacia atrás o no
  constante la quita, como en cualquier puntero crudo cuya posición no se sigue. `*end` y
  `*(end + 0)` siguen rechazados.
- **LKF-325.** `int[] b = a;` comparte el almacenamiento de `a`, pero solo las asignaciones
  registraban el alias. Con una declaración, `delete[] b` mientras vivía `ref int x = ref a[0]` se
  aceptaba y `x` leía memoria liberada. Un inicializador de arreglo nativo que nombra un local o un
  parámetro, también en un condicional, lo registra ahora como posible origen. La comprobación del
  `delete[]` recorre esos alias en ambos sentidos.
- **LKF-337.** Una referencia devuelta por un método `ref` de un objeto, o tomada de un elemento de
  su campo arreglo, vale mientras vive el búfer de ese campo. `delete[] b.Data` con esa referencia
  leída después se aceptaba. Ahora se rechaza si la referencia se lee después del `delete[]`. Si
  su último uso es anterior, se acepta.
- **LKF-342.** La entrada de una capacidad comptime (`file_i64`, `env_i64`) se leía completa y se
  comparaba después con el presupuesto de memoria. Ahora se rechaza antes de copiarla: el tamaño del
  archivo se consulta primero y la lectura se corta un byte después del límite. El diagnóstico es el
  mismo que antes ("comptime memory budget exceeded while reading declared input"). Lo que cambia es
  la memoria que se usa antes de darlo, así que la prueba fija la frontera y no puede distinguir el
  antes del después.

## Fronteras verificadas

- **LKF-301.** `volatile` es un modificador de local o de campo, no un calificador de tipo. Un
  puntero a volatile no se puede escribir en fuente NativeSharp, así que no llega a arreglos ni
  tuplas. Los rechazos que describía la ficha ya no existen en el código. Un local `volatile int*`
  ejecuta. Los punteros a volatile importados de C son interoperabilidad y quedan fuera.
- **LKF-320.** La marca `active-member-safe` no tiene productor: ningún parser ni modelo la fija.
  Las uniones NativeSharp siguen el contrato de C y permiten leer por otro miembro
  (`nsc.ns3.t03.c-unions`). Escribir `Signed = -1` y leer `Unsigned` da `4294967295`.
- **LKF-327.** Una guarda `when Check(v)` se evalúa una vez y su efecto se conserva en el camino
  restante.
- **LKF-328.** `using` exige una clase que implemente `NativeSharp.Core.IDisposable`. Un struct no
  puede implementar interfaces ordinarias (solo traits), así que no hay un `using` de struct que el
  lenguaje acepte. `Dispose` corre una vez, también cuando el cuerpo lanza.
- **LKF-329.** Las funciones locales, también recursivas, ejecutan. El diagnóstico de
  `FunctionDeclaration` en `analyzeStandaloneStatement` no se alcanza desde fuente.
- **LKF-330.** `throw e` de un local con nombre se rechaza con la alternativa (`move(e)`, un objeto
  nuevo o la excepción capturada), y `throw move(e)` ejecuta.
- **LKF-331.** Un dueño movido antes o después de una llamada que lanza dentro de un `try` se libera
  una vez.
- **LKF-332.** Un VLA (`int[n] a;`) ejecuta, también de strings. Se rechazan el inicializador, un
  elemento con destructor o campos owned, una dimensión bool y un puntero al VLA guardado fuera del
  marco.
- **LKF-338.** El retorno `ref` de un estático y el de un parámetro con lifetime nombrado ejecutan.
  El de un local se rechaza.
- **LKF-339.** El respaldo `BorrowedReturnForward` solo se alcanza con valores prestados: llamadas
  que devuelven prestado y condicionales de parámetros prestados. Un retorno prestado de una llamada
  sobre un dueño local, de un condicional con un local y de un campo owned se rechaza. El caso del
  campo owned exige un contrato de lifetime de campo, que el lenguaje no tiene.
- **LKF-340 / LKF-341.** Las constantes de 128 bits (desplazamiento de `(i128)1 << 100` y producto
  fuera de 64 bits) y el cast constante `(int)42.9` se pliegan bien. `1 << 100` sin el cast desplaza
  un int y atrapa en ejecución (131), igual que cualquier desplazamiento int fuera de rango.
- **LKF-343.** Un struct cuyos constructores son todos privados admite `new S()` por defecto.
- **LKF-344.** Una restricción incumplida en la base (`Child : Parent<int>`, `where T : class`) se
  rechaza antes de seleccionar el inicializador. La restricción cumplida ejecuta.
- **LKF-345.** Dos ciclos `this(...)` independientes dan dos diagnósticos, uno por ciclo.

## Pruebas desactualizadas corregidas

Al ejecutar `nsc.stage45.*` aparecieron cuatro pruebas con expectativas anteriores a cambios ya
integrados en la rama:

- `nsc.stage45.exception-sema` (`BareTry`): desde `519638b47` el parser rechaza un `try` sin
  handler con "expected 'catch' or 'finally' after try block" antes del análisis semántico.
- `nsc.stage45.llvm-win64-eh-basic`: desde `8b128ac6b` el constructor implícito de la clase restaura
  el descriptor base en un funclet de cleanup. La comprobación de que no hay cleanup se limita ahora
  a la función con el catch-all.
- `nsc.stage45.typed-catch-rtti`: desde `d00b3a869` el `finally` también corre en funclets catch
  (`eh.finally.pad`). Solo se cuenta el pad de las cláusulas catch.
- `nsc.stage45-11.eh-cleanup-execution`: desde `8d46bf5cc` (C-13) un método que llama a otro que
  declara `throws Boom` debe propagarlo o capturarlo. Dos programas de la prueba no lo declaraban.

## Prueba

`nsc.compiler.callable-iteration-and-place-boundaries`: 70 casos, cada rechazo con su control,
O0/O2. Antes de los cambios, los casos de LKF-286 terminaban con corrupción de heap o con
destructores que no corrían. El de LKF-290 se aceptaba. Los de LKF-310 (MoveNext que avanza),
LKF-325 y LKF-337 se rechazaban o se aceptaban al revés de lo esperado; en los de LKF-325 y LKF-337,
el programa aceptado leía memoria liberada. Para LKF-314 y LKF-324 el comportamiento anterior se
deduce del código: HIR usaba siempre la primera sobrecarga y la aritmética copiaba la marca sin
condición. `nsc.ns3.t11.f05.comptime-capabilities` añade los tres casos de LKF-342.
