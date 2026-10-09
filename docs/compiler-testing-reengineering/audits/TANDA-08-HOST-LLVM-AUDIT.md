# Segunda auditoría independiente — Host.LLVM

## Método

La segunda pasada comenzó en los 40 registros `PF-*`, no en el documento narrativo. Recontó archivos, líneas,
bytes y símbolos; reagrupó las rutas de cache, optimización, target, particionado, ambos lowerers, ABI, debug,
lifetime y object generation; después contrastó documentos, contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 40 |
| líneas | 25,115 |
| bytes | 1,269,325 |
| tipos | 78 |
| constructores/destructores | 20 |
| funciones definidas | 474 |
| funciones externas LLVM-C | 230 |
| símbolos | 802 |
| contratos | 64 |
| casos diseñados | 512 |
| casos PR | 448 |
| casos NIGHTLY | 64 |
| omitidos o desconocidos | 0 |

## Gates

- 40 archivos con huella, estado, evidencia y sección;
- 802 símbolos con archivo, línea y contrato;
- 64 contratos con fuente y exactamente ocho matrices;
- 512 casos con IDs únicos y referencias válidas;
- paridad CSV/JSON del manifiesto y hashes fuente iguales al congelado;
- cero cambios en producción, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. LLVM 23 es requisito estricto; un diagnóstico conserva el texto histórico `NativeSharp 2.0`.
2. La frontera LLVM-C es manual y versionada: firmas y enums son contrato de integración.
3. Debug info usa el enumerador DWARF `C_sharp` aunque el producto no depende de .NET.
4. O0 posee fast path sin pipeline y excepción `mem2reg` para funciones SEH.
5. El cache usa metadata propia, locks y cuarentena; no confía en existencia del objeto.
6. Full debug/inline obligatorio/Full LTO fuerzan módulo único; cache o ThinLTO fuerzan partición estable.
7. La división signed extrema se corrige sin trap y preserva los números históricos de trap.
8. El lowerer OOP concentra 265 funciones explícitas y múltiples máquinas de estado; las pruebas se atan a
   contratos observables, no a su futura modularización.
9. Existen recorridos recursivos en particionado, identidad MIR, callback discovery y MIR lowering; esta fase no
   los modifica.
10. El cleanup temporal de object bundle ignora error de `remove_all` por diseño vigente.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.LLVM` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
