# Entrega 230 — Lectores de entradas hostiles / Hostile-input readers

## Español

### Alcance

Se endurecieron los cinco límites donde datos externos podían provocar reservas grandes o recursión
antes de ser validados: fuentes, proyectos XML, bibliotecas COFF, PDB y NSMETA. NSMETA ya tenía un
límite de payload de 128 MiB y se mantuvo como autoridad existente.

### Implementación

- `SourceManager` consulta el tamaño antes de reservar, limita cada fuente a 64 MiB, lee exactamente
  esos bytes y rechaza truncamiento o crecimiento concurrente.
- `ProjectLoader` aplica el mismo patrón con 4 MiB para `.nsproj`.
- `SimpleXmlParser` transporta profundidad explícita y rechaza más de 128 elementos anidados antes
  de hacer la siguiente llamada recursiva.
- `CoffArchiveVerifier` limita el linker member inspeccionado a 256 MiB y su tabla a 16,777,216
  símbolos antes de `reserve`.
- `PdbVerifier` rechaza MSF mayores de 1 GiB antes de materializar el archivo completo.
- `ProjectLoaderTests` agrega el caso de anidación adversaria y exige el diagnóstico de límite.
- El contrato del compilador agrega `NSC-INPUT-001` y su mapa de conformidad conecta lectores y
  evidencia.

Los límites son de seguridad del toolchain, no semántica del programa ni ABI. Se eligieron muy por
encima de fuentes/proyectos ordinarios y por debajo de tamaños que podrían agotar una máquina de
desarrollo. Todos los cálculos de rangos existentes en NSMETA, COFF y PDB permanecen activos.

## English

### Scope

The five boundaries where external data could trigger large allocations or recursion before
validation were hardened: sources, XML projects, COFF archives, PDB files, and NSMETA. NSMETA's
existing 128 MiB payload ceiling remains authoritative.

### Implementation

`SourceManager` and `ProjectLoader` size first, enforce 64 MiB and 4 MiB ceilings, read exactly that
amount, and reject concurrent truncation or growth. `SimpleXmlParser` rejects nesting beyond 128
elements before the next recursive call. The COFF verifier bounds the inspected linker member at
256 MiB and its symbol count at 16,777,216 before reserve. The PDB verifier bounds a materialized
MSF file at 1 GiB. Project tests cover hostile nesting, while compiler contract clause
`NSC-INPUT-001` records the complete policy and evidence.

These are toolchain safety limits, not program semantics or ABI. Existing per-field range checks
remain in force. The queued tests run only in the final baseline after delivery 234.
