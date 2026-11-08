# T08-010/015: alineación comprobada de aliases en HIR

Los accesos HIR por alias C volatile pueden transportar la alineación del
escalar si el binding semántico acredita una alineación suficiente y múltiplo
del ancho de 1, 2, 4 u 8 bytes. Sin contrato, sin modo, con ancho no reconocido
o con alineación insuficiente, no se crea la marca de alineación atómica.

La protección de configuración explícita sigue activa. Este cambio no prueba
que el análisis produzca la alineación adecuada ni cubre todos los flujos de
punteros; se necesitan controles de reasignación, campos empaquetados, bool C,
LLVM y caché antes de habilitarlo. Aún no se ha construido ni ejecutado.
