# Corrección de índices para layouts MIR sintéticos

## Causa

`ProjectCompilationIndexes::rebuildMirLayouts` trataba todos los ordinales de tipo como índices de un
vector. Los layouts MIR sintéticos usan `std::numeric_limits<std::size_t>::max()` para indicar que no
tienen un ordinal normal. Incrementar ese centinela desbordaba a cero y la asignación posterior accedía
fuera de los límites del vector.

Además, `MirNodeMachine::structLayoutForTypeKey` abandonaba la búsqueda cuando el índice estaba
construido, aunque un layout sintético no estuviera representado en el índice ordinal. Eso eliminaba la
búsqueda lineal que conservaba el comportamiento anterior.

## Corrección

- Los layouts sintéticos permanecen en el índice por clave de tipo, pero no se insertan en el vector por
  ordinal.
- Una ausencia en el índice por clave conserva la búsqueda lineal existente.
- No se modifica la representación MIR, la sintaxis, el ABI ni los contratos del lenguaje.

## Validación focal

- `cmake.exe --build build-clang --target nsc`: completado correctamente.
- `TL-LANG-520`: PASS con el `nsc.exe` reconstruido.
- No se ejecutó baseline, minibaseline ni suite externa como parte de esta corrección.
