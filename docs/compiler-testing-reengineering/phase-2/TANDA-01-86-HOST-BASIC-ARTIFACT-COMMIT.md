# Tanda 01.86 - Host.Basic: commit de artefactos

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-013` |
| Casos implementados | 7/7: `TB-BAS-058..059`, `TB-BAS-068..072` |
| Casos bloqueados | Ninguno |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | CERRADO |

La tanda comprueba el commit vacío y su cierre irreversible, la protección
contra un segundo commit, la publicación de destinos nuevos y existentes, el
commit ordenado de tres artefactos, la mezcla de destinos nuevos/existentes y
una ruta Unicode superior a `MAX_PATH`.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TB-BAS-058` | Un commit sin entradas tiene éxito, limpia el error y rechaza exactamente el segundo intento. |
| `TB-BAS-059` | Un commit poblado sólo puede ejecutarse una vez y el rechazo posterior no altera los bytes publicados. |
| `TB-BAS-068` | Un destino nuevo recibe bytes exactos; el stage conserva el basename y permanece aislado. |
| `TB-BAS-069` | Un destino existente se reemplaza y no quedan backup ni sibling de transacción. |
| `TB-BAS-070` | Tres destinos nuevos se publican juntos con bytes exactos y sin residuos. |
| `TB-BAS-071` | La mezcla existente/nuevo/existente reemplaza o crea cada destino sin residuos. |
| `TB-BAS-072` | Una ruta Unicode mayor que `MAX_PATH` se publica mediante la ruta extendida Win32. |

## Corrección de infraestructura durante la regresión

El gate agregado para `CB-BAS-013` pasó, pero la primera regresión completa
reveló que el gate agregado de `Host.Basic` conservaba una lista manual de todos
los casos PR del componente. El nuevo contrato amplió correctamente ese
conjunto y volvió obsoleta la lista duplicada. El gate ahora deriva los IDs PR
del manifiesto y los ordena con el mismo contrato determinista del selector.
No se relajó ningún resultado ni se ocultó ningún fallo de caso.

## Evidencia reproducible

- Gate dirigido Debug: 7/7 en 0.68 s; gate Release dentro de la regresión final: 7/7 en 0.57 s.
- Suite nueva completa final: Debug 82/82 en 49.39 s; Release 82/82 en 49.41 s.
- Controlador Python: 51/51 en 0.726 s.
- Fuentes Python de la suite: 103/103 parseadas en memoria.
- Manifiesto: 728 casos, 0 duplicados y 7 casos implementados de `CB-BAS-013`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en el módulo nuevo.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 119 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CB-BAS-013` queda cerrado con sus siete casos implementados y verificados. El
siguiente contrato ejecutable de la ola 1 es `CB-BAS-014`.
