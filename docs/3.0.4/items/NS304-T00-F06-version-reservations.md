# NS304-T00-F06 — Reservas de versión 3.0.4

## Estado

`CERRADO` el 2026-10-12 como contrato de planificación. Congela las bases y
ventanas que pueden reclamar T01–T15; no adelanta ningún writer, reader ni ABI.

## SCOPE LOCK

- Objetivo: evitar números improvisados o incompatibles cuando una feature
  cambie AST, HIR, MIR, NSMETA, runtime ABI o debug metadata.
- Naturaleza: contrato documental y manifiesto mecánico; sin cambio de producto.
- Revisión base: `73019e590` en `development`.
- Artefacto canónico:
  `docs/3.0.4/contracts/version-reservations.json`.
- Protegido: constantes vigentes, codecs, lectores, writers, runtime, CMake,
  toolchain, Builder y configuración.

## Baseline demostrado

| Contrato | Estado actual verificable | Fuente actual |
|---|---|---|
| AST | modelo in-memory sin wire schema global | `include/nativesharp/AST/` |
| HIR | modelo in-memory sin constante global de schema | `include/nativesharp/Frontend/ProjectOopHirModel.h` |
| MIR | `1.30` | `kProjectOopMirSchemaMajor/Minor` |
| NSMETA | `1.38` | `kMetadataFormatMajor/Minor` |
| runtime ABI | `1.2`, symbol set `4` | `kRuntimeAbiMajor/Minor`, `kRuntimeSymbolSetVersion` |
| debug metadata | versión del productor LLVM, sin schema NativeSharp propio | `LLVMDebugMetadataVersion()` en el lowerer |

AST, HIR y debug no se describen falsamente como formatos wire actuales. La
reserva introduce namespaces futuros (`NSAST`, `NSHIR`, `NSDEBUG`) que sólo
adquieren existencia productiva cuando un ítem propietario cierre su contrato.

## Ventanas reservadas

| Contrato | Siguiente reservable | Techo 3.0.4 | Regla |
|---|---:|---:|---|
| AST | `NSAST 1.1` | `1.15` | revisión estructural interna, no wire |
| HIR | `NSHIR 1.1` | `1.15` | revisión estructural interna, no wire |
| MIR | `1.31` | `1.45` | minor aditivo; reader/verifier en el mismo ítem |
| NSMETA | `1.39` | `1.53` | minor aditivo; writer/reader/downlevel en el mismo ítem |
| runtime ABI | `1.3` | `1.17` | sólo cambio físico o de símbolos observable |
| debug metadata | `NSDEBUG 1.1` | `1.15` | contrato NativeSharp sobre CodeView/PDB |

Los 15 slots corresponden al máximo de una reclamación por tanda T01–T15. No
son versiones emitidas ni obligan a consumir un slot: el primer ítem que cambie
una forma observable reclama el siguiente minor libre de ese contrato. T16
consolida los valores efectivamente reclamados y T17 sólo los certifica.

## Reglas de transición

1. El cambio y su versión viven en el mismo ítem; nunca se adelanta el número.
2. Dos formas observables distintas no comparten un minor.
3. Un cambio source-only no avanza runtime ABI.
4. Cambiar major requiere contrato breaking explícito; no está autorizado por
   esta reserva.
5. MIR exige modelo, dumper, verifier y cache identity coherentes.
6. NSMETA exige writer, reader, downgrade, incompatibilidad futura y consumer
   sin fuentes.
7. Debug exige CodeView/PDB observable; una anotación LLVM interna no basta.
8. El slot reclamado se registra en el expediente del ítem y en este manifiesto.

## Evidencia puntual

La validación mecánica parseó el JSON y comprobó:

- schema único y 6/6 contratos requeridos;
- bases numéricas idénticas a las constantes del producto para MIR, NSMETA y
  runtime ABI;
- AST/HIR/debug explícitamente no-wire en su baseline;
- 15 propietarios T01–T15 por contrato, sin duplicados ni T00/T16/T17;
- `next_minor <= ceiling_minor` y major congelado en `1`;
- cinco prohibiciones explícitas.

## Gate F06

```text
contracts = 6/6
current_versions_invented = 0
current_numeric_mismatches = 0
owner_sets = 6 * 15
duplicate_owners = 0
premature_product_bumps = 0
```

Siguiente acción determinista: `NS304-T00-F07`, manifest de dependencias
compiler/runtime/STL por feature.
