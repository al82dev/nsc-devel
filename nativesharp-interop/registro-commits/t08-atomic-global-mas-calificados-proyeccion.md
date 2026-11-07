# T08: proyección de apuntados calificados adicionales

El fixture `diagnosticar-mas-calificados.cmd` compiló el C y NSC rechazó los
seis globales atómicos no TLS a const y volatile float, long y unsigned long.
El predicado agregado para los tres tipos de 32 bits exigía apuntado sin
calificar. Se retira esa exclusión; se conservan los tipos, tamaños y límites
no TLS ya seleccionados. Faltan soporte HIR del volatile float, build, controles
positivos y negativos, IR y regresión. No se modifica NSLink ni metadata.
