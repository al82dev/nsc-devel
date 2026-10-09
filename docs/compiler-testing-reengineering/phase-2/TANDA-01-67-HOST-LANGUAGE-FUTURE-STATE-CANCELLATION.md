# Tanda 01.67 - Host.Language: estado de futuros y cancelación

Fecha: 2026-10-09

## Alcance cerrado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-005` |
| Casos implementados | `TL-LANG-057` a `TL-LANG-062` (6 casos) |
| Acumulado del contrato | 13/28 casos |
| Producto modificado | No |
| Suite histórica modificada | No |
| Pendiente del contrato | `TL-LANG-063` a `TL-LANG-077` |

La subtanda prueba el protocolo mutable de `FutureState` y la cancelación cooperativa
contra la implementación pública existente. Cada celda de las matrices crea un estado
nuevo; ninguna prueba depende del resultado mutable de otra.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-057` | Seis estados iniciales por tres resultados de `poll`, con transición, terminación y error exactos. |
| `TL-LANG-058` | `consume` desde todos los estados, ambas terminaciones `Ready` y segundo consumo rechazado. |
| `TL-LANG-059` | Cancelación desde todos los estados; sólo `Created` y `Pending` cambian a `Cancelled`. |
| `TL-LANG-060` | Limpieza vacía y combinada: defers inversos antes de locales inversos. |
| `TL-LANG-061` | Tres estados suspendidos por tres causas, defers estables por profundidad y drops inversos. |
| `TL-LANG-062` | Estado, nombres de slots y defers inválidos fallan sin entregar un plan parcial. |

## Propiedades adicionales verificadas

- Los rechazos de `poll`, `consume` y `cancel` no mutan estado ni terminación.
- Consumir conserva si el futuro terminó con valor o error.
- Un plan de cancelación conserva la causa y el estado suspendido solicitados.
- Toda cancelación válida es cooperativa y no usa aborto de hilo.
- Defers con la misma profundidad mantienen su orden de registro.
- Sólo se destruyen slots vivos que requieren destrucción; los demás no generan acciones.
- Los diagnósticos mantienen prioridad determinista cuando coinciden varias entradas inválidas.
- La implementación de pruebas usa bucles y matrices; no introduce recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 13/13 en 5.05 s; Release 13/13 en 0.77 s.
- Suite nueva completa: Debug 65/65 en 65.72 s; Release 65/65 en 11.04 s.
- Controlador Python: 51/51 en 0.762 s.
- Fuentes Python de la suite: 86/86 compiladas en memoria.
- Manifiesto: 504 casos, 0 duplicados y 13 casos de `CL-LANG-005`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; 143 avisos de
  dependencias no propias fueron suprimidos por la configuración estándar.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`.
- `git diff --check`: correcto; fuentes nuevas o modificadas con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.

## Estado

Estado de futuros y cancelación quedan cerrados sin hallazgos de producto. El siguiente
bloque de `CL-LANG-005` es `TL-LANG-063..068`, correspondiente a generadores asíncronos.
