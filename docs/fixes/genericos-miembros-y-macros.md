# Genéricos, miembros, construcción y macros en el borde de la superficie

Fichas LKF-346, 349, 355, 356, 362, 365, 370, 371, 376, 377, 382 a 388. Cada ficha se contrastó
con programas que se compilan y ejecutan en O0 y O2. Las que mostraron un defecto se corrigieron y
las demás quedan como fronteras verificadas con su control.

## Defectos corregidos

- **LKF-346.** Un constructor escrito sin inicializador llama a `base()` de forma implícita. Si el
  constructor base elegido tenía parámetros con valor predeterminado (`B(int v = 42)`), HIR no
  pasaba esos valores y el verificador de LLVM rechazaba el módulo ("Incorrect number of arguments
  passed to called function"). Ahora se pasan los predeterminados, igual que con `: base()`
  explícito. El constructor implícito de una clase sin constructores sigue exigiendo un constructor
  base sin parámetros, como hacía el enlace. La previsión que usaba otra regla (todos los parámetros
  con valor predeterminado) usa ahora la misma, y el mensaje explica que hay que declarar un
  constructor que llame a `base()`.
- **LKF-355.** El primer parámetro de una extensión es su receptor, que viaja en el callee. El mapeo
  posicional de la inferencia empezaba en ese parámetro, así que `b.Pick(2)` emparejaba el 2 con el
  tipo del receptor y fallaba con "cannot be inferred". Ahora el receptor no participa en el mapeo de
  argumentos y aporta su evidencia también cuando hay argumentos.
- **LKF-371.** La identidad de especialización escribía todo arreglo fijo como `[fixed]`, sin su
  extensión. `Box<int[2]>` y `Box<int[8]>` compartían la misma especialización: `b.V[7]` sobre la
  segunda se rechazaba con "outside [0, 2)" y su tamaño era el de la primera. La extensión literal o
  con nombre forma parte ahora de la identidad (`FixedArraySpelling.h`). Una extensión de otra forma
  conserva `[fixed]`.
- **LKF-382.** El contador de higiene de las macros se comparaba con el límite de profundidad. Con
  33 invocaciones independientes la expansión fallaba con "macro expansion depth limit exceeded",
  aunque ninguna estaba anidada. La profundidad de una invocación es ahora uno más que la de la
  expansión más interna cuyo texto la contiene.
- **LKF-383.** `#macro` y `#expand` se buscaban con una búsqueda de texto simple. `"#expand f(1)"`
  dentro de un literal o en un comentario se tomaba por una directiva ("unknown macro 'f'"). Ahora
  solo cuenta el texto de código: se saltan las cadenas (también verbatim y raw), los caracteres y
  los comentarios.
- **LKF-384.** Un valor predeterminado entero solo se comprobaba como numérico. `byte x = 300` pasaba
  44 y `uint x = -1` pasaba 4294967295. Ahora el valor debe caber en el tipo del parámetro.
- **LKF-388.** Un elemento de tupla guarda una referencia de clase y nunca destruye el objeto.
  `owned` en un tipo de tupla (`(owned P, int)`) se descartaba en silencio, y un `new` o un dueño
  transferido guardado en un elemento se perdía sin destructor. Ahora ambos se rechazan con la
  alternativa: un local raii o un struct con campo owned.

## Diagnósticos mejorados

- **LKF-376.** `string +=` solo se baja sobre un local string raii. Sobre un local string simple el
  mensaje hablaba de operandos numéricos; ahora dice que hace falta un local string raii. Sobre un
  campo owned se rechaza con el mensaje de transferencia de propiedad.
- **LKF-386.** `params` acepta cualquier `ReadOnlySpan<T>` con la forma requerida (datos `T*` y
  longitud `int`), también uno de usuario, porque las pruebas mínimas declaran el suyo. Si la forma
  no se cumple, el error nombra ahora el elemento y la forma esperada. Antes era un mensaje interno.
- **LKF-387.** El rechazo de una propiedad automática `static owned` nombraba el campo de respaldo
  interno (`__nsc_backing_Item$stage38`). Ahora nombra la propiedad.

## Fronteras verificadas

- **LKF-349.** Un ciclo de tipos asociados se diagnostica ("normalization cycle"). Una proyección
  dentro de un argumento de tipo (`Holder<Number.B>`) no se resuelve ("unknown type name"), así que
  el ciclo anidado que describía la ficha no es alcanzable.
- **LKF-356.** Ocho llamadas genéricas anidadas se infieren y ejecutan.
- **LKF-362.** La inferencia a través de un condicional ejecuta. Con dos tipos de argumento distintos
  (`int` y `long`) no se busca un tipo común y se pide escribir los argumentos de tipo.
- **LKF-365.** Un método genérico dentro de un tipo genérico ejecuta.
- **LKF-370.** Dos usos de un genérico con argumentos distintos, en el mismo namespace y en el mismo
  offset de dos archivos, se especializan por separado.
- **LKF-377.** `p?[1, 2]` sobre un arreglo de dos dimensiones no se puede escribir: el parser lo
  rechaza. La ruta HIR que usaba solo el primer índice no es alcanzable desde fuente.
- **LKF-385.** Un `impl` exige un destino sin calificar propio del módulo, así que la selección por
  nombre simple no puede elegir el tipo de otro namespace.

## Prueba

`nsc.compiler.generic-member-and-macro-boundaries`: 40 casos, cada rechazo con su control, O0/O2.
Antes de los cambios, las sondas daban esto: LKF-346 hacía fallar el verificador de LLVM; LKF-355
fallaba la inferencia; LKF-371 rechazaba el índice válido; LKF-382 y LKF-383 fallaban la
expansión; LKF-384 aceptaba los valores fuera de rango; y en LKF-388 el destructor no corría.
