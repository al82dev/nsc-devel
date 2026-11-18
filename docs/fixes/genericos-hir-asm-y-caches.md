# Clones genéricos, HIR de generadores, ensamblador integrado, cachés y metadatos

Fichas LKF-117, 120, 128, 132, 139, 322, 335, 352 a 354, 357, 358, 360, 363, 366 a 369, 372, 373,
375, 378 a 380, 389, 393 a 395, 409, 410, 414, 417, 433, 437, NSC-AV-W64-003/004, LIB-261 y
NSC-LIB-007. Cada defecto se reprodujo con un programa antes
de corregirlo (sondas `b90` a `b93` en `A:/nsc-fixes-evidencia/batch/w`), salvo donde se indica.

## Defectos corregidos

- **LKF-128.** `out(x : "+r")` pasaba `+r` a LLVM, que no tiene esa restricción de registro, con el
  valor previo como argumento sin atar: en O0 el resultado era basura (1, 0, 0 en lugar de 42, 42, 43)
  y en O2 la compilación fallaba. Ahora una salida de registro de lectura-escritura es la salida `=r`
  (o `={reg}`) más una entrada atada a su número de operando, añadida tras las entradas ordinarias
  para que `$N` siga nombrando las entradas escritas. `nsc.integrated-assembly.mir` fijaba la
  escritura `+r,~{cc}` y ahora espera `=r,0,~{cc}`.
- **LKF-358.** El clonador genérico no copiaba el perfil de fallo del asignador: dentro de un método
  genérico, `new (a, failure: null) A(9)` con un asignador que devuelve null atrapaba con 135 en vez
  de dar null (el mismo código fuera de un genérico daba 42).
- **LKF-369 y LKF-357.** La expansión de alias copiaba tres campos del modificador y perdía el rango:
  `using Grid = int[,];` se volvía `int[]` ("cannot initialize local 'g' of type 'builtin:int[]' from
  'builtin:int[,]'"). El clonador perdía igual los contratos de puntero (nulidad, procedencia,
  alineación, espacio de direcciones). Ahora se copia el modificador completo y solo se clona la
  extensión de arreglo.
- **LKF-379.** En un generador síncrono con tipo de error, un `throw` se reescribía como
  `return false` sin las regiones de limpieza que cruzaba: ni el `finally` ni los locales `raii`
  corrían (41 en lugar de 42), y un `throw` con `catch` dentro del generador tampoco llegaba a su
  `catch`. Ahora el retorno sintetizado lleva las regiones del `throw` y un `throw` con manejador
  local sigue siendo nativo, como en los cuerpos `async` (C-13).
- **LKF-378.** La comprobación `noheap` de invocaciones indirectas solo miraba la raíz de la
  sentencia: `return 40 + f();` con `f` sin contrato `noheap` se aceptaba. Ahora todo frame de
  invocación de delegate, puntero a función o evento lo comprueba, y un tipo función declarado
  `noheap` (`fn*() -> int noheap`) conserva el contrato también en la raíz.
