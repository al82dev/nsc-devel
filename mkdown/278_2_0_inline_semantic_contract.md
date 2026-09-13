# 278 — Contrato semántico de inlining / Inlining semantic contract

## Resumen / Summary

NativeSharp dispone ahora de una autoridad interna única para distinguir inlining automático,
preferido, obligatorio y prohibido. Esta entrega fija significado, invariantes y nombres estables;
no adelanta la gramática ni el lowering que pertenecen a los ítems siguientes.

NativeSharp now has one internal authority for automatic, preferred, required, and forbidden
inlining. This delivery fixes their meaning, invariants, and stable names without prematurely
adding the grammar or lowering owned by the following items.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/InliningPolicy.h` — declara `InliningPolicy` y las consultas
  canónicas de permiso, solicitud, obligatoriedad y llamada fuera de línea.
- `tests/unit/InliningPolicyTests.cpp` — certifica las cuatro políticas, sus nombres y el rechazo
  fail-closed de valores desconocidos.
- `CMakeLists.txt` — registra `nsc.language.inlining-policy`.
- `docs/1.0.1/` — actualiza roadmap, estado, changelog, especificación, contrato, libro y
  conformidad.

## Diseño y decisiones / Design and decisions

La política es un enum exclusivo, no una colección de flags. Por construcción un callable no puede
ser simultáneamente `forceinline` y `noinline`. El valor automático no equivale a una solicitud:
autoriza al modelo de costo futuro a decidir. `inline` pide pero no viola legalidad; `forceinline`
debe cumplirse para destinos estáticos o producir diagnóstico; `noinline` exige una llamada real.

The policy is an exclusive enum rather than a flag set, making contradictory combinations
unrepresentable. Automatic is permission, not a request. Preferred may never override legality;
required must be honored for static targets or diagnosed; forbidden requires a real call.

El contrato separa optimización de semántica. Ninguna expansión puede cambiar evaluación, efectos,
ownership, cleanups, destrucción, excepciones, dispatch, layout, ABI, exportaciones o identidad
física. Una definición fuera de línea sigue existiendo cuando la ABI o la toma de dirección la
requieren. Los inline sites PDB deben describir expansiones reales.

The contract separates optimization from semantics. Expansion cannot change evaluation, effects,
ownership, cleanup, destruction, exceptions, dispatch, layout, ABI, exports, or physical identity.
An out-of-line definition remains when required by the ABI or address-taking, and PDB inline sites
must represent actual expansions.

## Evidencia dirigida / Focused evidence

La prueba enfocada verifica nombres estables, permiso, solicitud, obligatoriedad y prohibición para
las cuatro políticas. También construye un valor serializado inválido y exige que sea rechazado en
lugar de degradarlo silenciosamente a automático.

The focused test verifies stable names and the permission/request/requirement/forbidden predicates
for all four policies. It also constructs an invalid serialized value and requires fail-closed
handling instead of silently treating it as automatic.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Define el contrato semántico de inlining / Define the inlining semantic contract`

**Descripción / Description**

`Unifica las políticas automática, preferida, obligatoria y prohibida, y fija sus invariantes de semántica, ABI, ownership, excepciones y depuración para las siguientes etapas. / Unifies automatic, preferred, required, and forbidden policies and fixes their semantic, ABI, ownership, exception, and debugging invariants for subsequent stages.`
