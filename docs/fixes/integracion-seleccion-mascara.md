# Integración de selección por máscara SIMD

El ejemplo original `cases/89-mask-select` fue rechazado antes del cambio con
`the condition of a conditional expression must have type bool`; el comando
y diagnóstico están en `A:/nsc-fixes-evidencia/integration-simd-select-before.log`.

Se aplicó el commit de la laptop `3b20616cd74fe714c80228abdf2320549961e2ce`,
después de integrar la indexación de máscara en `1769a17`. Para una condición
de máscara, se evalúan condición, primera y segunda rama una vez y en ese
orden, y se selecciona por lane. Las ramas deben coincidir con el tipo y
cantidad de lanes de la condición. El condicional escalar conserva evaluación
perezosa. El contrato y la evidencia original están en
`seleccion-mascara-simd-laptop.md`.

El build incremental `integration-simd-select-build.cmd` terminó con código 0.
El ejemplo original se compiló sin cambios y ejecutó con salida 0 en O0/O2
(`integration-simd-select-original.log`). La prueba
`integration-simd-select-test.cmd` pasó los casos de lanes mezclados,
selección de vectores y máscaras, orden y evaluación única, además de los
controles de condicional escalar y rechazos de tipo o cantidad. Se repitieron
las pruebas dirigidas de indexación y conversión de máscara y la comprobación
de solo lectura de Arc con el SDK binario de prueba; todas terminaron con
código 0. También pasó la coherencia del contrato provisional de Arc. Los
comandos y logs están en `A:/nsc-fixes-evidencia`.

El build incluye cambios locales ajenos sin registrar y no certifica un árbol
limpio. Las pruebas cubren Windows x64 y las formas documentadas; no prueban
toda la semántica de ownership o todas las plataformas. No se ejecutó la
batería completa ni se modificaron fuentes de interoperabilidad C/C++.
