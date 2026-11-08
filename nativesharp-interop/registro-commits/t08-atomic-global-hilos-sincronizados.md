# Tanda 8: control sincronizado de global atómico

El control de dos hilos coordina explícitamente 100 rondas entre C y NativeSharp mediante una segunda variable `_Atomic(int)`. Cada ronda establece que el hilo C está activo antes de comenzar los incrementos y que ambos completaron su trabajo antes de la siguiente. Cada lado incrementa 100 veces el mismo contador por ronda; se comprueba el resultado final de 20 000. Las esperas tienen límites y el código de error del trabajador se conserva en una variable atómica.

El validador compara en el IR de Clang y NSC las lecturas y escrituras `seq_cst` de la señal y el `atomicrmw add` del contador. Pasó O0–O3: `A:\nsc-interop-build\tanda08-atomic-global-thread-21464-30032`.

La coordinación demuestra que ambos hilos permanecen activos y se sincronizan durante la prueba; no demuestra ejecución simultánea de las instrucciones de incremento ni sustituye la validación completa de la tanda.
