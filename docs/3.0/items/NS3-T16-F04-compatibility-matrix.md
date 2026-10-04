# NS3-T16-F04 — Matriz source/ABI / Source and ABI compatibility matrix

## Español

`nsc --migration-compatibility-matrix <viejo.nsmeta> <nuevo.nsmeta>` carga y valida ambos artefactos
NSMETA con el codec del compilador, ejecuta el diff de API pública y emite `NSMIGRATION-MATRIX 1`.
Cada cambio recibe dos ejes independientes: `source-compatible`/`source-breaking` y
`abi-neutral`/`abi-breaking`, además de la decisión `consumer-rebuild`.

La cabecera conserva seis huellas: source vieja/nueva, superficie ABI vieja/nueva y fingerprint
ABI NSMETA viejo/nuevo. Así una clasificación nunca queda separada de los artefactos exactos que
la produjeron. Un cambio aditivo no fuerza rebuild; una ruptura source o ABI sí.

## English

`nsc --migration-compatibility-matrix <old.nsmeta> <new.nsmeta>` loads and validates both NSMETA
artifacts through the compiler codec, runs the public API differ, and emits
`NSMIGRATION-MATRIX 1`. Every change receives independent source and ABI classifications plus a
`consumer-rebuild` decision.

The header records old/new source fingerprints, old/new ABI-surface fingerprints, and old/new
NSMETA ABI fingerprints. A classification therefore cannot be detached from the exact artifacts
that produced it. Additive changes do not force a rebuild; source or ABI breaks do.

## Validación dirigida / Targeted validation

Construcción y ejecución directa de `nsc_ns3_t16_migration_tests`; sin CTest ni baseline.
