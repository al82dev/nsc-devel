# Conversión implícita de una carga prestada de solo lectura

Una clase `CastPayload` declara `implicit operator int(CastPayload value)` y
escribe `value.Data`. Antes del cambio, `int result = owner.Get()` compilaba;
el operador modificó la carga de `Rc` y de `Arc` en O0 y O2. Las cuatro
ejecuciones devolvieron 42 al comprobar el valor modificado. La reproducción
y los comandos están en `A:/nsc-fixes-evidencia/shared-implicit-conversion-before.log`.
La asignación simple `result = owner.Get()` también pasó el análisis semántico;
`shared-implicit-implicit_readonly_assignment.log` conserva esa comprobación.

El compilador ya rechazaba el operador explícito de usuario sobre la misma
vista. Ahora aplica la misma regla conservadora a la inicialización de un local
y a una asignación simple cuando la conversión implícita selecciona un operador
de usuario y el origen es un objeto prestado de solo lectura. La conversión no
posee actualmente un contrato que demuestre que su implementación respeta esa
restricción, por lo que también se rechazan operadores que podrían ser puros.
Las conversiones intrínsecas y las realizadas desde un alias mutable conservan
su comportamiento.

Con `A:/nsc-fixes-evidencia/build/bin/nsc.exe`, build `5fc1242110d0dd0b`, la
selección `shared-readonly-cast-check.cmd` pasó para Rc y Arc: inicialización,
asignación y elemento de arreglo rechazan el operador mutante; el retorno,
argumento, propiedad y condición ensayados también se rechazan por sus reglas
correspondientes. Las conversiones explícitas de control no permiten escribir
la carga. El operador implícito sobre un objeto mutable y la lectura de un
campo escalar terminaron con 42 en O0 y O2. Registro
`A:/nsc-fixes-evidencia/shared-readonly-cast-check.log`.

El consumidor de `A:/nsc-fixes-evidencia/generic-owner-sdk43/NativeSharpCore.lib`
rechazó las dos formas nuevas para Rc y Arc, además de pasar sus controles de
propiedad y fábricas (`shared-binary-sdk43-check.log`). Estos controles no
demuestran todos los contextos posibles de conversión implícita, la seguridad
de alias escapados desde constructores ni el contrato Send/Sync de Arc.
