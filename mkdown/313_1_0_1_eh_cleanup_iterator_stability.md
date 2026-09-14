# 313 — Estabilidad del recorrido de cleanups EH / EH cleanup traversal stability

## Resultado / Result

Se eliminó una invalidación de iteradores de std::vector en el lowering LLVM de
excepciones tipadas. El defecto podía mostrar en una compilación Debug de MSVC:

~~~text
Expression: can't decrement invalidated vector iterator
~~~

En una compilación Release, la misma operación constituía comportamiento indefinido:
el iterador inverso podía leer almacenamiento liberado después de una reasignación.

The LLVM typed-exception lowerer no longer keeps a std::vector reverse iterator
alive while recursively lowering cleanup bodies. Debug MSVC diagnosed the defect;
Release builds could silently read invalidated storage.

## Alcance observado / Observed scope

La ruta afectada se ejecuta cuando ninguna cláusula de un typed catch acepta la
excepción y ésta debe abandonar el try. Antes de relanzarla, el compilador emite
los cleanups envolventes en orden interno-a-externo.

El fallo requiere que uno de esos cleanups vuelva a entrar al lowerer y agregue una
región a activeCleanupRegions_; un finally que contiene otro try/finally es un
caso mínimo y representativo.

The affected path is the unmatched branch of typed-catch dispatch. Before rethrowing,
the compiler emits enclosing cleanup regions from innermost to outermost. A cleanup
body may itself contain another cleanup region, so recursive lowering can grow the
same vector being traversed.

## Causa raíz / Root cause

Archivo:

- src/LLVM/ProjectOopLlvmLowerer.cpp

Función:

- ProjectOopLlvmLowerer::lowerTryRegion

La rama eh.catch.unmatched recorría directamente activeCleanupRegions_ con
rbegin()/rend() y dentro del recorrido llamaba emitCleanupAction(). Para una región
FinallyRegion, esa función llama de nuevo a lowerStatement(). El lowering anidado
puede ejecutar push_back() sobre activeCleanupRegions_, reasignar su buffer e
invalidar el iterador que conservaba el recorrido exterior. El siguiente incremento
del iterador inverso decrementa su iterador base ya invalidado.

The unmatched path traversed activeCleanupRegions_ directly and called
emitCleanupAction() from inside that traversal. A finally cleanup recursively
lowers statements, which can push another cleanup ordinal into the same vector.
Reallocation invalidates the outer reverse iterator; advancing it then operates on
an invalid base iterator.

## Implementación / Implementation

Antes de emitir el primer cleanup se copia la ruta activa:

~~~cpp
const std::vector<std::size_t> unmatchedCleanupPath = activeCleanupRegions_;
~~~

El recorrido inverso usa esa fotografía estable. activeCleanupRegions_ continúa
representando el estado vivo del lowerer y puede cambiar legítimamente durante el
lowering anidado, pero esos cambios ya no invalidan el recorrido exterior.

Before emitting the first cleanup, the active ordinal path is copied. Reverse
iteration uses that stable snapshot, while activeCleanupRegions_ remains the live
lowering state and may legitimately change during recursive lowering.

## Decisión de diseño / Design decision

Se eligió una fotografía y no reserve() porque reservar capacidad sólo ocultaría
el defecto hasta superar otra capacidad. Tampoco se indexa el vector vivo: las
regiones transitorias creadas dentro de un finally no pertenecen a la ruta de
unwind capturada antes de entrar en ese cleanup.

The implementation uses a snapshot rather than reserve(), because capacity is not
a correctness contract. It also avoids indexing the live vector, because transient
regions introduced while lowering a finally body are not members of the unwind path
captured before that cleanup began.

Las otras rutas de cleanup se revisaron en el mismo archivo. Las salidas normales
de bloque y de for copian el ordinal de back(), hacen pop_back() y sólo después
llaman emitCleanupAction(). La cadena excepcional usa ordinales copiados/índices,
y los temporales de expresión completa ya construyen su propio vector antes de
recorrerlo. No requirieron cambios.

Other cleanup paths were reviewed. Normal block and for exits copy and pop the
ordinal before emission; exceptional chains use copied ordinals/indices; full
expression temporaries already snapshot their range. No other path required a
change.

## Prueba enfocada / Focused test

Archivo:

- tests/unit/Stage45TypedCatchRttiTests.cpp

Función agregada:

- testNestedFinallyKeepsUnmatchedCleanupTraversalStable

La prueba construye un proyecto mínimo con:

1. Un typed catch que no coincide.
2. Un finally envolvente que debe ejecutarse antes del relanzamiento.
3. Otro try/finally dentro de ese finally, que hace crecer la pila activa de
   cleanups durante la emisión.
4. Lowering completo a LLVM y verificación del módulo.

Focused result:

~~~text
PASS: 43 checks
~~~

Sólo se compiló y ejecutó nsc_stage45_typed_catch_rtti_tests. No se ejecutó el
baseline ni proyectos externos.

Only the focused typed-catch/EH test target was built and run. No baseline or
external project was executed.

## Mensaje de commit de referencia / Reference commit message

Título en español:

~~~text
Estabiliza el recorrido de cleanups EH anidados
~~~

Descripción en español:

~~~text
Copia la ruta activa de cleanups antes de emitir la rama no coincidente de un typed catch, evitando que un finally anidado invalide el iterador inverso del vector. Agrega una regresión O0 enfocada que baja y verifica el caso typed catch + finally anidado.
~~~

English title:

~~~text
Stabilize nested EH cleanup traversal
~~~

English description:

~~~text
Snapshot the active cleanup path before emitting an unmatched typed-catch branch so nested finally lowering cannot invalidate the vector reverse iterator. Add a focused O0 regression that lowers and verifies the typed-catch plus nested-finally case.
~~~
