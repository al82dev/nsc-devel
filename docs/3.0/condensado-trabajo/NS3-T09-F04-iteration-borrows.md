# NS3-T09-F04 — Views and iteration borrows

## Español

### Resultado

La iteración por referencia de NativeSharp 3 mantiene un préstamo léxico del almacenamiento fuente
durante todo el `foreach`:

```ns
foreach (ref int item in source) { /* préstamo compartido */ }
foreach (ref mut int item in source) { /* préstamo mutable de elementos */ }
```

El préstamo comienza después de evaluar una sola vez el source y termina al salir del loop, incluso
por `break`, `return` o cleanup estructurado. Su lugar de origen es la expresión original, no el
snapshot oculto que usa el lowering.

### Reglas

- `ref` permite observar elementos, pero impide escribir directamente el source o sus elementos.
- `ref mut` permite escribir mediante el binding de elemento y admite un acceso indexado compatible.
- Ninguno de los dos modos permite reemplazar el almacenamiento fuente durante el recorrido.
- Una llamada de instancia mutante sobre el source se rechaza porque podría invalidar las
  referencias ya producidas. Los miembros `readonly` permanecen disponibles.
- El préstamo desaparece al terminar el loop; después puede reemplazarse el source normalmente.
- La iteración por valor conserva su snapshot evaluado una vez y no inventa un préstamo de
  elementos.

Estas reglas usan los place keys y scopes del borrow checker existente. No existe un segundo modelo
de aliasing específico para iteradores. Arrays, `Span<T>` y los iteradores estructurales convergen en
la misma regla una vez que `foreach` seleccionó el candidato tipado.

### Archivos

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: pila de préstamos de iteración activos.
- `src/Frontend/ProjectMethodBodyModel.cpp`: apertura/cierre léxico y rechazo de asignaciones o
  llamadas mutantes incompatibles.
- `tests/unit/NativeSharp3T09IterationBorrowTests.cpp`: comprobación dirigida O0/NSLink.
- `CMakeLists.txt`: objetivo aislado `nsc_ns3_t09_iteration_borrow_tests`.

### Evidencia puntual

La prueba dirigida pasó 7/7 comprobaciones con O0 y NSLink: escritura por `ref mut`, préstamo
readonly, reemplazo del source, liberación al salir, snapshot por valor, miembro readonly y miembro
mutante. No se ejecutó baseline ni suite general.

## English

NativeSharp 3 reference iteration now holds a lexical loan on the original source storage for the
entire `foreach`. A shared `ref` iteration rejects source writes; `ref mut` permits writes through
the generated element binding and compatible indexed element access. Both modes reject source
replacement and mutating receiver calls that could invalidate yielded references, while readonly
members remain callable.

The loan is pushed only while the selected typed foreach candidate is analyzed and is removed at
loop exit. It reuses the compiler's existing canonical places and reference-local semantics rather
than creating an iterator-specific borrow checker. The dedicated O0/NSLink executable passed all
7 checks; no baseline or general suite was run.
