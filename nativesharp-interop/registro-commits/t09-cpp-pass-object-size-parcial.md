# T09: argumento de tamaño de objeto en funciones C++ (parcial)

**Avance posterior:** los modos 1 y 3 ya admiten algunas expresiones;
véase [tamaños de subobjeto](t09-cpp-tamano-subobjeto-parcial.md). Los límites
de abajo describen el alcance del cambio registrado en este documento.

Clang puede agregar un `size_t` oculto después de un parámetro puntero con
`pass_object_size` o `pass_dynamic_object_size`. Antes de este cambio NSC
aceptaba una declaración C++ cdecl sin emitir el argumento oculto: el control
Clang devolvía éxito y el consumidor NativeSharp fallaba. La reproducción
original se guardó en `A:/temp/nsc-t09-object-size-rdskxkjx`.

La sesión captura el modo y la variante dinámica. Se conservan en el contrato
actual de metadatos, la identidad de caché y el transporte HIR/MIR. Para
funciones libres cdecl, los modos 0 y 2 se emiten como argumentos ocultos con
`llvm.objectsize`. La expresión puntero se evalúa una vez. Los calificadores
superiores del puntero formal no se aplican al argumento del llamador. Clang
prohíbe tomar la dirección de una función con este atributo y NSC rechaza esa
conversión.

## Validación dirigida

- Compilación incremental del compilador y pruebas afectadas.
- `pruebas-tanda-09-cpp-tamano-objeto/validar.py --solo-extremos`, con y sin
  `--const-parametros`: controles C++ y NativeSharp en O0 a O3 con biblioteca
  estática; escalares locales, campos, tamaños desconocidos, efectos laterales,
  argumentos ocultos intercalados y las dos variantes del atributo. La prueba
  también comprueba que se rechace la conversión a puntero de función.
- `validar-precompilado.py`, con y sin `--const-parametros`: ocho consumidores
  por ejecución, con y sin encabezado, en O0 a O3; incluye múltiples argumentos
  y ambas variantes.
- Pruebas dirigidas de identidad y transporte de metadatos, planes ABI y
  observación de records; regresión existente `noescape` variádica SysV.

Las carpetas de evidencia incluyen `A:/temp/nsc-t09-object-size-gptiuqyc`,
`A:/temp/nsc-t09-object-size-bn0tu260`, `A:/temp/nsc-t09-object-size-pre-pl9pwob2`
y `A:/temp/nsc-t09-variadic-noescape-qrwxi74r`.

## Pendiente

Los modos 1 y 3 requieren semántica de subobjeto: por ejemplo, el tamaño de un
campo puede diferir del tamaño del objeto que lo contiene. Se rechazan hasta
implementarlos; ejecutar `validar.py` sin `--solo-extremos` todavía falla por
esta razón. Quedan por probar otras convenciones, métodos, DLL, resolución de
sobrecargas con este atributo, el comportamiento dinámico con tamaños realmente
variables y combinaciones adicionales. El ítem T09-003 sigue abierto; esta
muestra no acredita interoperabilidad general.