- **LKF-414.** Un `async int*` se declaraba pero no se podía esperar ("await operand must be a Future
  value"): la clave `future:builtin:int*` terminaba en `*` y se leía como puntero a future. El
  resultado puntero va ahora entre paréntesis (`future:(builtin:int*)`).
- **LKF-360.** Un clon de lambda genérica que superaba el límite de anidación devolvía null y se
  desreferenciaba; ahora se informa.
- **LKF-363.** Un argumento genérico inferido perdía el calificador de referencia, la vida y los
  atributos de tipo función; ahora se copian.
- **LKF-368.** La forma de restricción que debe repetir una `impl` genérica no incluía `ref-like` ni
  `where T.Item == U`.
- **LKF-322.** Los nombres de respaldo de valores fuera de su enumeración nombraban una categoría
  válida (method, local, finally); ahora dicen `unknown`.
- **LKF-353.** Las listas del catálogo de plantillas escribían su tamaño pero un paso nulo no escribía
  nada; ahora cuentan solo las entradas no nulas, así el lector no se desfasa.
- **LKF-354.** Los lectores del catálogo recursan por nodo; un payload más profundo que 4096 niveles
  es un fallo de caché, y una entrada se restaura solo si su payload se consumió entero.
- **LKF-389.** Los índices en preorden del caché MIR desreferenciaban hijos nulos; ahora ambos
  recorridos los omiten con la misma numeración.
- **LKF-393.** Reconstruir el modelo de funciones de namespace no vaciaba las referencias a estado.
- **LKF-394.** El layout del frame de Future sumaba y alineaba sin comprobar overflow; ahora con
  aritmética comprobada y diagnóstico.
- **LKF-395.** El volcado MIR imprimía `0` para un ordinal o posición de bit ausente; ahora `none`.
- **LKF-409.** Sin layout del modelo, un enum se publicaba en NSMETA como 4/4; ahora toma el layout
  de su tipo subyacente.
- **LKF-410.** Al fusionar fragmentos NSMETA no se comparaban las banderas de lenguaje y se perdían
  las implementaciones de trait de los fragmentos posteriores al primero; ahora se comparan y se
  añaden (una repetida debe coincidir).
- **LKF-139.** El bundle NSDIAG redactaba solo `--token=...` en minúsculas; ahora también la forma
  separada (`--token VALOR`), cualquier caja, y el valor repetido en stdout o stderr. El bundle sigue
  sin ser anónimo (rutas y huellas de fuentes se registran a propósito).
- **LKF-380.** El rebase de ordinales de la construcción paralela de HIR movía las regiones sintéticas
  de `OwnedTemporaryBorrow` pero no las que nombra una ruta de salida (`cleanupRegionOrdinals`);
  ahora se cuentan y se mueven igual (las semánticas, menores que el piso, no cambian).
- **LKF-366.** El recolector de dependencias de plantillas no recorría el resultado ni el error de un
  tipo función (`fn*(T) -> Box<T>`); ahora sí.
- **LKF-433.** La traza LLVM opcional (desactivada por defecto) escribía en `A:\temp` con nombre
  fijo y truncado; ahora usa el directorio temporal con el id del proceso e informa si no se abre.
- **LKF-437.** Una entrada del caché de backends ThinLTO se aceptaba si era un archivo regular; ahora
  debe ser un objeto COFF AMD64 o bigobj, y el temporal de publicación lleva el id del proceso.

## Contratos verificados

- **LKF-117.** Un array inline cuyo layout desborda se rechaza ("struct layout overflows the Windows
  x64 value-size model", "field array layout overflows the Windows x64 object-size model").
- **LKF-120.** Un bitfield más ancho que su almacenamiento se rechaza; uno de 64 bits funciona.
- **LKF-132.** Una división por cero o un índice fuera de rango sin uso siguen atrapando (129, 132) en
  O0 y O2: el clasificador de efectos no los elimina.
- **LKF-335.** La colección de un `foreach` se evalúa una sola vez.
- **LKF-352.** La huella de plantilla concatena los lexemas del nodo; dos fuentes con los mismos tokens
  son la misma plantilla, y los comentarios no cuentan, que es lo deseado.
- **LKF-367.** La resolución calificada por módulo no filtra visibilidad a propósito: es la escritura
  que usa la fuente reconstruida desde metadatos para nombrar una plantilla por su identidad NSMETA.
  Filtrarla rompió `nsc.stage48_2.library-metadata` y se retiró.
- **LKF-417.** Las auditorías de depuración semántica y de coste cero cubren categorías y
  optimizaciones como conjuntos separados, no como matriz; el fixture TL-LANG-140 lo fija y el código
  lo dice ahora en un comentario.
- **NSC-AV-W64-003.** No hay boxing (`Object.ns`): `Object o = 5;` y `Object o = s;` con un struct se
  rechazan en compilación.
- **NSC-AV-W64-004.** Un tipo anidado no declara parámetros genéricos ("declare 'Box' at namespace
  scope"); la alternativa a nivel de namespace compila y ejecuta.
- **LIB-261, NSC-LIB-007.** `unsafe` es opcional: desreferenciar un puntero y convertir entre entero y
  puntero fuera de un bloque `unsafe` compila y ejecuta. `NS3-T13-F05-unsafe-auditing.md` (ítems y
  condensado) lleva ahora una nota de vigencia; el texto original se conserva.
- **LKF-372, 373, 375.** La invocación de un evento con argumento `owned` transfiere y libera una vez;
  los índices desde el final funcionan en arreglos bidimensionales; la desestructuración de una tupla
  de referencias prestadas no destruye nada.

## Reproducido y pendiente de decisión

- **LKF-374 y NSC-NEW-031.** La capacidad `trailing:` de una construcción con asignador nunca se
  analiza ni se suma: el asignador recibe solo el tamaño de la cabecera (8 bytes con `trailing: 16`),
  y la construcción devuelve la cabecera por valor (`OopLlvmConstruction.cpp`, "trailing.header"),
  así que `p.Data[i]` direcciona la cola de la copia local y no la memoria reservada. Corregirlo
  exige decidir la forma del resultado (puntero o referencia a la reserva) y la unidad de la
  capacidad (elementos o bytes).

## Pruebas

- `nsc.compiler.generic-clone-and-asm-contracts` (nueva, 25 casos, O0 y O2).
- `nsc.compilation-telemetry` (redacción NSDIAG), `nsc.integrated-assembly.mir` (restricción atada).
