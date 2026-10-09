# Segunda auditoría independiente — Host.Linker

## Método

La segunda pasada partió de los 16 registros `PF-*`. Recontó archivos, líneas, bytes y símbolos; reagrupó COFF,
PE, PDB, linker/process, EXE, LIB/DLL, imports y runtime; después contrastó documentos, contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 16 |
| líneas | 4,362 |
| bytes | 160,519 |
| tipos definidos | 17 |
| funciones definidas | 115 |
| símbolos | 132 |
| contratos | 40 |
| casos diseñados | 320 |
| casos PR | 280 |
| casos NIGHTLY | 40 |
| omitidos o desconocidos | 0 |

## Gates

- 16 archivos con huella, estado, evidencia y sección;
- 132 símbolos con archivo, línea y contrato;
- 40 contratos con fuente y exactamente ocho matrices;
- 320 casos con IDs únicos y referencias válidas;
- hashes fuente iguales al manifiesto congelado;
- cero cambios en producto, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. `LldLinker` conserva un nombre histórico aunque NSLink es el producto exigido por discovery.
2. Librarian y linker son herramientas distintas; `llvm-lib` sigue siendo necesario para archives.
3. Response file, log, proceso y verificación de producto son gates separados.
4. COFF, PE y PDB aplican límites defensivos antes de reservar o dereferenciar.
5. El incremental link y el artifact cache usan identidades completas y telemetría separada.
6. El runtime library no puede poseer el entry point y debe exportar el catálogo ABI esperado.
7. La tabla de imports runtime es única y fusiona módulos externos case-insensitivamente.
8. Los builders eliminan productos parciales cuando la verificación posterior falla.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Linker` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
