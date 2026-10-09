# Tanda 01.69 - Host.Language: concurrencia y alcances de tareas

Fecha: 2026-10-09

## Alcance cerrado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-005` |
| Casos implementados | `TL-LANG-069` a `TL-LANG-077` (9 casos) |
| Acumulado del contrato | 28/28 casos |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | LISTO |

La subtanda final separa los contratos de concurrencia en un módulo propio. Verifica
derivación `Send/Sync`, metadata, préstamos de tareas, salida estructurada, identidad del
contrato, persistencia en disco y operaciones simultáneas sin estado global compartido.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-069` | Matriz booleana completa de campos, raw pointer y opt-outs negativos para `Send/Sync`. |
| `TL-LANG-070` | Todos los tipos de miembro, overrides seguros/inseguros, negativos y prioridad diagnóstica. |
| `TL-LANG-071` | Round-trip de metadata, compatibilidad minor y rechazo exacto de major o identidad inválidos. |
| `TL-LANG-072` | Cuatro combinaciones de join y vigencia del préstamo. |
| `TL-LANG-073` | Executor completo e incompleto, noheap, hijos normales y préstamos válidos o inválidos. |
| `TL-LANG-074` | Salida normal, error, cancelación y pánico con subconjuntos de hijos terminados. |
| `TL-LANG-075` | Bytes contractuales y fingerprint FNV congelados y repetibles. |
| `TL-LANG-076` | Escritura, padres ausentes, CRLF, alteración, destino vacío y archivo ausente. |
| `TL-LANG-077` | Ocho workers independientes sobre futuros, metadata y generadores asíncronos. |

## Propiedades adicionales verificadas

- `Atomic` deriva ambas capacidades; raw pointers fallan cerrados.
- Referencias compartidas, mutables y traits dinámicos usan sus garantías específicas.
- Un override inseguro sólo surte efecto después de una prueba de seguridad auditada.
- Un opt-out negativo prevalece sobre el override revisado de la misma capacidad.
- Los hijos incompletos se cancelan y esperan en orden inverso antes de propagar la causa.
- El manifiesto de concurrencia tiene 695 bytes y fingerprint `7cb427840ab173ff`.
- La verificación normaliza CRLF, pero rechaza cualquier otro cambio de bytes.
- La prueba concurrente escribe únicamente en slots independientes y une todos los workers.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido completo: Debug 28/28 en 12.13 s; Release 28/28 en 1.21 s.
- Suite nueva completa: Debug 65/65 en 72.50 s; Release 65/65 en 11.94 s.
- Controlador Python: 51/51 en 0.752 s.
- Fuentes Python de la suite: 86/86 compiladas en memoria.
- Manifiesto: 519 casos, 0 duplicados y 28 casos de `CL-LANG-005`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 281 avisos de
  dependencias no propias suprimidos y una supresión local documentada para el bitmask
  estándar `std::ios::openmode`.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`, incluso con `std::thread`.
- `git diff --check`: correcto; fuentes nuevas o modificadas con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.

## Estado

`CL-LANG-005` queda completo: 28 casos implementados y verificados, sin hallazgos de
producto ni pendientes internos. La continuación de Ola 1 debe tomar el siguiente contrato
independiente de `Host.Language` según el inventario canónico.
