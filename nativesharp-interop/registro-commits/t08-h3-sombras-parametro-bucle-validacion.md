# T08 H3: resultado de parámetros y bucles

La construcción incremental de `nsc` y `nsc_cpp_c_identity_tests` terminó con
código 0; el build generado fue `3b90b18be712a1cf`.

`diagnosticar.cmd` pasó en O0–O3: el consumidor precompilado comprobó el valor
del parámetro y el bucle, y la inspección verificó que la global C homónima no
se publicó indebidamente. `validar-ambito.cmd` también pasó en O0–O3, con
resultado 182 y una llamada C legítima después de un bucle con nombre homónimo.

No se ejecutó la validación completa de la tanda 8. Permanecen pendientes otras
formas de ámbito y la verificación de sus inicializadores.
