# Congelamiento 3.0 P8 — época de layout NSMETA 1.41: una biblioteca anterior pide recompilarse

Fecha: 2026-10-26. Revisión de partida: `b8585f145`.

## Decisión aplicada

P8, opción 1, en palabras de Alberto: el layout binario cambió y una biblioteca vieja no se puede aceptar
silenciosamente. Autoriza excepcionalmente el incremento de la versión/época de NSMETA necesario para rechazarla
con un diagnóstico que pida recompilar. No autoriza reactivar versionado variable ni modificar NSMETA por cambios
que no alteren su contrato.

## Problema

Tres cambios de la campaña alteran lo que una biblioteca precompilada no genérica pasa o recibe, sin cambiar el
número de NSMETA (A-05 de la auditoría):

- DEC-11: un arreglo cuyo elemento requiere drop lleva un mapa de vida detrás de sus datos (doc 15);
- P4: `Span`/`ReadOnlySpan` sobre un arreglo llevan el arreglo y su desplazamiento, de 16 a 32 bytes (doc 29);
- P3: el bit 31 de las banderas de miembro, `kMetadataMemberReadonlyReferenceReturn` (doc 19).

Un `.lib` compilado antes se aceptaba sin aviso. Sonda `A:\temp\nsc-dec\d20`: `OldApi` compilada con el `nsc`
de `b8585f145` (NSMETA 1.40) y consumida por `UseOld` daba 7, mezclando los dos layouts sin diagnóstico.

## SCOPE LOCK

- **Cambia.**
  - `LibraryMetadata.h`: `kMetadataFormatMinor` pasa de 40 a 41, con su comentario de minor, y
    `kMetadataLayoutEpochMinor = 41`.
  - `LibraryMetadataCodec`: método nuevo `verifyLayoutEpoch`. Rechaza un módulo con NSMETA anterior a 1.41 y
    pide recompilarlo.
  - Los tres caminos que consumen el código de una biblioteca llaman a `verifyLayoutEpoch` después de
    `verifyCurrentCompiler`: `PrecompiledProjectPreparation.cpp` (importación de metadata precompilada),
    `LibraryReferenceValidation.cpp` (`LibraryReference`) y `LibraryMetadataConstruction.cpp` (dependencia
    binaria de una biblioteca nueva).
  - `tests/fixtures/nsmeta/historical-1.40.nsmeta`: el 1.40 pasa a ser minor histórico, y la prueba de
    compatibilidad exige su golden.
- **Idéntico.**
  - La codificación del payload: 1.41 se escribe y se lee igual que 1.40.
  - La lectura y `verifyCurrentCompiler` siguen aceptando todo minor anterior. Las herramientas de inspección y
    diff de bibliotecas, y las pruebas históricas de compatibilidad (`Stage48_2`, `Stage50_9`), no cambian.
  - La emisión down-level (`writeModel` con un minor anterior) queda como estaba; no se reactiva ni se amplía.
  - Los goldens 1.0 a 1.39.
- **Prohibido.** Cambiar el major; cambiar la codificación; rechazar minors anteriores en la lectura.

## Contrato

1. NSMETA 1.41 marca la generación de layout del productor. No agrega campos.
2. Una biblioteca con NSMETA 1.x, x < 41, no se consume. El error nombra el módulo, su versión y la época, y
   pide recompilar.
3. La caché persistente de verificación de NSMETA incluye el minor en su llave
   (`LibraryMetadataRepository.cpp`), así que una verificación guardada con 1.40 no salta la comprobación nueva.

## Validación

nsc Release recompilado (exit 0). Sondas `d20`:

| Paso | Resultado |
|---|---|
| `OldApi` con el `nsc` de `b8585f145` (`Old40`), consumida por `UseOld` antes del cambio | 7 |
| La misma `Old40`, consumida por `UseOld` con el `nsc` nuevo | rechazo (texto abajo) |
| `OldApi` recompilada con el `nsc` nuevo (`New41`), consumida por `UseNew` | 7 |

```
nsc: error: cannot import precompiled metadata 'A:/temp/nsc-dec/d20/Old40/OldApi.nsmeta': module 'OldApi' was
produced with NSMETA 1.40, before the NativeSharp 3.0 layout epoch (NSMETA 1.41): arrays of values with
destructors and array-backed spans changed layout; recompile the library with this compiler
```

Regresión focal (`all.cmd p8`): sondas d16 Q1–Q9, d15 G4 y H1–H8, d13 A1–A11, c27, c35, d12 L1–L6, d11 L2, P2,
C12* y d14 K1 con los valores de siempre; casos ref 96, copia 30; corpus de `await` igual a `d04`.

Golden 1.40. Árbol de pruebas aparte, `A:\temp\nsc-dec\bt` (mismo toolchain que `build-clang`,
`NSC_BUILD_TESTS=ON`), sólo con los objetivos `nsc_stage48_2_library_metadata_tests` y
`nsc_freeze30_contract_tests`:

| Corrida de `nsc_stage48_2_library_metadata_tests` | Checks del golden 1.40 | Fallos |
|---|---|---|
| sin golden | «archived NSMETA 1.40 golden remains readable» y «… remains compatible» fallan | 42 de 400 |
| con `NSC_P2_REFRESH_GOLDENS=1` (crea sólo el 1.40; los demás se saltan) | pasan | 40 de 400 |

Los 40 fallos restantes son de entorno: falta `C:/Program Files (x86)/NativeSharp/sdk/win64/import-libraries`
(`KERNEL32.import.lib`) para emitir DLL y EXE. No tocan NSMETA ni cambian con este commit. `git status` muestra
sólo el golden nuevo bajo `tests/fixtures/nsmeta`.

Prueba nueva: `Freeze30ContractTests::testNsmetaLayoutEpoch`. `nsc_freeze30_contract_tests` compila en ese árbol
(exit 0, warnings como error); no se ejecutó.

## No verificado

- `Freeze30ContractTests` no se ejecutó; su caso nuevo es la sonda `d20` y la regla del codec.
- Batería completa: queda para el cierre final.
