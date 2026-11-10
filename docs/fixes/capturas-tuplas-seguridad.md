# Seguridad de capturas con tuplas

Una estructura con un campo tupla `(Raw, int)`, donde `Raw` contiene `byte*`,
se aceptaba en una captura por referencia enviada a un ejecutor de hilos.
También se aceptaba una tupla con un elemento que contiene `ref mut int`,
que es Send pero no Sync. Un contenedor de una tupla con elementos seguros
era aceptado. Los comandos y resultados previos están en
`A:/nsc-fixes-evidencia/thread-tuple-capture/before.json`; la regresión falló
antes de la corrección (`regression-before.log`). No se ejecutaron las tareas.

El recorrido de `callTypeIsSend` y `callTypeIsSync` no descendía por claves
de tupla. Ahora utiliza `ProjectTypeConversion::tupleElementTypeKeys` y
procesa los elementos en el mismo recorrido que las estructuras y arreglos.
La prueba `tests/integration/thread_tuple_capture.py` rechaza las capturas
inseguras, incluida una tupla anidada, y acepta el control seguro. Pasó con
Core desde fuentes y con el SDK binario aislado (`regression.log` y
`regression-binary.log`). La regresión anterior de arreglos también pasó.
Todos los logs están bajo `A:/nsc-fixes-evidencia/thread-tuple-capture` salvo
el de arreglos, que está bajo `thread-array-capture`.

Los intentos de declarar una tupla directamente como parámetro `ref` fueron
rechazados antes de esta comprobación con `argument 1 ... is not declared
'ref'`; no son evidencia de seguridad de la captura directa. El build
incremental coexistió con cambios ajenos sin registrar, y la prueba nueva se
ejecuta directamente mientras CMake compartido contiene cambios ajenos. No se
ha acreditado concurrencia general ni habilitado la transferencia de `Arc`.
