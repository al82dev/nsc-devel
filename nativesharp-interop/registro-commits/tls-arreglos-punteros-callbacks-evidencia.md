# Evidencia de arreglos TLS de punteros y callbacks

Con NSC `0b07dbb95feb6f48`, la construcción incremental y la suite de sesiones
pasaron. Los controles C y NativeSharp para `int*[2]` y un arreglo de
callbacks `int(int)` TLS pasaron asignación, lectura, invocación y aislamiento
entre hilos en biblioteca estática y DLL de O0 a O3.

Tras superar la barrera TLS se corrigió el mapeo del elemento callback; las
regresiones dirigidas de callbacks C sin TLS y de arreglos TLS escalares
pasaron. La evidencia cubre las firmas y formas del fixture; siguen pendientes
otros contratos, DLL mínima y el gate integral. NSLink y C: no se modificaron.
