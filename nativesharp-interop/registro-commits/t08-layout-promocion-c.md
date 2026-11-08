# Layout de miembros C promovidos

Tras seleccionar los tipos anónimos internos, `ProbeAnonymous` alcanzó el cálculo
de layout y recibió `invalid C++ instance field layout`. El recorrido de campos C
trataba el alias promovido como otro campo físico, aunque la promoción limpia su
offset y el layout del alias se calcula después mediante `buildPromotedFieldLayouts`.

El recorrido de campos físicos omite los aliases promovidos; no modifica los
campos físicos ni las reglas de layout del agregado. La corrección aún no se ha
compilado ni probado; el caso y la tanda 8 siguen abiertos.
