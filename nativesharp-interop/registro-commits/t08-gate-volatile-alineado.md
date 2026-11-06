# T08-015: comparación reproducible de volatile alineado

Clang 23.1.0 emitió release/acquire para el elemento enum volatile alineado
de `layout_aligned_volatile_pointer`; NSC emitió store/load volatile ordinarios
para el acceso correspondiente. El verificador compara ambos IR y se integra
al fixture para impedir que el resultado funcional oculte la diferencia. Se
espera fallo hasta implementar y validar la semántica correcta. El campo entero
del record mostrado por Clang permanece volatile ordinario; no generalizar.
