# Validación 254: reconciliación del catálogo pending vivo

Fecha: 2026-10-10

## SCOPE LOCK

- Objetivo: reconciliar los catálogos `InfrastructurePending` con el manifiesto
  y los registros vivos después de la reauditoría dirigida de 18 casos.
- Autoridad viva: manifiesto, registros de casos y catálogos de disposición.
- Snapshots históricos: no se reescriben; la validación 247 conserva sus tablas,
  conteos y huellas originales.
- Producto protegido: compilador, API, ABI, CMake, Builder y construcción NSC.
- Validación ejecutada: ninguna; sólo comprobación estructural de documentos.

## Transición

| Componente | Integrados | Trabajo futuro |
|---|---:|---:|
| Host.AST | 5 | 1 |
| Host.Parser | 3 | 0 |
| Host.Sema | 3 | 2 |
| Host.Source | 3 | 1 |
| **Total** | **14** | **4** |

Los 14 casos integrados que salen del catálogo pendiente son:

- Host.AST: `TA-AST-014`, `TA-AST-016`, `TA-AST-215`, `TA-AST-223` y
  `TA-AST-224`;
- Host.Parser: `TP-PAR-514`, `TP-PAR-516` y `TP-PAR-524`;
- Host.Sema: `TS-SEMA-101`, `TS-SEMA-265` y `TS-SEMA-281`;
- Host.Source: `TS-SRC-008`, `TS-SRC-040` y `TS-SRC-041`.

Los cuatro casos futuros conservan su cuerpo y oráculo, pero no tienen fila de
manifiesto ni cuentan como ejecutables del pipeline:

- `TA-AST-017`: target test-only con ThreadSanitizer;
- `TS-SEMA-077` y `TS-SEMA-094`: target test-only con ASan+LSan;
- `TS-SRC-049`: target ASan aislado en subprocess.

## Estado resultante

- Catálogo CSV vivo: `3,424` filas únicas.
- Catálogo JSON vivo: `3,424` objetos únicos y equivalente al CSV.
- Manifiesto ejecutable: permanece en `5,709` filas; esta tanda no lo modifica.
- Los archivos conservan el nombre histórico `3438` para mantener enlaces y
  procedencia; su contenido representa el estado vivo reconciliado.
- Esta transición no convierte ausencia de pipeline en `PASS`, `SKIP` ni
  validación funcional.
