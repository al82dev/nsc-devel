# Evidencia de arreglos TLS C

Con NSC `2748b66609004562`, la construcción incremental de `nsc` y la suite
de sesiones pasó. El control C y NativeSharp leyeron y escribieron `int[2]`,
`int[3]` e `int[2][3]` TLS y comprobaron aislamiento entre hilos: biblioteca
estática con LLD y DLL con NSLink, O0–O3. La suite de sesiones terminó con
código 0. También pasaron las regresiones de puntero y global TLS estático.

La proyección acepta dimensiones fijas positivas de `int` de 32 bits; la
evidencia cubre solo las formas indicadas. Arreglos de otros elementos o con
calificadores, el fallo independiente de inicialización de DLL mínima y el
gate integral siguen pendientes. No se modificó NSLink.
