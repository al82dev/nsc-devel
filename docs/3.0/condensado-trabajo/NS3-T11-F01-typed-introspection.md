# NS3-T11-F01 — Introspección estática tipada / Typed static introspection

## ES

`ProjectMetaprogrammingModel` se construye desde los modelos productivos de nombres, miembros y
layout. Cada consulta devuelve valores propios de compilación: identidad canónica, clase del tipo,
miembros, firmas y, cuando el layout ya fue autorizado, tamaño y alineación. El orden es el orden
semántico estable. Un consumidor externo sólo observa tipos y miembros públicos; el proyecto dueño
también puede consultar sus declaraciones privadas. No se emiten registros, tablas ni símbolos de
reflection en el binario.

Los genéricos abiertos se distinguen mediante la aridad de su identidad canónica y los tipos
cerrados materializados conservan su propia identidad. Una declaración incompleta sólo publica lo
que los modelos semánticos lograron completar; la introspección nunca inventa miembros.

## EN

`ProjectMetaprogrammingModel` is built from the production name, member, and layout models. Queries
return compile-time values: canonical identity, type kind, members, signatures, and authorized size
and alignment. Ordering follows stable semantic declaration order. External consumers see public
types and members only; the owning project may inspect its private declarations. No reflection
registry, table, or linker-visible symbol is emitted.

Open generics remain distinct through canonical arity and materialized closed types keep their own
identity. Incomplete declarations expose only semantically completed information; introspection
never fabricates members.

## Evidencia / Evidence

`nsc_ns3_t11_f01_typed_introspection_tests` builds a real edition-3 project through object layout
and verifies privacy, stable member order, layout data, and the absence of runtime metadata.
