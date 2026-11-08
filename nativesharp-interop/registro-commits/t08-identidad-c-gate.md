# Tanda 8: incluir el carril de identidad en la batería

Se añade el objetivo `nsc_cpp_c_identity_tests` a la construcción incremental
del gate y se ejecuta su validador dentro de la lista agrupada. El objetivo y
el validador ya pasaron de forma dirigida tras actualizar expectativas de H4;
véase `t08-identidad-enum-campo-expectativas.md`.

El validador contiene rechazos esperados de funciones aún no implementadas.
Una salida cero confirma las expectativas registradas, no que T08-002 esté
completo. La batería completa de la tanda queda pendiente de ejecución.
