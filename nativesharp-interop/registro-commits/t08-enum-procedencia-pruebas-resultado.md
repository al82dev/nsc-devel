# T08: resultado de pruebas de procedencia de enum

Revisión `62ab78698`: construcción incremental de `nsc_cpp_c_identity_tests`
con código 0. Las sondas de identidad ejecutaron con código 0 las comparaciones
de enum igual, valor distinto, desplazamiento distinto y evidencia ausente.

`pruebas-tanda-08-nombres-metadata/validar-referenciados.cmd` terminó con código
0, incluyendo el cambio de `NHappy` en la misma ruta de encabezado. El driver
rechazó el consumidor con conflicto del alias `NMood`; ese diagnóstico no
identifica individualmente el valor o la huella como causa. Las comparaciones
unitarias sí distinguen el cambio de valor de forma aislada.

El control cubre la interacción probada; la tanda 8 permanece abierta y el gate
completo sigue pendiente.
