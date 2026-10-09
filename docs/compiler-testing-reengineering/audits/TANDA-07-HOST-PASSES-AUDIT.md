# Segunda auditoría independiente — Host.Passes

## Método

La segunda pasada comenzó en los 42 registros `PF-*`, no en la narrativa. Recalculó tipos y definiciones físicas,
reconstruyó el grafo de ownership, siguió cada campo clonado, rehízo las tablas de folding y efectos, y recorrió la
máquina de estados de cada pass, del administrador y del verifier. Finalmente contrastó manifiestos, símbolos,
contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 42 |
| líneas | 3,956 |
| bytes | 113,411 |
| tipos | 25 |
| funciones y helpers | 133 |
| símbolos | 158 |
| contratos | 56 |
| casos diseñados | 448 |
| casos PR | 392 |
| casos NIGHTLY | 56 |
| omitidos o desconocidos | 0 |

## Gates

- 42 archivos con huella, estado, evidencia y sección;
- 158 símbolos con archivo, línea y contrato primario;
- 56 contratos con fuente y exactamente ocho matrices;
- 448 casos con IDs únicos y referencias válidas;
- cada archivo contiene al menos un símbolo inventariado;
- paridad total de 974 filas entre CSV/JSON y hashes fuente iguales al congelado;
- cero cambios en producción, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. `all()` y `cfgAndDominance()` son equivalentes mientras el dominio tenga sólo CFG y Dominance.
2. El builder precrea todos los valores antes de clonar instrucciones, lo que permite referencias no locales sin
   conservar punteros al MIR fuente.
3. Floating constant equality es bit-exacta, no igualdad numérica.
4. Una conversión floating → entero se considera potencialmente atrapante aunque no pueda plegarse.
5. Copy propagation protege cadenas cíclicas con un set visitado y no usa recursión.
6. DCE alcanza cierre recalculando usos, incluido cada incoming PHI.
7. SimplifyControlFlow sólo poda inalcanzables en funciones donde antes simplificó una rama.
8. Un pass que declara cambio se verifica inmediatamente después de invalidar análisis.
9. Los conteos del `PassRecord` se capturan después de esa verificación, por lo que incluyen reconstrucciones
   necesarias para certificar el resultado.
10. Los registros del módulo son append entre ejecuciones del manager.
11. El verifier distingue usos ordinarios de usos PHI sobre edges predecessoras.
12. El dump floating deja precisión 17 en el stream; el escape hexadecimal sí restaura flags y fill.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Passes` queda `AUDITADO` y `RECONSTRUIBLE` para la suite nueva. La Tanda 07 queda documentalmente completa:
128/128 archivos de HIR, MIR, CFG, SSA, Dominance y Passes. No se afirma ejecución, cobertura ni ausencia de
defectos de producto.
