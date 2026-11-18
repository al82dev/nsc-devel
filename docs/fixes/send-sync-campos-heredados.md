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

La regresión se amplió con una biblioteca productora independiente y un
consumidor que deriva de sus clases. Para un campo `int*` público o privado en
la base binaria, ambas rutas rechazaron la base y la derivada; los controles
con base segura se aceptaron. La matriz pasó tanto con Core desde fuentes como
con Core binario (`thread-inherited-fields/regression.log` y
`regression-binary.log`). La primera ejecución de la regresión ampliada no
encontró el runtime predeterminado; se corrigió el comando para usar el runtime
aislado de A y se repitió. Estas pruebas siguen siendo semánticas.

Una tercera biblioteca, con un campo `ref mut int`, no pudo generarse: la
verificación de metadata reportó que no puede materializar
`refmut:builtin:int`. El productor no llegó a un consumidor; el diagnóstico
se conserva en `thread-inherited-binary/mutable-producer-failure.json`. Esta
limitación se corrigió después para campos nominales; la prueba binaria y sus
límites están en `metadata-campo-ref-mut.md`.
