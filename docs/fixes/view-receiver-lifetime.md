# Vida de vistas devueltas por métodos

La reproducción `smart-pointer-view-probe/Main.ns` asignaba a un local exterior
una vista devuelta por `strong.Borrow()`, destruía `strong` y después accedía
al payload. El análisis de cuerpos la aceptaba. El usuario aprobó ligar
conservadoramente la vista al receptor del método.

El análisis registra como raíz el receptor local de una llamada de instancia
que devuelve un tipo vista. Rechaza devolver la vista cuando ese local muere
al retornar, asignarla a un local de mayor duración y mover el propietario
mientras vive la vista. También rechaza receptores temporales creados con
`new` o devueltos por una llamada ordinaria; admite el encadenamiento de
vistas cuyo origen sea un propietario que sigue vivo. En ausencia de un
contrato explícito de vida para estas vistas, la regla puede rechazar una
vista global devuelta por un método de instancia.

La construcción directa de una vista con argumentos `ref` a locales
también conserva la raíz local, aunque sea un valor `struct`: para varios
argumentos directos se usa el ámbito más interior. La revisión base aceptaba
guardar la vista fuera del ámbito del valor y la rama corregida la rechaza
(`shared-value-view-compare/results.json`). La prueba dirigida
`nsc.compiler.shared-value-view-lifetime` ejecuta el caso válido O0/O2 y
comprueba escapes por asignación y retorno, movimiento del valor y dos
argumentos con ámbitos distintos (`shared-view-tests.log`). Esta regla es
conservadora: un constructor que recibe un `ref` y no lo conserva puede ser
rechazado al escapar su vista. Solo se comprobó procedencia local directa;
esto no demuestra toda expresión indirecta, raw pointer o contrato entre
módulos.
La variante `in` no se incluyó en esta corrección: la reproducción también
falla al convertir la dirección de solo lectura al campo mutable, por lo que
no aisló una vista válida (`shared-value-view-in-probe/results.json`).

`nsc.compiler.view-receiver-lifetime` comprueba los rechazos por asignación,
retorno, movimiento y temporales; ejecuta un caso válido y un encadenamiento.
La prueba existente `nsc.stage58.view-lifetimes` también pasó. Comandos y
salidas en `A:/nsc-fixes-evidencia/view-lifetime-tests.log` y
`A:/nsc-fixes-evidencia/view-lifetime-check/results.json`.

**Límite pendiente:** extraer una referencia de objeto con `view.Get()` y
guardarla en un local que sobrevive al propietario sigue siendo aceptado
(`view-lifetime-check/payload-leak/Probe.nsproj` y `results.json`). Esta
reproducción declara el campo y el retorno como referencias ordinarias. Una
variante con campo y retorno `borrowed[a]` rechaza la asignación a un local
ordinario y ejecuta el uso válido; véase `borrowed-field-return.md`. Esto
no demuestra una API `Rc` segura ni resuelve todos los escapes de ese
objeto, el control block, las familias atómicas o el pin del módulo. Tampoco
se afirma cobertura de todas las formas posibles de receptor y composición.
