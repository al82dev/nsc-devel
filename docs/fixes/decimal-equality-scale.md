# Igualdad decimal con escalas diferentes

## Reproducción y causa

El coordinador compiló con el `nsc` autorizado el control
`decimal-scales-control/Probe.nsproj`; terminó con código 1 en O0/O2 al
evaluar `checked(0.10m + 0.20m) == 0.300m`. Comandos y salidas en
`A:/nsc-fixes-evidencia/prepared-source-controls-20261111/results.json`.
La fuente del control no se modificó.

En `OopLlvmOperators.cpp`, las comparaciones llegan antes que la ruta
aritmética decimal y `applyComparisonOperation` usa `LLVMBuildICmp` sobre el
valor decimal empaquetado. Coeficientes iguales después de ajustar escala
pueden tener bits distintos. Esta es la hipótesis causal del cambio; el
resultado del control por sí solo no separa suma y comparación.

## Cambio propuesto y límites

Solo `==` y `!=` para `decimal` comparan coeficientes y signos numéricamente.
Los coeficientes de 96 bits se amplían a 320 bits y se alinean por escala con
shifts y sumas de factor diez. La codificación admite seis bits de escala;
96 más `ceil(log2(10^63))` es menor que 306, por lo que esa anchura cubre la
alineación sin desbordamiento. Los dos ceros comparan iguales con cualquier
signo. Los otros operadores y tipos conservan la ruta anterior.

`tests/integration/decimal_scaled_equality` contiene controles de iguales y
desiguales, escalas en ambas direcciones, signos, cero, escala 27/28, un
coeficiente de más de 64 bits producido por multiplicación y comparación de
enteros. Los resultados previos del agente con ejecutables híbridos son evidencia no conforme según
`procedencia-herramientas-y-evidencia.md`.

La compilación incremental pasó. El control de escalas y la nueva prueba
compilaron y ejecutaron en O0/O2 con salida 0; el registro está en
`A:/nsc-fixes-evidencia/decimal-equality-integrated-20261111.log`. La prueba
registrada `nsc.compiler.decimal-scaled-equality` también pasó 1/1, sin
omisión (`decimal-scaled-equality-ctest-20261111.log`). El enlace fue exitoso;
no se ha inspeccionado exhaustivamente si se introdujo algún helper externo.
El compilador tenía otros cambios locales y no certifica una revisión limpia.
El comportamiento de `<`, `<=`, `>` y `>=`, otras operaciones decimales y el
contrato del parser sobre escalas no quedan cubiertos. El header modificado es
compartido con la rama de interoperabilidad y debe revisarse al integrar.
