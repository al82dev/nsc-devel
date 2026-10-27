# Congelamiento 3.0 P8 — Object ABI 3: una biblioteca con el layout anterior se rechaza y pide recompilarse

Fecha: 2026-10-26. Revisión de partida: `4da911c53`.

## Nota de trabajo

```text
HEAD: 4da911c53 (revierte 403b06641)
Elemento: P8 / D-12 — frontera incompatible del ABI de objetos
Contrato vigente: NSC-DECISIONES-VIGENTES-CONGELAMIENTO-3.0.md §6.1 — objectAbiVersion = 3; sin gate por
  versión de NSMETA
Documento histórico que podría contradecirlo: 34-P8-EPOCA-NSMETA.md (retirado en 4da911c53)
Archivos exactos que se modificarán:
  include/nativesharp/Interop/LibraryMetadata.h
  src/Interop/LibraryMetadataCodec.cpp
  tests/unit/Freeze30ContractTests.cpp
  tests/unit/Stage48_2LibraryMetadataTests.cpp
  tests/unit/Stage62ToolchainContractTests.cpp
```

## Decisión aplicada

Alberto Huerta Luna, 2026-10-26: P8 se resuelve mediante el contrato que realmente cambió. `objectAbiVersion`
pasa de 2 a 3. No es versionado de NSMETA, sino la identificación de una frontera incompatible del ABI de
objetos. Una biblioteca anterior se rechaza con un diagnóstico equivalente a «object ABI expected 3, found 2;
recompile the library with the current compiler». No se agrega ningún chequeo por versión minor de NSMETA.

Autorizó actualizar `Stage48_2` (goldens 1.15–1.40 rechazados, igual que 1.8–1.14) y el oráculo directo de
`kObjectAbiVersion` en `Stage62ToolchainContractTests.cpp:107`.

La primera implementación (`403b06641`, época por minor de NSMETA) se revirtió completa en `4da911c53`.

## Problema

DEC-11 (mapa de vida tras los datos de un arreglo cuyo elemento requiere drop) y P4 (`Span`/`ReadOnlySpan` sobre
un arreglo llevan el arreglo y su desplazamiento, de 16 a 32 bytes) cambiaron el layout que una biblioteca
precompilada no genérica pasa o recibe. El contrato ABI seguía en object ABI 2. Sonda
`A:\temp\nsc-dec\d20`: `OldApi` compilada con el `nsc` de `b8585f145` (`Old40`) y consumida por `UseOld` daba
7, sin diagnóstico.

## SCOPE LOCK

- **Cambia.**
  - `LibraryMetadata.h`: `kObjectAbiVersion = 3`, con comentario de la causa. El comentario de item 319 se
    conserva.
  - `LibraryMetadataCodec.cpp`:
    - `objectAbiMismatchMessage`: «NativeSharp ABI contract mismatch: object ABI expected 3, found N; recompile
      the library with the current compiler».
    - `verifyFrozenAbiContract` usa ese texto para el object ABI. Mangling, delegate y exception ABI conservan
      su texto y su orden.
    - `authenticateDecodedMetadata`: si la huella ABI no coincide y el object ABI decodificado no es el actual,
      el error agrega la misma causa. Así una biblioteca vieja cuya proyección ya no autentica también pide
      recompilarse.
  - Pruebas: los oráculos de `kObjectAbiVersion` en `Stage48_2` y `Stage62`, y un caso nuevo en
    `Freeze30ContractTests`.
- **Idéntico.** La versión y la codificación de NSMETA, la lectura de todo minor, la emisión down-level, los
  goldens `tests/fixtures/nsmeta`, los demás sub-contratos ABI y sus mensajes.
- **Prohibido.** Usar el major o el minor de NSMETA como gate; aceptar en silencio el layout anterior.

## Contrato

1. Object ABI 3 identifica el layout de arreglos con mapa de vida y de spans sobre arreglos.
2. Una biblioteca con otro object ABI no se consume. El error nombra el esperado, el encontrado y pide
   recompilar.
3. La versión de NSMETA no participa.

## Validación

nsc Release recompilado (exit 0). Sondas `d20`:

| Sonda | Biblioteca | Resultado |
|---|---|---|
| UseOld | `Old40`: `OldApi` del `nsc` de `b8585f145` (object ABI 2) | rechazo (1) |
| UseInstalled | `C:\native-sharp\lib\NativeSharpCore` del 2026-10-16 (object ABI 2) | rechazo (2) |
| UseAbi3 | `Abi3`: `OldApi` recompilada con el `nsc` nuevo | 7 |

```
(1) nsc: error: cannot import precompiled metadata 'A:/temp/nsc-dec/d20/Old40/OldApi.nsmeta': NativeSharp ABI
    contract mismatch: object ABI expected 3, found 2; recompile the library with the current compiler
(2) nsc: error: cannot import precompiled metadata 'C:/native-sharp/lib/NativeSharpCore.nsmeta': NativeSharp
    metadata ABI fingerprint mismatch; NativeSharp ABI contract mismatch: object ABI expected 3, found 2;
    recompile the library with the current compiler
```

Antes de este cambio, la biblioteca instalada ya se rechazaba sólo por «ABI fingerprint mismatch», sin causa ni
petición de recompilar (sonda `UseInstalled` con el `nsc` de `403b06641`).

Regresión focal (`all.cmd abi3`): sondas d16 Q1–Q9, d15 G4 y H1–H8, d13 A1–A11, c27, c35, d12 L1–L6, d11 L2,
P2, C12* y d14 K1 con los valores de siempre; casos ref 96, copia 30; corpus de `await` igual a `d04`.

Unitarias focales en un árbol aparte, `A:\temp\nsc-dec\bt` (mismo toolchain que `build-clang`,
`NSC_BUILD_TESTS=ON`), sólo con los objetivos tocados:

| Ejecutable | Resultado |
|---|---|
| `nsc_stage48_2_library_metadata_tests` | `PASS: 398 checks` |
| `nsc_stage62_toolchain_contract_tests` | `PASS: 46 checks` |
| `nsc_freeze30_contract_tests` | compila (exit 0, warnings como error); no se ejecutó |

Oráculos actualizados en `Stage48_2`:

- goldens 1.8–1.14 esperan «object ABI expected 3, found 1; recompile …» y 1.15 en adelante «… found 2; …»;
- el modelo emitido tiene `objectAbiVersion == 3`;
- el caso E5 usa object ABI 4 y espera «object ABI expected 3, found 4»;
- `verifyMetadata` imprime «object-abi: 3»;
- la huella revisada del contrato raíz pasa de `76fa6ca4…52e6af` a `81d5bb9c…6cdea0`. Es la huella que el
  propio test calculó con object ABI 3; el contrato raíz incluye ese campo.

`Stage62`: `object_abi=3` en el contrato del toolchain.

## No verificado

- `Freeze30ContractTests` (incluido `testObjectAbiBoundary`) y el resto de unitarias: los ejecuta Alberto en el
  gate final.
- Batería completa y árbol histórico: gates finales.
