# Referencias en elementos de generadores async de namespace

En `A:/nsc-fixes-evidencia/namespace-yield-reference-20261112/runtime-cases2`
se conservaron cuatro programas de control: generador miembro o de namespace,
con elemento `ref int` o `ref mut int`. Usan un local del generador, avanzan
dos veces y observan su identidad y valor; el caso mutable escribe a través
de la referencia. Con el compilador anterior SHA-256
`4aedad5ddd323493d549492da5a649ec23bacae39096fc2e922c01731780211d`, los
cuatro controles de clase devolvieron 42 en O0/O2, mientras las cuatro
ejecuciones de namespace terminaron con `3221225477`. Todas compilaron.
Los logs conservan fuentes, comandos, directorio de ejecución y resultados.

El HIR del miembro bajaba el operando como dirección; el de namespace lo
bajaba como valor. `initializeCoroutineValueStatementFrame` solo consultaba
ordinales de miembros para reconocer que el elemento era una referencia.
Ahora consulta `callableSymbol`, que también resuelve el adaptador de una
función de namespace, y mantiene el cálculo existente para `ref` y `ref mut`.

Tras compilación incremental, el compilador aislado tuvo SHA-256
`d6f7c8e534461a4df3394ae98d09a6ba9e26ebc24a3dc7de72a53efba88b38d1`.
`tests/integration/async_namespace_reference_items.py` conservó los mismos
cuatro programas y verificó sus resultados: ocho compilaciones y ejecuciones
O0/O2 devolvieron 42. Los comandos y salidas están en
`A:/nsc-fixes-evidencia/namespace-yield-reference-after-20261112`, con el
resumen `namespace-reference-after-20261112.log`. El runner se ejecutó desde
la copia integradora con runtime y temporales en A.

`cancel-drop-after-ref-fix-20261112.log` registra 18/18 controles de
cancelación/abandono con salida 42. El runner de generadores async de namespace
conservó sus controles aprobados y sus dos fallos conocidos de compilación
`class_yield_break_catch_cleanup_throws` en O0/O2; véase
`namespace-after-ref-fix-20261112.log`. Esta corrección no resuelve ese fallo
SEH ni la cancelación anidada, y no acredita todas las formas de lifetime.
