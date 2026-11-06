# Primera prueba de miembros superpuestos de una union C

La prueba existente `pruebas-tanda-08-union/validar.cmd` compiló el código C,
compiló y enlazó el consumidor NativeSharp con optimización O0, y el ejecutable
terminó con código 0. Comprueba un miembro escrito desde NativeSharp y leído
como byte superpuesto desde C. Es un caso dirigido: faltan otros niveles de
optimización, miembros compuestos, paso por valor y los demás contratos de la
tanda. No se cierra T08-011 por este resultado.
