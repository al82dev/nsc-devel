# Dirección de parámetros capturados en generadores async

Un generador async que recibe un `Rc` o `Arc` por valor y llama métodos del
parámetro accedía a una dirección equivocada. En la reproducción con observador
débil, los conteos fueron variables y fuera del rango esperado en O0/O2; no se
atribuyeron esos valores directamente a corrupción específica. La fuente y los
comandos están en `A:/nsc-fixes-evidencia/tuple-public-async-cancel-counts/`.

El MIR del poll registra el receptor como `ParameterAddress`. El LLVM antes de
la corrección (`llvm.log` en esa carpeta) calcula `future.frame + offset`;
en el poll de un generador async ese campo es el iterador, cuyo propio frame
contiene el parámetro. La limpieza y `ParameterValue` ya efectúan esa segunda
lectura. `lowerAddressExpression` ahora la efectúa para `$moveNext$poll` antes
de calcular el offset. No cambia la ruta de futuros ordinarios ni la de otros
polls, y conserva el tratamiento existente de parámetros de referencia.

`nsc.compiler.async-generator-parameter-address` comprueba llamadas a un struct
capturado antes y después de un await, dos elementos del iterador y un control
de función async ordinaria. El compilador base devolvió 1 en O0 antes del cambio
(`async-generator-paramaddr-baseline.log`; no se ejecutó O2 en esa comparación).
La versión de trabajo devolvió 42 en O0 y O2 (`async-generator-paramaddr-test.log`
y `async-generator-paramaddr-ctest.log`). En plataformas distintas de Windows la
prueba solo compila y no acredita ejecución.

La reproducción adicional desde fuentes con `Rc`, `WeakRc`, `Arc` y `WeakArc`
devolvió 42 en los ocho cruces O0/O2 después del cambio, incluidos los conteos
antes de cancelar y el débil retenido después del siguiente avance
(`tuple-public-async-cancel-paramaddr-after/results.json`). Los archivos de esas
API públicas y otras correcciones de tupla todavía están en desarrollo; esa
prueba no acredita el SDK binario ni todas las formas de captura. No se modificó
ni se probó interoperabilidad C/C++.
