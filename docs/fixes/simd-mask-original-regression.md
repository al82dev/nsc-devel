# Regresión de las máscaras SIMD originales

Las reproducciones originales de indexación, condicional y conversión de
`mask<float,8>` se copiaron byte por byte desde el ejemplo 89. La primera
aplicación textual alteró los finales de línea; se restauraron las copias
desde la referencia y se comprobó que tanto el árbol como el índice contienen
exactamente los bytes originales. El registro de comprobación del índice se
puede reproducir con `A:/nsc-fixes-evidencia/verify-simd-index.py`.

La prueba registrada `nsc.compiler.simd-mask-report-original` pasó 1/1: los
tres programas compilaron y ejecutaron con salida 0 en O0 y O2. Comando y log:
`A:/nsc-fixes-evidencia/simd-original-ctest-20261111.cmd` y `.log`. Se usaron
compilador y runtime aislados de A, y `nslink.exe` por nombre como dependencia
externa de solo lectura. Estos originales verifican que esas formas compilan
y se ejecutan, no la semántica de cada lane. El compilador contenía otros
cambios locales y no certifica una revisión limpia.
