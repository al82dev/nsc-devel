# Layout de valor de decimal (NSC-NEW-001)

`ProjectObjectLayoutModel::valueLayoutForTypeKey` no listaba `builtin:decimal`. Cada consumidor que
hace esa pregunta tenía que conocer el tipo por separado; los arreglos, `stackalloc`, los elementos
crudos y `sizeof(decimal)` (NSC-NCSS-013) ya lo hacían con el layout del destino. Los que no lo
hacían fallaban:

- `sizeof(decimal[2])` y `alignof(decimal[2])`: "sizeof cannot measure 'builtin:decimal[2]': it has
  no value layout" (O0 y O2), mientras `sizeof(int[3])` medía.
- Un método `async decimal`: "NativeSharp async future has no physical layout:
  future:builtin:decimal".
- Los locales `decimal` que sobreviven a un `await`: `MirFunctionLowerer` les asigna hueco en el
  marco de la corrutina solo si esa consulta devuelve layout.

## Cambio

La consulta devuelve para `builtin:decimal` el tamaño y la alineación `Decimal` de
`TargetDataLayout::WindowsX64()` (16 y 8), el mismo origen que usan las rutas que ya funcionaban.
Los respaldos locales de esas rutas quedan como estaban y dan el mismo valor.

Consumidores revisados: el HIR de agregados y `sizeof`/`alignof`, el marco de corrutinas, el
importador de layouts MIR (futuros y errores), el layout de elementos MIR y los metadatos de
biblioteca, que solo la usan para enums. Ninguno trataba la falta de layout de `decimal` como
una regla del lenguaje.

## Prueba

`tests/integration/decimal_value_layout.py` (`nsc.compiler.decimal-value-layout`), O0 y O2, salida
42: `decimal[2]` mide 32 con alineación 8, `decimal[3]` mide 48, `struct { decimal; int; }` mide 24
con alineación 8 y aísla sus campos, un arreglo fijo conserva 0.1 y el máximo, tres decimales
sobreviven a un `yield` y a un `await`, y un `async decimal` devuelve su valor. Antes del cambio el
programa no compilaba.

Resultado: las pruebas `decimal`, `sizeof`, `alignof`, `async`, `layout` y `stackalloc` pasan (54 de
56) con los ejecutables de prueba reconstruidos. Los dos fallos no dependen de este cambio:
`nsc.compiler.async-finally-frame-regression` (`sync_inner_catch*`, error LLVM "Cleanup funclets
for the SEH personality cannot contain exceptional actions", documentado en
`async-fallback-seh-20261112.md`) y `nsc.item328.msvc-bitfield-layout`, que exigía rechazar NSMETA
por su número de versión (ver `nsmeta-bitfield-sin-version.md`).
