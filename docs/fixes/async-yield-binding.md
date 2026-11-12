# Bindings semánticos de valores producidos por async yield

`tests/unit/AsyncYieldOwnershipBindingTests.cpp` verifica la información
semántica asociada a los operandos de `AsyncYieldValue`: nueve formas de
valores y referencias, un alias de clase con dueño local y cuatro yields de
dos especializaciones genéricas. Comprueba unicidad del binding por sintaxis
y callable, categorías de valor, transferencia, origen, tipo y formas HIR de
referencia. Las especializaciones `int` y `Payload` tienen cada una dos
yields y recuperan su binding con el ordinal del callable correspondiente.

En particular, un alias a una clase cuyo local es owned tiene ownership
`Owned`, pero no transfiere la propiedad: descartarlo como dueño basándose
sólo en ese campo podría provocar doble destrucción. Las construcciones nuevas
de clase y de estructura también difieren en los indicadores `fresh` y
categoría. El sondeo inicial y la prueba con aserciones son evidencias de las
formas examinadas, no una clasificación universal de tipos o expresiones.

La compilación incremental dirigida y ejecución se registran en
`A:/nsc-fixes-evidencia/async-yield-binding-owned-alias-assert-build.log` y
`async-yield-binding-owned-alias-assert.log`; el primer intento con una
expectativa incorrecta para el alias se conservó en
`async-yield-binding-owned-alias.log`. La prueba con la expectativa corregida
terminó con salida 0. Tras exigir exactamente un yield por fixture individual,
el build y la ejecución dirigidos volvieron a pasar con salida 0
(`async-yield-binding-final-build.log` y `async-yield-binding-final.log` en la
misma carpeta). Este test comprueba representación y bindings; no
ejecuta los iteradores ni acredita destrucción, cancelación, suspensión ni el
descarte seguro del resultado de un yield.
