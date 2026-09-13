# NativeSharp 1.0.1-dev — cambios acumulados desde 0.9

Esta lista resume la línea actual para consumidores. Los razonamientos, repros y commits completos
permanecen en `mkdown/`; aquí sólo se conserva el resultado que afecta cómo se programa, depura o
empaqueta con NativeSharp 1.0.1.

## Lenguaje y Core

- `static_assert(condición[, "mensaje"])` certifica constantes y layouts dentro de bloques sin
  generar código de ejecución.
- `offsetof(T, campo)` obtiene como `nuint` el desplazamiento físico de un campo de struct o union
  usando el mismo layout canónico que el ABI, los arreglos y los headers C.
- `align(N)` puede aplicarse a un campo de instancia para elevar su alineación, su offset y la
  alineación final del agregado; el contrato se conserva en NSMETA y headers C.
- `T[*]` declara una cola inline flexible como último campo de un struct; `sizeof` cubre sólo el
  encabezado, el indexado es raw y el header C publica `T field[]`.
- `readonly struct` declara un valor inmutable después de construcción: todos sus campos de
  instancia deben ser `readonly`, mientras que sus campos estáticos conservan reglas independientes.
- Los receptores readonly se aplican a parámetros `in`, campos readonly, miembros marcados
  `readonly` y todos los miembros de instancia de un `readonly struct`; permiten lectura pero no
  stores ni llamadas a miembros mutadores.
- Los arreglos multidimensionales fijos usan `T[d0, d1, ...]`, almacenamiento inline contiguo y
  orden row-major sin cabecera ni asignación de heap.
- `array[i0, i1, ...]` accede directamente a arreglos fijos multidimensionales y escala cada índice
  por el stride canónico de su dimensión.
- `T[length] local;` reserva un arreglo local de longitud evaluada en ejecución directamente en el
  frame de la función, sin `new`, `delete` ni cabecera de heap; los campos y firmas siguen exigiendo
  dimensiones contractuales.
- `T[rows, columns] local;` compone dimensiones de ejecución en una sola reserva row-major; cada
  extensión se evalúa una vez al declarar y conserva sus strides aunque cambien las variables fuente.
- Cada dimensión VLA se valida antes de convertirla a `nuint`; el producto se comprueba antes de
  multiplicar y una reserva individual no puede superar 1 MiB de stack.
- El almacenamiento de un VLA no puede escapar mediante retornos, campos o capturas de lambdas;
  los aliases locales conservan su procedencia para que la regla no se pierda al copiar el puntero.
- `const string` es una constante de compilación utilizable en APIs y metadata.
- Strings nativos conservan unión condicional, literales y préstamos en constructores.
- Enums, conversiones numéricas y conversiones de usuario conservan identidad lógica y física.
- Las clases parciales anidadas participan en la misma agregación nominal que las de nivel superior.
- Punteros, handles, delegates `extern C`, callbacks, structs y arrays siguen el ABI Windows x64.

## Bibliotecas y DLLs

- NSMETA avanzó a 1.12 y publica clausura nominal externa y templates transitivos.
- Clases estáticas, interfaces, eventos, arreglos de tipos referencia y especializaciones genéricas
  cerradas sobreviven el consumo mediante `LibraryReference`.
- Callables, constructores y destructores precompilados conservan nombres físicos enlazables.
- Las especializaciones reutilizables y helpers EH aplican reglas ODR/COMDAT por símbolo.
- Los helpers locales de closures, inicializadores estáticos y constantes precompiladas aplican
  ownership e identidad física coherentes entre unidades, bibliotecas estáticas y DLLs.
- Las bibliotecas estáticas no venden como propias las definiciones de sus dependencias.
- La selección de runtimes depende de símbolos efectivamente alcanzados y deduplica DLLs idénticas
  por contenido, no por ruta incidental.
- El verificador compara de manera temprana NSMETA, archivos COFF, DLL e import library.
- NSMETA omite miembros privados e internos de tipos públicos sin perder el layout físico requerido;
  tipos usados sólo por campos estáticos privados ya no contaminan la clausura pública.

## Depuración

- Se formalizó el contrato CodeView/PDB para líneas, variables, tipos y funciones sintetizadas.
- Los checksums SHA-256 y `-fdebug-prefix-map` permiten identidad de fuente reproducible.
- Se documentaron y cubrieron `this`, structs, scopes, thunks, dispatch de interfaces, SEH,
  excepciones, closures y código optimizado.
- El verificador PDB reconoce helpers locales y nombres físicos largos sin confundirlos con símbolos
  públicos ausentes.
- La certificación PDB inspecciona todas las unidades de código particionadas y permite ejecutar su
  evidencia dirigida únicamente en O0 sin reducir la cobertura O0/O2 del baseline.

## Herramientas y desarrollo local

- Cada build muestra una identidad única y ahora pertenece a `1.0.1-dev`.
- NSC Builder ofrece limpieza manual y segura de árboles de construcción.
- Los productos regenerables de CMake/CTest y bytecode Python dejaron de contaminar el índice.
- La documentación actual se instala y verifica bajo `share/nativesharp/docs/1.0.1`.

## Estado

La línea permanece en desarrollo. Este changelog no sustituye un baseline ni afirma certificación.
La promoción a `1.0.1` final se registrará como una entrega separada cuando exista evidencia nueva.
