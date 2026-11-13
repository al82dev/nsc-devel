# Integración del caso 73 de la laptop

## Reproducción original disponible en la integradora

La fuente original del reporte está disponible en esta máquina. Se copió sin
cambiar bytes desde `A:/native-sharp-reference/code-examples/chapter-17/73_comptime_crc32_and_trig_tables/expected-errors/comptime_function_call/Program.ns`;
su SHA-256 es `caaf64439020596d775a98541920e9d416f5aba61903e779aa300900de482486`.
Con el compilador aislado construido desde esta copia, SHA-256
`d4386128b8f3e6aabed0f7498811ed554f36832725b40a54d8fe917891be76c6`, la
copia y un proyecto auxiliar compilaron en O0/O2 y ambas ejecuciones devolvieron
42. La fuente original se comparó antes y después; comandos, cwd y salidas están
en `A:/nsc-fixes-evidencia/run73-original-current-20261113.log`.

La copia byte a byte se conserva en `tests/integration/run73_original_fixture`;
`run73_original.py` comprueba su hash antes de repetir el control. Pasó O0/O2
mediante `run73-original-fixture-20261113.cmd` y su log en la misma carpeta de
evidencia. Esta verificación usa un proyecto auxiliar, no demuestra soporte de
todas las funciones de `#run` ni sustituye las restricciones documentadas abajo.

Se aplicó el cambio `82f56e06c69fb59e4f2db4a7cbf7983ca174d0bd` a
`development-fixes-limitaciones`, sin mezclar ramas. El contrato limitado,
decisiones, pruebas y restricciones del evaluador figuran en
`73-run-laptop.md`. El archivo `CMakeLists.txt` tenía cambios sin registrar de
otra tarea: solo se prepararon las dos inserciones del evaluador y su prueba;
las demás modificaciones permanecen en el árbol sin formar parte del commit.

`A:/nsc-fixes-evidencia/integration-run73-build.cmd` compiló incrementalmente
`nsc`, la nueva prueba unitaria y las cuatro pruebas históricas, con código 0.
El build recompiló `src/Interop/CHeaderGenerator.cpp` como dependencia del
objetivo `nsc` en A; no se modificó esa fuente ni se usaron los builds o
pruebas de interoperabilidad C/C++. El compilador enlazado incluye cambios
locales sin registrar de otras áreas y no certifica un árbol limpio.

`integration-run73-unit.cmd` ejecutó cinco entradas: la prueba nueva y las
cuatro históricas de motor, sandbox, caché y reproducibilidad; pasaron 5/5.
`integration-run73-test.cmd` ejecutó la regresión funcional con código 0; su
log registra 116 invocaciones de compilador y ejecutables, incluyendo los
casos O0/O2 válidos y los rechazos esperados. También pasaron los controles
dirigidos `conditional-unsafe-coherence-after.cmd` e
`integration-external-sources-test.cmd` después de enlazar el nuevo compilador.
Los comandos y salidas están en los archivos homónimos de evidencia en A.

No se ejecutó el objetivo independiente de compiler-suite ni la batería
completa. La comparación con el ejemplo original de la laptop fue omitida por
autorización previa, tal como registra `73-run-laptop.md`. La reserva de memoria
del parser es contable y no demuestra un límite duro del heap. El perfil no
admite todas las funciones ni efectos del lenguaje; los rechazos documentados
no deben interpretarse como soporte general de `#run`.

La regresión funcional se repitió con el compilador aislado de la campaña,
SHA-256 `e486f7786bf580a06b9ec41a41ca92d639666000875bef74ec0f5c1adb07fb27`.
`A:/nsc-fixes-evidencia/integration-run73-current-test.cmd` terminó con código 0;
su log conserva compilaciones y ejecuciones O0/O2 y rechazos esperados. El árbol
de trabajo contiene otros cambios sin commit, por lo que la repetición no es una
certificación de `fc7ea5bb7` por separado ni de todos los usos de `#run`.
