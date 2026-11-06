# Referencias históricas NSMETA 1.40 a 1.43

Se recuperaron bytes de productores reales, emitidos antes de la extensión
NSMETA 1.45 y aceptados por el verificador actual con Object ABI 3:

| Versión | Origen en `A:/nsc-interop-build` | Productor | SHA-256 |
| --- | --- | --- | --- |
| 1.40 | `cpp-fixtures/published-snapshot/SnapshotProducer.nsmeta` | `20261105T182209Z.f05a67d769d69214` | `c69e870cfbbdb1b73e2f6f98c3fe31776d573254435b54ef0018e475a4c6cb3f` |
| 1.41 | `tanda08-inline-c/CInlineProducer.nsmeta` | `20261106T154951Z.66725a00ab433111` | `b613fd16e588a4ccbdc0bf3a791dd2b22f738cd860ca184229048ed57808e0d9` |
| 1.42 | `tanda08-meta-c/CPublishedProducer.nsmeta` | `20261106T101920Z.4e3e3ad093afac65` | `d9bd05dcf8606eed856fd9bddacf5a87bd4f54576770753b4676ff9812716b18` |
| 1.43 | `tanda08-bitfields-metadata/CBitFieldsProducer.nsmeta` | `20261106T144226Z.d2e27697ae33cc36` | `59159d1d0f29fa423d6e50dd6529cc6002721c741bce1deae9f418217fc9ea1a` |

La prueba debe aceptar estas referencias porque declaran Object ABI 3, a
diferencia de las referencias anteriores con ABI 1 o 2. Pendiente repetir la
validación agrupada con las cuatro referencias incorporadas.

## Resultado de la validación agrupada

Con la construcción incremental `85d0346a4c41393f`, pasaron las tres pruebas
independientes: `nsc.stage48_2.library-metadata`,
`nsc.library-metadata-version-tolerance` y `nsc.library-metadata-merger`
(3/3, código 0). Las referencias 1.40–1.44 se leyeron con el verificador
actual; las referencias con Object ABI antiguo mantienen su rechazo explícito.
Los temporales quedaron en A: y no se activó la actualización automática de
referencias. Esta comprobación no cierra la tanda 08 ni demuestra todos los
contratos de interoperabilidad.
