# 284 — Inlining de structs e interfaces / Struct and interface inlining

## Resumen / Summary

Los métodos concretos de struct ya obedecen las políticas de inlining sin boxing. Las llamadas de
interfaz se devirtualizan sólo cuando una conversión directa contiene una construcción de clase
fresca cuyo tipo dinámico e implementación son demostrables; los receptores polimórficos permanecen
dinámicos.

Concrete struct methods now honor inlining policies without boxing. Interface calls devirtualize
only when a direct conversion contains a fresh class construction whose dynamic type and
implementation are provable; polymorphic receivers remain dynamic.

## Archivos y funciones / Files and functions

- `src/Frontend/ProjectOopHirModel.cpp`, rutas de llamada resuelta — reconoce conversiones exactas,
  consulta `ProjectInterfaceModel::tablePlan()` y sustituye contrato/dispatch por implementación
  directa.
- `tests/unit/StructInterfaceInliningTests.cpp` — cubre struct `forceinline`, interfaz exacta y
  fallback polimórfico.
- `CMakeLists.txt` — registra `nsc.language.struct-interface-inlining`.
- `docs/1.0.1/` — actualiza especificación, contrato, mapas, libro, estado y roadmap.

## Implementación y decisiones / Implementation and decisions

La devirtualización se realiza antes de crear el nodo HIR de llamada. El contrato de interfaz y el
tipo concreto se cruzan en la tabla semántica ya usada para construir el interface map; así no se
reimplementa la búsqueda de métodos ni se supone coincidencia por nombre. El nodo resultante lleva
el ordinal de la implementación y atraviesa la ruta directa existente, donde `alwaysinline` puede
actuar normalmente.

Devirtualization occurs before creating the HIR call node. The interface contract and concrete type
are matched through the semantic table already used to build the interface map, avoiding a second
name-based method lookup. The resulting node carries the implementation ordinal and follows the
existing direct-call path, where `alwaysinline` can act normally.

La prueba de tipo exacto es deliberadamente estrecha: `new Clase()` produce exactamente esa clase y
la conversión inmediata no puede observar otra implementación. No se propaga esa certeza a través
de locals ni fields; hacerlo correctamente requiere análisis de flujo y aliasing. Esos casos siguen
usando el dispatch dinámico completo y la prueba negativa lo verifica.

The exact-type proof is deliberately narrow: `new Class()` produces exactly that class, and its
immediate conversion cannot observe another implementation. Certainty is not propagated through
locals or fields because doing so correctly requires flow and alias analysis. Those cases retain
full dynamic dispatch, verified by the negative control.

## Evidencia dirigida / Focused evidence

La llamada de struct y la llamada de interfaz exacta nombran sus implementaciones físicas antes de
optimizar y desaparecen bajo el inliner obligatorio O0. Un segundo proyecto con dos implementadores
y un parámetro de interfaz conserva `interface.map.inrange`.

The struct call and exact-interface call name their physical implementations before optimization
and disappear under mandatory O0 inlining. A second project with two implementations and an
interface parameter retains `interface.map.inrange`.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Inlinea structs y destinos de interfaz demostrados / Inline structs and proven interface targets`

**Descripción / Description**

`Conserva políticas en métodos de struct, devirtualiza construcciones de clase convertidas directamente a interfaz y mantiene dispatch dinámico para receptores polimórficos. / Preserves policies on struct methods, devirtualizes class constructions directly converted to interfaces, and retains dynamic dispatch for polymorphic receivers.`
