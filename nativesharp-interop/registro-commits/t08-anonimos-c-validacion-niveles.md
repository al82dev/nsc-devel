# Validación inicial de anónimos C anidados y empaquetados

El fixture ampliado terminó con código 0 en O0–O3 para consumo directo, Shared y
Metadata usando `A:/nsc-interop-build/tanda1-ON` y Clang 23 de A:. C comprobó
los valores escritos a través de un anidamiento de dos niveles y de miembros
anónimos en un struct `pack(1)`.

Se inspeccionó el IR sin optimizar de los tres modos. La escritura del `int`
empaquetado en offset 1 y la del `short` en offset 5 usan ambas alineación 1.
Esta inspección no contrasta automáticamente todos los accesos contra el IR de
Clang ni cubre bitfields, otras alineaciones o niveles optimizados. La tanda 8
y su validación completa siguen pendientes.
