# Controles de operaciones y escape atómico

La matriz preparada compara también pre/post incremento y resta con C. Se
exige rechazar la dirección como puntero ordinario y rechazar explícitamente
la multiplicación compuesta que todavía no tiene descenso atómico. Los cambios
no acreditan otras operaciones ni el soporte completo de `_Atomic`.
