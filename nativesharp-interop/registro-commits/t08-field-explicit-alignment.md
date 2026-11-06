# T08-010 / T08-012 / T08-020: alineación de accesos de campo

El layout coloca la alineación del campo en MIR, pero las cargas y escrituras
ordinarias de campo dejaban que LLVM infiriera la alineación del tipo. Ahora
la carga directa y las lecturas/escrituras de asignación simple o compuesta
indican la alineación almacenada en MIR. El tratamiento está limitado a nodos
de campo y conserva la lógica volatile y atómica ya existente.

Esta preparación no habilita structs empaquetados: el importador todavía los
rechaza y quedan por resolver la dirección escapada, campos anidados y demás
formas de acceso. Las pruebas se agruparán con el resto de la tanda.
