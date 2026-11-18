# Integración de lectura de máscara SIMD

El ejemplo original `cases/89-mask-index` fue rechazado antes del cambio al
tratar el índice de máscara como un índice de arreglo o puntero. El comando y
diagnóstico están en `A:/nsc-fixes-evidencia/integration-simd-index-before.log`.

Se aplicó el commit de la laptop `6c6e33b85f959b8004bcd3dd11c421f5b2860e4c`.
La lectura de un lane devuelve `bool`, evalúa receptor e índice una vez y
comprueba los límites antes de extraerlo. Se rechazan las formas inválidas
descritas en `indice-mascara-simd-laptop.md`. El archivo MIR compartido tenía
modificaciones locales para otra tarea: se preparó únicamente el caso de
máscara y los cambios previos permanecen sin registrar.

El build `integration-simd-index-build.cmd` terminó con código 0. El ejemplo
original se compiló sin cambios y ejecutó con salida 0 en O0/O2
(`integration-simd-index-original.log`). La prueba dirigida
`integration-simd-index-test.cmd` pasó sus casos de lanes, evaluación única,
rechazos y límites, incluido un índice dinámico mayor que 64 bits que produjo
la trampa esperada en O0/O2. `integration-simd-arrays-unit.cmd` ejecutó y aprobó
una prueba de arreglos y cadenas. Pasaron también las dos pruebas dirigidas de
conversión de máscara y la coherencia provisional de Arc. Los comandos y logs
están en `A:/nsc-fixes-evidencia`.

El build incremental contiene cambios locales ajenos no registrados; no
certifica una revisión limpia. Las pruebas cubren Windows x64 y los casos
documentados, no todas las plataformas o longitudes. No se ejecutó la batería
completa ni se modificaron fuentes de interoperabilidad C/C++.
