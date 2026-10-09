# Tanda 01.73 - Host.Language: genéricos, identidades y presupuestos

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-009` |
| Casos implementados | 10/12: `TL-LANG-112..115`, `TL-LANG-117..120`, `TL-LANG-122..123` |
| Casos bloqueados | `TL-LANG-116`, `TL-LANG-121` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | PARCIAL POR DOS DEFECTOS DE PRODUCTO |

La tanda caracteriza normalización de restricciones, argumentos const, identidades de
instanciación, packs, inferencia, aliases y límites de instanciación. Las pruebas llaman
directamente a las interfaces públicas vigentes y no duplican lógica de producción.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-112` | Conjunto vacío, ordenamiento, deduplicación y canonical length-prefixed exacto. |
| `TL-LANG-113` | Átomo vacío, tres conflictos, prioridad del primer error y estado definido del resultado. |
| `TL-LANG-114` | Cuatro clases const y matriz de type key/valor presente o vacío. |
| `TL-LANG-115` | Bytes del marker y round-trip exacto con tipos corto, puntuado y de 64 bytes. |
| `TL-LANG-117` | Identidad sin argumentos, con tipos y mixta con las cuatro clases const. |
| `TL-LANG-118` | Cero, uno y varios packs; longitudes, budget y prioridad diagnóstica. |
| `TL-LANG-119` | Igualdad, diferencia, vacíos y conversión arbitraria en inferencia bidireccional. |
| `TL-LANG-120` | Alias transparente y opaco con campos poblados y vacíos. |
| `TL-LANG-122` | Enter/leave, depth, count, recursión, counters y fallos transaccionales. |
| `TL-LANG-123` | Leave vacío, trece frames, truncado a doce y leave adicional sin underflow. |

## Bloqueos confirmados

### `TL-LANG-116`: trailing indistinguible del valor

El formato `$nsc_const$<length>:<type>:<normalizedValue>` no contiene longitud ni
terminador para `normalizedValue`. El parser asigna todo byte posterior al segundo `:` al
valor. Por ello no existe una entrada que permita distinguir un valor válido de un trailing
añadido: ambos son el mismo campo según el formato actual.

- `BLOQUEADO_POR`: gramática del marker sin frontera final verificable.
- Resolverlo exige definir un terminador, añadir longitud al valor o delegar su gramática a
  una validación de tipo conocida.
- No se creó una prueba con una expectativa imposible ni se cambió el formato de producto.

### `TL-LANG-121`: los duplicados alteran la dominancia

`specializationStrictlyDominates` deduplica `candidateConstraints`, pero compara el tamaño
de ese conjunto contra `fallbackConstraints.size()` sin deduplicar el fallback. Por ejemplo,
el candidato `{A, B}` no domina al fallback `{A, A}` aunque sus conjuntos normalizados sean
`{A, B}` y `{A}`. La relación deja de ser un superconjunto estricto cuando hay duplicados,
caso requerido expresamente por el plan.

- `BLOQUEADO_POR`: comparación asimétrica entre cardinalidad normalizada y cardinalidad bruta.
- Resolverlo exige normalizar ambos operandos antes de comparar la relación de conjuntos.
- Fase 2 no autoriza cambios de producto; no se congeló el defecto como oráculo correcto.

## Propiedades adicionales verificadas

- Los conflictos se diagnostican en orden: copy/move-only, class/struct y class/unmanaged.
- Un pack que excede budget falla antes de comprobar longitudes incompatibles posteriores.
- El count de instanciación es monotónico y los fallos no alteran stack ni contadores.
- El reporte causal conserva los doce frames activos más recientes y antepone `... -> `.
- Los ejecutables Debug y Release usan CRT estático y sólo importan `KERNEL32.dll`.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido implementable final: Debug 10/10 en 5.04 s; Release 10/10 en 0.65 s.
- Suite nueva completa final: Debug 69/69 en 270.31 s; Release 69/69 en 33.21 s.
- Controlador Python: 51/51 en 0.761 s.
- Fuentes Python de la suite: 90/90 compiladas en memoria.
- Manifiesto: 562 casos, 0 duplicados y 10 casos implementados de `CL-LANG-009`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en el módulo nuevo.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-009` queda con 10 casos implementados y verificados y dos bloqueos de producto
documentados. La Fase 2 puede continuar con `CL-LANG-010`, pero no declarar esta ola completa
mientras `TL-LANG-116` y `TL-LANG-121` sigan bloqueados.

## Adenda de contrato — 2026-10-09

La revisión del productor y parser confirmó que `normalizedValue` ocupa todo el
sufijo del marker. Por tanto, `TL-LANG-116` no es un bloqueo de producto: la
expectativa de `trailing` inválido era irrepresentable y fue corregida en el test
plan. `TL-LANG-121` conserva su clasificación como defecto observable y tendrá
una prueba real, sin invertir ni relajar el oráculo.
