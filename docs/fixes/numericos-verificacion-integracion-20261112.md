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

## Repetición dirigida en la integradora

La revisión probada fue `f8ae07b1eae8c4df813034d16ebf6426d74c435d`, con
árbol limpio antes y después. El target `nsc` se compiló incrementalmente en
`A:/nsc-fixes-evidencia/build`; su SHA-256 durante las pruebas fue
`4b16e12bc828624cf6bb85958c9a2c7d8eda1cebc9d6ae73b906545bb49ae2e0`.
El runtime independiente tuvo SHA-256
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.
El entorno, comandos, códigos, resultados y hashes se conservan en
`A:/nsc-fixes-evidencia/numericos-actual-1794526923112333100/results.json`;
el ejecutor fue `A:/nsc-fixes-evidencia/numericos-integrados-dirigido.py`.
Sus 69 entradas son invocaciones de procesos, no 69 casos independientes.

Pasaron las 13 invocaciones dirigidas de saturación decimal, multiplicación y
división decimal, checked decimal, suma checked de i128, división y residuo
i128/u128, arreglos decimal fijos y genéricos, stackalloc y matemática
determinista desde fuente. El ejecutor de saturación cubrió 25 entradas O0/O2;
el de multiplicación/división, 29 entradas O0/O2, sin fallos. Los controles
de matemática fuente cubrieron O0–O3 y los modos precise y deterministic.

Los cuatro casos COMP-01 a COMP-04 del reporte pasaron en O0/O2. Para los
tres proyectos con referencia histórica a C:, se copiaron las fuentes sin
cambiar bytes y se generaron proyectos auxiliares que apuntan al Core fuente
de la copia o a los Core binarios aislados O0 y O2, con ambos niveles de
optimización del consumidor. La salida de saturación fue `2147483647`, `32767`
y `255`; la de decimal verificó factura `2314`, igualdad verdadera y redondeos
`100/101/101`. El overflow checked terminó con trap `147`. El proyecto original
de división, que no referencia Core, también compiló y ejecutó directamente
sin modificar su fuente ni su proyecto, O0/O2. El detalle de este último está
en `A:/nsc-fixes-evidencia/numericos-originales-extra-1794527031517761200/`.

El ejemplo original `70` de matriz decimal compiló y ejecutó con salida 0 en
O0/O2 con Core fuente y con los dos Core binarios, usando copias idénticas de
`Program.ns` y `Matrix.ns` y proyectos auxiliares aislados. El `Main` original
escribe un elemento, pero no verifica su lectura; la regresión genérica y las
pruebas de arreglos proporcionan controles adicionales, no prueba exhaustiva.

La prueba de `DeterministicMath` pasó además con cada Core binario O0/O2 y
consumidores O0/O2 en modos precise y deterministic, con denormales preserve:
ocho combinaciones de ejecución. Las opciones de punto flotante corresponden al
consumidor; no se afirma que los Core binarios se recompilaran con cada modo.
Sus hashes coinciden con el manifiesto del SDK de `14a65343f`, y no hubo cambios
en `lib/NativeSharp.Core` entre esa revisión y la probada. No se acredita
precisión general ni modos ftz/daz.

Se registraron en `CMakeLists.txt` las dos regresiones existentes que aún se
invocaban directamente: `nsc.compiler.decimal-saturating-inexact` y
`nsc.compiler.decimal-multiply-divide`. Tras regenerar el build incremental,
`ctest -N -V` descubrió exactamente una prueba para cada nombre y mostró el
compilador y runtime aislados en A. Las ejecuciones dirigidas `ctest --test-dir
A:/nsc-fixes-evidencia/build -R decimal-saturating-inexact --output-on-failure`
y la equivalente para `decimal-multiply-divide` pasaron 1/1 cada una. En esa
segunda compilación incremental el compilador tuvo SHA-256
`d32393769b57b2b1d3a9de1dd7a1246da1db677a5b7ffce052b01f91d8e78023`;
el único cambio de producto era el registro de pruebas. `CMakeLists.txt` es
compartido con otras ramas y su integración deberá revisarse.

No se ejecutó batería completa, no se modificaron ejemplos, interoperabilidad
C/C++ ni la instalación, y no se considera demostrada la aritmética para todas
las entradas o plataformas.
