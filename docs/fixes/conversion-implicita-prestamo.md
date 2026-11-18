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

## Operadores de coalescencia

Después del primer cambio se comprobó que tanto `result ??= owner.Get()` como
`value ?? owner.Get()` podían invocar un operador implícito a `string` que
escribía la carga. En ambos patrones, Rc y Arc en O0/O2 devolvieron 42 al
observar el campo modificado. La reproducción previa de `??` se conserva en
`A:/nsc-fixes-evidencia/shared-null-coalesce-before.log`; para `??=` el
resultado previo y las cuatro combinaciones quedaron registrados en el
`SCOPE-LOCK.md` de esa carpeta antes de sustituir el log de la ejecución.

Las dos rutas rechazan ahora el operador de usuario si recibe la vista de solo
lectura. Con el compilador aislado build `61eb40a7fe07dbe5`, ambas variantes
se rechazaron para Rc y Arc. Controles con alias mutable terminaron con 42 en
O0/O2 y verificaron que el operador se ejecuta solo cuando el lado izquierdo es
nulo. El consumidor del SDK binario rechazó ambas variantes para Rc y Arc.
Los resultados están en los registros `shared-implicit-implicit_readonly_null_coalesce*.log`,
`shared-implicit-implicit_null_coalesce*control.log` y
`shared-binary-sdk43-check.log`, en `A:/nsc-fixes-evidencia/`.
