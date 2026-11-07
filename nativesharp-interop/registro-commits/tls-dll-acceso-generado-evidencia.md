# Evidencia dirigida de acceso directo TLS de DLL

La primera construcción falló por la constancia del índice de `LLVMBuildGEP2`;
tras corregirla, el compilador de desarrollo `8e8795a3d555ae35` construyó y
ejecutó el fixture `Native.ns`, que lee y escribe directamente el global C
TLS exportado por la DLL, en O0–O3 con NSLink sin modificaciones. El control C
por directorio PE pasó y verificó aislamiento entre dos hilos. La lectura del
hilo principal se comparó con `tls_read` y se mantuvo tras el hilo de la DLL.

El control negativo de referencias estática y DLL ambiguas pasó. Los fixtures
de global TLS estático y escalares TLS estáticos volvieron a pasar O0–O3,
incluidos `const` y `volatile`. Se añade el nuevo positivo al gate de tanda.

La prueba previa de consumidor C que usa directamente `SECREL` sigue en la
batería y falla antes de llegar a NSC; su interpretación y resolución siguen
pendientes. Falta comprobar accesos NativeSharp desde un segundo hilo y otros
casos de DLL. No se ha ejecutado ni aprobado el gate completo de la tanda 8.
