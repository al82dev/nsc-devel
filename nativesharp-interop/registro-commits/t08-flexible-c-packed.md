# T08-010: miembro flexible de struct C empaquetado

La proyección exigía que el miembro flexible conservara la alineación natural
de su elemento. Un `int data[]` en `pack(1)` se omitía aunque el compilador ya
puede representar accesos con alineación menor. Ahora se calcula la alineación
garantizada a partir de la alineación del record y el offset del miembro, como
para los demás campos C, y se conserva en el layout importado y reconstruido.

## Validación

- Compilación incremental de NSC: código 0.
- `pruebas-tanda-08-flexible-c/validar.cmd`, salida
  `A:/nsc-interop-build/tanda08-flexible-packed-20261107`: acceso cruzado
  C/NativeSharp, argumento y retorno por valor O0–O3, en directo y con
  biblioteca precompilada con y sin encabezado. Comparación IR: lectura y
  escritura `i32 align 1` en los tres modos, igual que Clang. Código 0.
- Regresiones agrupadas de `_Bool`, layout-clang, layout, record-c y tres
  controles de bitfields volatile: código 0, `VALIDACION_CALL_OK`.

El caso `const` continúa rechazado, y la conversión implícita del miembro a
puntero sigue pendiente. Esta muestra no cierra T08-010 ni la tanda 8; falta
la validación completa de la tanda y sus demás casos pendientes.
