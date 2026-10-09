# Segunda auditoría independiente — Host.Runtime

## Método

La segunda pasada partió de los seis registros `PF-*`. Recontó archivos, líneas, bytes, tipos y funciones;
reagrupó ABI, capacidades, construcción LLVM, Win32, atomics y EH; después contrastó documentos, contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 6 |
| líneas | 7,191 |
| bytes | 384,512 |
| tipos definidos | 7 |
| funciones definidas | 122 |
| símbolos | 129 |
| contratos | 48 |
| casos diseñados | 384 |
| casos PR | 336 |
| casos NIGHTLY | 48 |
| omitidos o desconocidos | 0 |

## Gates

- seis archivos con huella, estado, evidencia y sección;
- 129 símbolos físicos con archivo, línea y contrato;
- 48 contratos con fuente y exactamente ocho matrices;
- 384 casos con IDs únicos y referencias válidas;
- 99 entradas ABI contrastadas con sus clases de imagen;
- hashes fuente iguales al manifiesto congelado;
- cero cambios en producto, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. El ABI vigente es 1.2, symbol set 4, con 98 funciones y un global TLS.
2. La verificación exige igualdad exacta por imagen, no sólo presencia de algunos exports.
3. El reporte de capacidades conserva estados parciales y ausentes sin promoverlos a listos.
4. `WindowsRuntimeBuilder.cpp` es monolítico, pero sus futuros tests deben sobrevivir a su modularización.
5. Los wrappers Win32 usan sentinels definidos por operación y mantienen límites de buffers/handles.
6. Atomics son `seq_cst`; half se implementa sin CRT/F16C.
7. EH mantiene payload por TLS y separa códigos de throw tipado y panic.
8. Todos los modos de imagen terminan en verificación LLVM y ABI aplicable.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Runtime` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
