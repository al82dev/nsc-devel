# T08: proyección TLS de punteros atómicos adicionales

El fixture TLS compiló el productor C y NSC rechazó los punteros atómicos a
float, long y unsigned long por no estar proyectados en TLS. El predicado de
importación limitaba esos tipos a globales no TLS.

Se admiten sus variantes TLS de apuntado no calificado, conservando las
restricciones de tipo, tamaño, alineación y las variantes calificadas no TLS.
Faltan build, IR, ejecución y regresión; no se cambia NSLink ni metadata.
