# Tanda NSC305 GATE-F01 — fingerprint ABI root estable

## Alcance

Esta corrección restaura la huella golden del contrato ABI root con Object ABI 2 sin cambiar el
payload físico NSMETA 1.40, su versión, el decoder, el golden ni los contratos públicos. El cambio
queda limitado a `encodeAbiProjection`.

## Causa

La proyección ABI común se rebaja a la representación neutral 1.24, pero conservaba activada la
sección `traitImpls` para todo modelo 1.40. Cuando la colección estaba vacía, `encodePayload`
insertaba un contador `u32(0)` que no representaba contenido ABI y desplazaba la huella root de
`76fa6ca4...` a `eacd2b7b...`.

El payload físico 1.40 sí necesita siempre ese contador para que su decoder conserve el framing.
Por eso no se modifican ni la llamada de `writeModelImpl` ni `decodePayload`.

## Corrección

`encodeAbiProjection` conserva la sección 1.40 sólo cuando el modelo realmente contiene una
conformidad externa:

```cpp
const bool includeTraitImpls =
    projected.formatMinor >= 40 && !projected.traitImpls.empty();
```

Con la colección vacía, la proyección vuelve a ser byte por byte la raíz histórica neutral. Con
una colección no vacía, el contador y todos los registros siguen participando en la huella.

## Pruebas y evidencia

- Build focal de `nsc_stage48_2_library_metadata_tests`,
  `nsc_library_metadata_version_tolerance_tests` y
  `nsc_item407_official_promotion_tests`: PASS.
- `nsc.library-metadata-version-tolerance`: PASS, 16/16. Comprueba el round-trip de la sección
  1.40 no vacía y que su huella difiere de la metadata vacía.
- `nsc.item407.official-promotion`: PASS, 11/11.
- `nsc.stage48_2.library-metadata`: el fallo específico
  `ABI root contract with Object ABI 2 retains its reviewed golden fingerprint` desapareció. El
  ejecutable continuó hasta los mismos quince fallos ajenos de productos DLL/archive y el
  `SegFault` ya documentados por la tanda de compatibilidad universal; por ello el agregado no se
  declara PASS ni se atribuye a F01.
- Auditoría estática: la llamada de payload físico permanece exactamente protegida por
  `metadata.formatMinor >= 40`; no se modificaron versión, decoder ni golden.
- `git diff --check`: PASS.

Logs:

- `A:\temp\gate-f01-build-targets.log`
- `A:\temp\gate-f01-ctest.log`

## Límites

GATE-F02 y GATE-F03 permanecen pendientes. No se ejecutaron baseline, batería completa, deploy,
instalación, bindings ni consumidores externos.
