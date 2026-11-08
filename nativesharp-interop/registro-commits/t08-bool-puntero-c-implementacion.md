# Tanda 8: almacenamiento bool accedido por puntero

Los accesos indirectos `bool*` leen un byte y convierten su bit bajo a `i1`; las escrituras por puntero o índice amplían el `i1` a byte. La representación del valor sigue siendo `i1`. Esto permite usar la misma operación cuando el puntero procede de C o de almacenamiento NativeSharp, incluso después de guardarlo o seleccionarlo sin conservar el origen. Los atributos volatile y de alineación se aplican a la carga antes de convertirla.

La construcción incremental pasó. `pruebas-tanda-08-bool-puntero-c` comparó IR y ejecutó O0–O3, incluyendo lectura/escritura cruzada de bool C y local nativo y selección entre punteros: `A:\nsc-interop-build\tanda08-bool-pointer-18051-4603`. Pasaron O0–O3 las regresiones de global bool, callbacks, bool volatile en DLL y productor precompilado sin header.

Esta muestra no acredita todas las formas de punteros, calificadores, concurrencia o almacenamiento bool. La tanda sigue abierta y el gate completo no se ha ejecutado.
