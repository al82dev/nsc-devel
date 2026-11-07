# Proyección de arreglos TLS escalares

El control C pasó los arreglos escalares ampliados y NSC los rechazó antes de
generar código. Se permite proyectar arreglos fijos de elementos escalares ya
representables por `mapGlobalType`, conservando `const`; `volatile` se limita
al entero de 32 bits reproducido. El mapeo y los accesos existentes verifican
las dimensiones y calificadores. Pendiente la construcción y los controles
agrupados en biblioteca estática y DLL; no se afirma soporte de otros tipos.
