# NS304-T13-F07 — Provenance intermodular/DLL

- **Estado**: AUDITADO (metadata de allocator sin contrato de deallocator; consumer sin validación)
- **Rama**: development

## Auditoría (2026-10-14)

- El metadata público no publica el **contrato de deallocator/provenance** del allocator, y no hay
  validación de que el consumer destruya con el allocator correcto (mismatch antes de corrupción) —
  el plumbing intermodular del deallocator, depende de F01.
