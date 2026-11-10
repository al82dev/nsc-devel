# Send y Sync de campos heredados

Una clase `RawChild` sin campos propios, derivada de `RawBase` con `int*`,
se aceptaba para las restricciones `Send` y `Sync`, aunque `RawBase` se
rechazaba. Una derivada de `MutBase`, con un campo `ref mut int`, se aceptaba
para `Sync` aunque su base se rechazaba. Las mismas derivadas se aceptaban
como capturas compartidas de una tarea; las bases inseguras se rechazaban.
Los comandos y diagnósticos anteriores están en
`A:/nsc-fixes-evidencia/thread-inherited-fields/before.json` y
`capture-before.json`. No se ejecutaron las tareas.

El recorrido de restricciones y los recorridos `callTypeIsSend` y
`callTypeIsSync` solo revisaban miembros declarados por el tipo actual. Ahora
consultan `ProjectInheritanceModel` y añaden la base inmediata al recorrido,
que ya evita repetir tipos visitados. La prueba
`tests/integration/thread_inherited_safety.py` comprueba ambas rutas con bases,
derivadas, una cadena de dos niveles, controles seguros y la distinción entre
Send y Sync. Pasó con Core desde fuentes y con el SDK binario aislado
(`thread-inherited-fields/regression.log` y `regression-binary.log`). También
pasaron los controles dirigidos anteriores de arreglos, tuplas, referencias
mutables y seguridad anidada en `A:/nsc-fixes-evidencia`.

Los tipos derivados de la prueba pertenecen al consumidor fuente; usar Core
binario no demuestra bases precompiladas ni derivados cargados después. Las
pruebas son semánticas y no acreditan toda la seguridad de clases con alias
mutables o de las familias públicas `Arc`/`WeakArc`. El build fue incremental
en A y coexistió con cambios ajenos sin registrar; la prueba nueva se ejecuta
directamente mientras el CMake compartido tiene cambios ajenos.
