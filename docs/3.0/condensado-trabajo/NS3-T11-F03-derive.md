# NS3-T11-F03 — Derivación estática / Static derive

## ES

`derive(Equality|Hash|Print|Clone)` atraviesa el parser y los modelos semánticos productivos de
records. `ProjectMetaprogrammingModel` materializa un registro tipado por generador con la identidad
del record, fields participantes, miembros que el generador puede publicar y una clave incremental
estable. `#[NativeSharp.DeriveIgnore]` excluye un field sin eliminarlo del layout ni de su semántica
ordinaria. La salida se limita a los miembros declarados por cada contrato de derive.

El orden canónico es Equality, Hash, Print y Clone, independiente del orden interno de hashes o de
la ejecución paralela. La clave incluye tipo, trait y campos tipados; por ello cualquier cambio de
entrada invalida exactamente el derive afectado. Duplicados y derives desconocidos siguen
diagnosticándose en el span fuente por el parser. Los generadores integrados no aceptan dependencias
entre derives, de modo que no existe un ciclo oculto que ordenar.

## EN

`derive(Equality|Hash|Print|Clone)` flows through the production record parser and semantic models.
`ProjectMetaprogrammingModel` materializes one typed generator record containing the record identity,
participating fields, the members that generator may publish, and a stable incremental key.
`#[NativeSharp.DeriveIgnore]` excludes a field without removing it from ordinary layout or semantics.
Output is restricted to each derive contract's declared members.

Canonical order is Equality, Hash, Print, Clone regardless of hash iteration or parallel execution.
The key includes type, trait, and typed fields, so an input change invalidates exactly the affected
derive. Duplicate and unknown derives remain source-mapped parser errors. Built-in generators do not
accept dependencies on other derives, eliminating hidden generator cycles.

## Evidencia / Evidence

`nsc_ns3_t11_f03_derive_tests` compiles a real record, verifies ignored fields, deterministic output,
restricted generated members and dependency keys, and checks duplicate source diagnostics.
