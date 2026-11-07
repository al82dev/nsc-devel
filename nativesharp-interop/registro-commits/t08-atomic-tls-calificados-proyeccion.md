# T08: proyección TLS de punteros atómicos calificados adicionales

La variante TLS compiló el productor C y Clang emitió el IR volatile esperado;
NSC rechazó los seis apuntados const y volatile de float, long y unsigned long
como no proyectados. El predicado de importación admitía esas variantes solo
sin TLS o sin calificador.

Se retira esa restricción para los tres tipos de 32 bits ya seleccionados,
conservando los requisitos de tipo, tamaño y alineación. Faltan construcción,
IR, ejecución y regresión TLS. No se modifica NSLink ni metadata.
