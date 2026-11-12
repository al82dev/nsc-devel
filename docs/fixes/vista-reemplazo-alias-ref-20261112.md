# Vista activa y reemplazo a través de un alias ref

## Estado

**Reproducido en análisis semántico; corrección pendiente.** No se conoce un
ejecutable anterior verificado con el que atribuir el hallazgo a un commit.
No se han ejecutado los programas potencialmente inseguros.

## Reproducción

El compilador acepta reemplazar mediante un local `ref Owner` el dueño de una
vista viva. La asignación directa equivalente al mismo dueño se rechaza con un
diagnóstico de préstamo activo. El resultado se observó con origen directo de
la vista y con `owner.Borrow()`, con y sin un local homónimo en un scope
interior, y con una cadena de tres représtamos.

El volcado MIR de `negative_known_replace_alias_no_shadow` registra un
`reference-local` inicializado desde `local-address owner`, seguido de
`store Owner` hacia el puntero del alias. El volcado de cuerpos de la cadena
de tres représtamos conserva `frame-local:0`, `borrow-place=L0` y la
cadena de fuentes. La evidencia está en
`A:/nsc-fixes-evidencia/vistas-shadow-riesgos-20261112/resultados-comparacion/known-alias.mir.txt`
y `resultados-reborrow/reborrow-bodies.txt` bajo la misma carpeta.

El nsc de la primera comparación tuvo SHA-256
`4aedad5ddd323493d549492da5a649ec23bacae39096fc2e922c01731780211d`;
el de la tanda ampliada tuvo
`d6f7c8e534461a4df3394ae98d09a6ba9e26ebc24a3dc7de72a53efba88b38d1`.
Cada JSON conserva hashes de las fuentes, comandos, cwd, stdout, stderr y
hashes del ejecutable antes/después. Las dos tandas usaron ejecutables
diferentes; no constituyen comparación antes/después de una corrección.

`resultados-reborrow/results.json` registra siete controles positivos
aceptados, seis reemplazos por alias/représtamo aceptados inesperadamente,
dos reemplazos directos rechazados y cuatro escapes por `ref/out` rechazados.
Los positivos incluyen ausencia de vista, vista fuera de scope, dueño homónimo
distinto, campo no relacionado y représtamo sin vista.

## Límites y siguiente paso

El volcado semántico resuelve `Borrow` sobre el local `owner`; por
inspección, la recolección de fuentes del método de instancia registra esa raíz
como `Unknown`. El volcado no imprime directamente el vector, y la aceptación
por alias también se observa con origen directo. No debe atribuirse el hallazgo
a `Unknown` ni al shadowing sin una prueba adicional.

`AssignmentAnalyzer.cpp` identifica el local LHS y consulta la protección de
dueño para ese ordinal; `referenceEscapeOrigin` y `referencePlaceKey` del
alias ofrecen una posible ruta hacia el almacenamiento real. La propuesta
estática, sus controles de représtamo y el peligro de contar el propio alias
como conflicto están en
`A:/nsc-fixes-evidencia/vistas-shadow-riesgos-20261112/DISENO.md`. Quedan por
resolver explícitamente procedencia desconocida, proyecciones, parámetros,
uniones y binarios. No hay solución productiva aplicada ni prueba general de
seguridad.
