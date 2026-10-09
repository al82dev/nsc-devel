# Segunda auditoría independiente — Host.Interop

## Método

La segunda pasada comenzó en los 12 registros `PF-*`, no en el documento narrativo. Recontó archivos, líneas,
bytes y símbolos; reagrupó header C, reporte ABI, diff API/ABI, capacidades, modelo/códec NSMETA y merger; después
contrastó documentos, contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 12 |
| líneas | 8,423 |
| bytes | 387,816 |
| tipos definidos | 41 |
| funciones definidas | 137 |
| símbolos | 178 |
| contratos | 48 |
| casos diseñados | 384 |
| casos PR | 336 |
| casos NIGHTLY | 48 |
| omitidos o desconocidos | 0 |

## Gates

- 12 archivos con huella, estado, evidencia y sección;
- 178 símbolos con archivo, línea y contrato;
- 48 contratos con fuente y exactamente ocho matrices;
- 384 casos con IDs únicos y referencias válidas;
- hashes fuente iguales al manifiesto congelado;
- cero cambios en producción, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. NSMETA vigente es 1.38 y contiene compuertas aditivas desde 1.0.
2. El byte de revisión de fuente introducido en 1.23 se consume pero se descarta; no selecciona edición.
3. `producerVersion` sí conserva una ventana de productores conocidos incompatibles.
4. Los comentarios históricos duplican y desordenan minors 23, 34 y 38.
5. `LibraryMetadata.cpp` concentra 59 funciones físicas y varias máquinas contractuales.
6. La integridad de bytes y la coherencia del modelo son gates separados.
7. La proyección ABI excluye información semántica/privada sin borrar layout, dispatch o exports.
8. El merger colapsa sólo duplicados exactos y rechaza contenido divergente bajo el mismo key.
9. El header C rechaza layouts que C plano no puede expresar; no aproxima silenciosamente.
10. Source y ABI compatibility se calculan como superficies distintas.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Interop` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
