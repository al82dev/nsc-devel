# Commit Tanda 44 / SH42

Estado: `LANDED`.

## Título

`selfhost(tanda-44): certify nsc0 and nsc2 functional equivalence`

## Alcance

- gate diferencial sobre las imágenes `nsc2` reales de SH41;
- nueve capas: diagnósticos, frontend, semántica, IR, COFF, metadata, productos y proyectos;
- matriz O0–O3 con LLD y NSLink;
- fixture común de proyecto y contrato versionado;
- 477/477 CTest, 148/148 fixtures y 18/18 triage en cada enlazador;
- Gate 0 `LANDED`, cero omitidos y cero problemas.

## Cuerpo

Certify the self-hosted nsc2 images against the nsc0 observable contract instead of rebuilding
candidates with the seed compiler. Reuse the established frontend, semantic and IR differential
oracles, add direct AMD64 COFF inspection, dual-linker execution, project model comparison, metadata
header validation and product planning checks. Keep normalization comparison-only and preserve every
original artifact. Record the clean dual-linker baseline and make promotion readiness depend on SH42.
