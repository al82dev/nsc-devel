# NS304-T13-F07 — Provenance intermodular/DLL

- **Estado**: AUDITADO (metadata de allocator sin contratos; consumer deallocator sin validación)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar provenance intermodular/DLL*.
Plan T13-F07 (DeepSeekDocs): *publicar contratos necesarios para que consumer destruya con el
allocator correcto; probar allocate en producer/deallocate en owner autorizado, DLL unload y mismatch
rechazado antes de corrupción*.

Estado real:

- El metadata público (LibraryMetadata) no publica el contrato de deallocator/provenance del allocator.
- No hay validación de que el consumer destruya con el allocator correcto (mismatch antes de
  corrupción).

## Gap

Falta publicar el contrato de provenance del allocator en NSMETA y validar que el consumer destruya
con el deallocator correcto (allocate producer / deallocate owner autorizado + DLL unload + mismatch
rechazado).

## Nota

F07 depende de F01 (provenance) + el metadata existente. No es una surface nueva; es el plumbing
intermodular del deallocator.
