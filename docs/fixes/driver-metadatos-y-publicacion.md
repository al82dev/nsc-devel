# Driver: metadatos reconstruidos, publicación, rutas, migración e instalación

Fichas LKF-240 a 247, 250 a 257, 259 a 263, 272 a 276 y 347.

## Defectos corregidos

- **LKF-260.** Un parámetro `char` con valor predeterminado fuera del ASCII imprimible (control,
  DEL, 0x80 a 0xFF) hacía fallar la verificación de lectura de la propia biblioteca ("cannot spell
  non-ASCII character default"). Un `char` es un byte, así que ahora se escribe `'\xHH'`, que el
  lexer decodifica al mismo byte. Del mismo modo `escapedStringLiteral` escribía crudos los bytes de
  control y los no ASCII, que un literal de cadena rechaza ("invalid or unterminated string
  literal"); ahora todo byte fuera del ASCII imprimible se escribe `\xHH`.
- **LKF-261.** Las constantes `float`/`double` se escribían con un `ostringstream` sin locale fijo y
  sin tratar infinito ni NaN (`inf.0f`, que el parser rechaza). Ahora el flujo usa el locale clásico
  y los valores no finitos se escriben como las divisiones constantes que el front-end pliega al
  mismo valor (`(1.0 / 0.0)`, `(-1.0 / 0.0)`, `(0.0 / 0.0)`). Los finitos conservan sus bits
  (`max_digits10`).
- **LKF-242.** `irTextSymbolReference` entrecomillaba un nombre sin escapar su contenido. Ahora
  escapa como `printEscapedString` de LLVM: comilla, barra inversa y todo byte no imprimible pasan a
  `\HH`.
- **LKF-240.** El rollback de una transacción de artefactos no comprobaba si había podido quitar un
  artefacto nuevo sin versión anterior; ahora un fallo así marca el rollback como incompleto.
- **LKF-241.** Los nombres hermano y de respaldo de una publicación llevan el id del proceso además
  del ordinal, así dos procesos que publican el mismo destino no reutilizan nombres.
- **LKF-246.** Tras publicar una biblioteca sin sidecar ThinLTO, un fallo al quitar el sidecar viejo
  se informa como advertencia (el sidecar viejo sigue siendo rechazado por sus huellas).
- **LKF-247.** La ayuda en inglés decía que `-g` aplica a objetos y ejecutables; aplica también a
  bibliotecas y DLL, como ya decía el uso y la clasificación de la invocación.
- **LKF-250.** El lector JSON interno era recursivo sin límite; ahora rechaza más de 256 niveles de
  anidación con un error (256 se aceptan, 257 y 200000 se rechazan).
- **LKF-251.** La validación de referencias precompiladas marcaba como validada la ruta de la
  biblioteca y omitía después cualquier otra aparición, aunque llegara con otra metadata u otro
  runtime. Ahora solo se omite una repetición idéntica; una combinación distinta se rechaza. (La
  preparación del proyecto ya rechaza dos metadatas para el mismo módulo, comprobado por
  `nsc.stage46.native-libraries`; esta es la defensa local.)
- **LKF-253.** Los marcadores `nsc-migration:generated` y `nsc-migration:disable-next-line` se
  buscaban en texto crudo, así que una cadena que los contuviera saltaba el archivo o suprimía
  hallazgos. Ahora solo cuentan dentro de un comentario `//` (al inicio de línea o al final de
  código sin comillas antes), como documenta NS3-T16-F02.
- **LKF-254.** Aplicar correcciones de migración rechazaba solo `origen == destino` textual y luego
  truncaba el destino. Ahora también rechaza el mismo archivo con otra escritura (`equivalent`,
  `weakly_canonical`), antes de truncar.
- **LKF-256.** Los temporales y respaldos del ciclo de instalación tenían nombres fijos (`.tmp`,
  `.previous`); ahora llevan el id del proceso.
- **LKF-257.** El retiro de una instalación eliminaba en secuencia y se detenía en el primer fallo
  sin restaurar lo ya eliminado. Ahora primero aparta cada destino con un nombre por proceso; si uno
  no se puede apartar, los apartados vuelven y no se elimina nada; solo con todos apartados se
  eliminan.
- **LKF-259.** Un fallo al publicar un bundle de compilación en caché se descartaba en silencio.
  La compilación sigue siendo exitosa (es caché), pero ahora se informa como advertencia; un
  manifiesto sin identidad no es cacheable y no se informa.
- **LKF-263.** `foldedPathKey` bajaba cada byte con `std::tolower`, que depende del locale global
  para bytes mayores que 0x7F y no pliega letras no ASCII. Ahora la caja se pliega como el sistema
  de archivos (`LCMapStringEx` invariante en Windows, archivo nuevo `PathKeyCaseFolding.cpp`) y el
  paso ASCII es explícito. La ruta no se hace absoluta: una relativa lo es a su dueño (un primer
  intento con `absolute()` duplicó fuentes en `nsc.item391-thinlto` y se retiró).
- **LKF-273.** El recorrido de carriers anónimos no tenía control local de ciclos; ahora se detiene
  ante un carrier activo repetido o pasados 256 niveles, con error.
- **LKF-274.** `collectPublishedMembers` recorría los miembros una vez y omitía los de un tipo que
  solo después pasaba a ser dependencia de contrato; además los tipos de las firmas de namespace se
  añadían después. Ahora las firmas de namespace se recogen antes y el recorrido se repite hasta que
  no aparece un tipo de contrato nuevo, conservando el orden de declaración.
- **LKF-275.** El archivo COFF de una biblioteca consumida por ThinLTO se reutilizaba por los 16
  primeros dígitos de su digest si pasaba la verificación estructural. Ahora el directorio usa el
  digest completo y la reutilización exige un sello con los objetos y el digest de los bytes del
  archivo; un archivo sin sello confirmado no se reutiliza (el build enlaza el suyo).
- **LKF-276.** Las dependencias de un sidecar ThinLTO se buscaban por nombre de archivo y se
  conservaba la primera ruta con ese nombre; ahora toda metadata con ese nombre es candidata.

## Contratos verificados

- **LKF-244.** Las opciones explícitas de depuración, mapas de prefijo y perfil se aceptan en las
  cuatro acciones OOP de proyecto (obj, exe, lib, dll) y se rechazan en las demás
  (`CompilerInvocation.cpp`); la ayuda de LKF-247 ya lo dice.
- **LKF-252.** La presencia del nombre UTF-16LE de un runtime no ASCII en el PE es un indicio
  adicional, no la única comprobación: la tabla de exports se compara con el contrato de la
  metadata antes (`validateLibraryReferenceRuntime`).
- **LKF-255.** La comparación de reproducibilidad de manifiestos trata el ejecutable y el PDB del
  compilador como volátiles (llevan la identidad de build) y compara el resto; lo fija
  `nsc.stage66.e9-e10-release-contract`. No es igualdad binaria ni pretende serlo.
- **LKF-262.** Los genéricos abiertos se publican como plantillas, no como claves de tipo, y los
  tipos función no tienen forma de fuente en la superficie importada; las bibliotecas genéricas del
  SDK se consumen por la ruta de plantillas (`nsc.sdk.*`).
- **LKF-272.** Una propiedad sin `get` no llega a la metadata: el lenguaje la rechaza al compilar la
  biblioteca ("a property without one cannot be bound"), así que la exigencia de getter del
  materializador coincide con el lenguaje.

- **LKF-347.** Una clase precompilada con inicializadores de campo los ejecuta cuando el consumidor
  la construye, con el constructor implícito y con uno explícito, en O0 y O2.

## Pruebas

- `nsc.integration.metadata-surface-values` (nueva, 7 casos, O0 y O2): defaults `char` 0x07, 0x80,
  0xFF, comilla y NUL; constantes infinitas, NaN y finitas; cadena con controles y UTF-8; propiedad
  sin getter rechazada; inicializadores de campo de una clase precompilada. Con el compilador instalado los cuatro primeros fallan en la verificación de
  lectura de la biblioteca.
- `nsc.compilation-model` (escapes IR), `nsc.driver.json-numbers` (profundidad),
  `nsc.ns3.t16.migration` (marcadores en literales, comentario final, ruta alias),
  `nsc.stage66.e9-e10-release-contract` (retiro interrumpido por un handle abierto: nada se elimina,
  y con el handle cerrado el retiro termina).
- `nsc.artifact-publisher`: la prueba no preparaba el acompañante `.nsproducts` que la publicación
  DLL incluye desde `5b948560b`; ahora lo prepara y lo comprueba.

Fuera de alcance: LKF-258 y LKF-269 (sesiones y dependencias C/C++, interoperabilidad).
