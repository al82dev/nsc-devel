# T08-015 / T08-016: calificadores de arreglos escalares C en campos

La proyección de campos C conserva const y volatile presentes en cualquiera de
los niveles de un arreglo escalar fijo y los transporta al miembro. El
mapeo del elemento se realiza sin los calificadores solo para obtener el tipo
físico; se restituyen en la declaración del campo. La ruta de alineación
reducida acepta esos arreglos y mantiene las demás restricciones de tipos.
Clang emitió accesos volatile alineados a 1 para un arreglo empaquetado en la
consulta de referencia. La prueba mixta queda pendiente.
