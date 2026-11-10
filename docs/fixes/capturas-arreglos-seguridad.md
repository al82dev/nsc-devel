# Seguridad de capturas de arreglos

Una tarea enviada a un ejecutor de hilos capturaba por referencia un `Raw[]`
cuyo elemento `Raw` contiene `byte*`. Antes de la corrección, el análisis
aceptó arreglos dinámicos, fijos y anidados de ese tipo, igual que sus
controles `Safe`; los seis resultados y comandos están en
`A:/nsc-fixes-evidencia/thread-array-capture/matrix-before.json`. La regresión
falló inicialmente al aceptar `Raw[]` (`regression-before.log`). No se ejecutó
la tarea que contiene el puntero.

`callTypeIsSend` y `callTypeIsSync` consultaban el tipo nominal sin descender
por los elementos de un arreglo. Ambas rutas usan ahora los helpers existentes
para arreglos fijos y nativos y continúan el recorrido sobre cada elemento,
incluidos arreglos anidados y campos de estructuras. Los rechazos y controles
se verificaron con `tests/integration/thread_array_capture.py`: pasó desde
fuentes (`thread-array-capture/regression.log`) y con el SDK binario aislado
(`regression-binary.log`). Rechaza elementos con punteros crudos y un elemento
Send que no es Sync; acepta arreglos y contenedores de elementos seguros. La
selección existente de seguridad anidada y préstamos también pasó
(`nested-thread-safety-tests.log`).

El compilador se reconstruyó incrementalmente en A y coexistió con cambios
ajenos sin registrar; las pruebas se ejecutaron directamente y la nueva aún
no está registrada en CMake compartido. Estas comprobaciones son semánticas:
no prueban intercalados concurrentes, todas las formas de captura o arreglos,
ni habilitan la transferencia pública de `Arc` o `WeakArc`.
