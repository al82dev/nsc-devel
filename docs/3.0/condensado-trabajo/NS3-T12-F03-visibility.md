# NS3-T12-F03 — Visibilidad granular / Granular visibility

## Español

La edición 3 distingue cuatro fronteras semánticas:

- `private`: sólo la declaración/contenedor autorizado;
- `module`: el proyecto que declara el símbolo;
- `package`: cualquier proyecto con el mismo `<Package>` lógico;
- `public`: cualquier consumidor que tenga la referencia.

`internal` continúa siendo compatible y equivale a `module`. Los tipos de namespace aceptan los
modificadores contextuales `module` y `package`; no se añadieron keywords globales que pudieran
romper identificadores 2.x. El parser conserva la frontera en el AST y `ProjectNameLookup` la
aplica al seleccionar candidatos entre proyectos.

Un productor puede declarar acceso friend verificable con `<Friend Include="Modulo" />`. La
comparación de Package y Friend es determinista e insensible a mayúsculas bajo las reglas Windows
x64. Friend no publica el símbolo ni cambia su ABI: sólo autoriza al módulo nombrado durante el
binding desde fuente. Un consumidor precompilado sigue viendo exclusivamente la superficie
publicada en NSMETA.

El mismo `ProjectNameLookup` es usado por código normal, árboles generados y fases de
metaprogramación posteriores; no existe una ruta de lookup que omita esta frontera. Los errores
distinguen una identidad Package diferente de un contrato Friend ausente.

Validación puntual: `nsc_ns3_t12_visibility_tests`, 6/6 checks. Compila y ejecuta a O0/NSLink un
consumidor del mismo paquete y otro autorizado por Friend, y rechaza un paquete externo.

## English

Edition 3 distinguishes four semantic boundaries: `private`, `module`, logical `package`, and
`public`. Existing `internal` remains compatible and means `module`.

Namespace types accept contextual `module` and `package` modifiers. The parser retains that
boundary in the AST and `ProjectNameLookup` enforces it while selecting cross-project candidates.
Projects declare logical identity through `<Package>` and auditable friend access through
`<Friend Include="Module" />`. Windows x64 package/friend comparison is deterministic and
case-insensitive.

Friend access neither exports the declaration nor changes ABI. It authorizes only the named
source module; precompiled consumers still receive the public NSMETA surface. Normal source,
generated trees, and later metaprogramming phases share the same lookup model.

Focused validation: `nsc_ns3_t12_visibility_tests`, 6/6 checks. It compiles and executes an O0
same-package consumer and a friend consumer through NSLink, then rejects an external package.
