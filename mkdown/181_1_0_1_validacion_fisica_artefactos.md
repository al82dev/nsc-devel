# 1.0.1 — Validación física de artefactos / Physical artifact validation

## Resultado / Outcome

**ES.** DLL-P6 convierte la robustez negativa del contrato DLL en pruebas de
artefactos completos. El verificador rechaza los archivos antes de enlazar o
ejecutar y conserva diagnósticos específicos para el miembro, máquina, tipo de
imagen, export o tabla incoherente.

**EN.** DLL-P6 turns negative DLL-contract robustness into complete-artifact
tests. The verifier rejects files before linking or execution and preserves
specific diagnostics for the member, machine, image kind, export, or
inconsistent table involved.

## Matriz negativa / Negative matrix

- archive truncado e índice del linker con offsets ilegibles;
- PE válido sin `IMAGE_FILE_DLL` y DLL que anuncia máquina i386;
- import library perteneciente a otro producto;
- exports faltantes e inesperados contra NSMETA;
- NSMETA con checksum y fingerprint válidos pero tablas de dispatch
  incoherentes;
- límites de payload, strings y conteos comprobados antes de reservar;
- identidad de módulo duplicada y export C duplicado;
- ausencia de ejecutable o metadata parcial después de rechazo.

- truncated archive and linker index with unreadable offsets;
- valid PE without `IMAGE_FILE_DLL` and a DLL advertising the i386 machine;
- import library belonging to another product;
- missing and unexpected exports against NSMETA;
- NSMETA with valid checksum and fingerprint but inconsistent dispatch tables;
- payload, string, and record-count limits checked before allocation;
- duplicate module identity and duplicate C export;
- no partial executable or metadata after rejection.

## Evidencia / Evidence

```text
nsc_stage48_2_library_metadata_tests: 151/151 PASS (lld-link)
nsc_stage46_native_libraries_tests: 161/161 PASS (lld-link)
nsc_stage48_2_library_metadata_tests: 151/151 PASS (NSLink)
nsc_stage46_native_libraries_tests: 161/161 PASS (NSLink)
build root: A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
```

No se instaló producto ni se usó `A:\nsc-devel`.
No product was installed and `A:\nsc-devel` was not used.

## Commit / Commit

```text
Endurece la validación de artefactos NativeSharp / Harden NativeSharp artifact validation
```
