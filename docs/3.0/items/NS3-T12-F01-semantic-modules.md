# NS3-T12-F01 — Módulos semánticos

## Español

### Resultado

En edición 3, un proyecto NativeSharp define un módulo semántico: una unidad canónica de
declaraciones, visibilidad y metadata que puede contener cualquier cantidad de archivos. El orden
de los archivos no define el módulo y un `ProjectReference` añade visibilidad de bindings; no
incluye texto ni ejecuta inicializadores durante la importación.

La carga del grafo ahora admite componentes cíclicos de módulos edición 3. El ciclo sólo elimina la
restricción artificial de orden de declaración: el frontend reúne primero los símbolos de todos los
módulos visibles y los modelos de herencia/layout conservan la autoridad para rechazar ciclos no
resolubles. Por ejemplo, dos clases pueden referirse mutuamente, pero dos structs que se contienen
por valor siguen siendo un error de layout.

Edición 2 conserva el diagnóstico histórico de ciclo; no se cambió su compatibilidad.

### Identidad y determinismo

- Los propietarios de módulo fuente y NSMETA se comparan con casing y separadores canónicos de
  Windows; `SharedModule` y `sharedmodule` son una identidad duplicada.
- Las rutas de proyecto se resuelven sin distinguir mayúsculas en Windows.
- El DFS ordena cada componente de forma estable y mantiene el proyecto raíz al final.
- Cada archivo fuente tiene un solo módulo propietario. En grafos acíclicos se conserva el dueño
  dependency-first histórico; en un componente cíclico se usa el orden DFS determinista.
- Cargar el mismo grafo repetidamente produce el mismo dump, base necesaria para fingerprints e
  invalidación incremental reproducible.

### Prueba dirigida

`nsc_ns3_t12_semantic_module_tests` verifica un módulo multi-file, un ciclo de bindings que compila
y ejecuta en O0/NSLink, que importar no ejecuta un inicializador estático sin usar, el rechazo de un
ciclo de layout, identidades duplicadas por casing, rutas Windows case-insensitive, reproducibilidad
y el rechazo compatible de ciclos edición 2. Resultado local: **9/9**.

No se ejecutó baseline, minibaseline ni suite general.

## English

### Result

In edition 3, a NativeSharp project defines a semantic module: a canonical declaration,
visibility, and metadata unit that may contain any number of files. File order does not define the
module, and a `ProjectReference` adds binding visibility; it neither textually includes a file nor
executes initializers while importing.

Graph loading now accepts cyclic edition-3 module components. A cycle only removes the artificial
declaration-order restriction: the frontend first gathers symbols from every visible module, while
inheritance/layout models remain authoritative for unresolvable cycles. Two classes may therefore
refer to each other, whereas two structs containing each other by value remain a layout error.

Edition 2 retains its historical cycle diagnostic; its compatibility was not changed.

### Identity and determinism

- Source and NSMETA module owners are compared under canonical Windows case/separator rules;
  `SharedModule` and `sharedmodule` are duplicate identities.
- Project paths resolve case-insensitively on Windows.
- DFS orders each component stably and keeps the root project last.
- Every source file has one module owner. Acyclic graphs retain historical dependency-first
  ownership; a cyclic component uses deterministic DFS order.
- Repeated loads produce the same graph dump, providing the stable base required by fingerprints
  and reproducible incremental invalidation.

### Directed verification

`nsc_ns3_t12_semantic_module_tests` covers a multi-file module, a binding cycle compiled and run at
O0 with NSLink, import without eager static-initializer execution, layout-cycle rejection,
case-folded duplicate identities, case-insensitive Windows paths, reproducibility, and compatible
edition-2 cycle rejection. Local result: **9/9**.

No baseline, mini-baseline, or general suite was run.
