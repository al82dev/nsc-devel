# T08: resultado inicial de conversión flexible

Con los cambios hasta `30f066270`, la construcción incremental de `nsc`
terminó con código 0. `validar-decay.cmd` pasó en O0–O3: inicialización,
asignación, argumento y retorno usaron el almacenamiento C original; el
control de dirección empaquetada se rechazó. `validar.cmd` pasó su regresión
O0–O3 y la sonda anterior `ProbeDecay` compiló y ejecutó.

Esto acredita el caso `int` no calificado en consumo directo. Faltan elementos
const/volatile, otros tipos, rutas precompiladas, otras expresiones y la
validación integral de la tanda. No se cierra ningún ítem por esta muestra.
