# Prueba directa inicial de miembros anónimos C

Se construyó incrementalmente `nsc` de `A:/nsc-interop-build/tanda1-ON` con
Clang 23 de `A:/al82dev-clang`. El control `explorar-formas.cmd`, con salida
aislada en `A:/nsc-interop-build/t08-anonimos-c-integracion`, informó que
`ProbeAnonymous` compiló y terminó con código 0. El caso escribe y comprueba
campos de una union anónima y un struct anónimo en `LAnonymous`. Los controles
`ProbeNested`, `ProbeFlexHeader` y `ProbeFlexData` también terminaron con 0.

Antes de la corrección de selección el caso rechazaba el tipo interno como
dependencia ausente; tras dicha corrección fallaba en la validación física del
alias promovido. La corrección posterior excluye ese alias del recorrido físico.

Esta evidencia cubre solo el caso directo en O0. Quedan pendientes O1–O3,
anidamientos y layout más amplios, encabezado con biblioteca, solo metadata,
compatibilidad de versiones y la validación completa de la tanda 8.
