# Verificación dirigida de cambios numéricos integrados

Se integraron selectivamente los commits de saturación decimal inexacta,
multiplicación y división de la rama numérica; los mensajes Git conservan los
identificadores de origen. El build incremental agrupó `nsc` y la prueba de
bindings async y terminó con código 0. El compilador resultante tuvo SHA-256
`090641569b19d41d13d19d9301a0a467b592f7b0c655611794ba227d0686baa0`, estable
durante las pruebas. El runtime tuvo SHA-256
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.

La prueba dirigida de saturación pasó 50 ejecuciones; la selección de
multiplicación y división pasó 58, y el runner independiente de multiplicación
pasó 14 que se solapan parcialmente con esa selección. Se usaron O0 y O2.
`A:/nsc-fixes-evidencia/numericos-integrados-20261112/results.json` conserva
comandos, cwd, temporales, códigos, salidas y hashes. No se ejecutó una batería
completa y estos resultados no verifican el consumo de Core como binario.

Los tres runners CMake fijan ahora `NSC_WINDOWS_RUNTIME_LIBRARY` y
`NSC_LINKER` en su propio entorno y llaman directamente al compilador, sin el
proceso auxiliar `cmake -E env`. Se conservaron los casos y expectativas.
La diferencia entre un resultado decimal matemáticamente reducible y la regla
de overflow de coeficiente de multiplicación no se trata como defecto de estos
commits: la revisión estática encontró que el contrato vigente comprueba ese
coeficiente. Esa comparación concreta no se ejecutó en esta tanda.

Después del build integrado se repitieron secuencialmente `Cancel(); Drop()`,
la reproducción anidada y el driver ASan. El primero pasó 18 ejecuciones; el
driver ASan pasó sin omisiones; la cancelación anidada siguió devolviendo 22
en O0/O2 y su control normal dio 42. El archivo
`A:/nsc-fixes-evidencia/regresiones-integradas-despues-numericos-20261112/results.json`
registra comandos, entorno, resultados y hashes estables. El fallo anidado no
se considera corregido, y ASan se ejecutó con `detect_leaks=0`.
