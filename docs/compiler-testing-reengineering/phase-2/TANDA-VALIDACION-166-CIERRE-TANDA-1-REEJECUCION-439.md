# Validación 166: cierre de Tanda 1 mediante reejecución exacta de 439 variantes

## SCOPE LOCK

- Objetivo: documentar el cierre de la compuerta de Tanda 1 mediante la
  reejecución exclusiva de los resultados terminales de la corrida autoridad.
- Corrida autoridad:
  `_nscbuilder3/history/20261010-142113-a514d35c`.
- Corrida de cierre:
  `_nscbuilder3/history/20261010-170051-1caf1c2f`.
- Revisión congelada:
  `ca1ef7f04f6bd924405ea40f0fc37b4efb19c5fc`.
- Selección: acción histórica `Reejecutar fallos`, perfil `Todos`, build
  `Debug` y selección global `O0`; los ejecutores host conservan `NA` en su
  clave canónica.
- Universo exacto: `439` variantes provenientes de `274 FAIL`,
  `4 INFRASTRUCTURE_ERROR` y `161 BLOCKED`.
- Naturaleza de esta validación: documentación y evidencia; no modifica
  producto, pruebas, manifiestos, oráculos ni Builder 3.
- Archivos autorizados: este documento y
  `docs/compiler-testing-reengineering/phase-2/TRACKING.md`.
- Acciones prohibidas: reejecutar la batería integral de `5,695`, ampliar la
  selección, editar resultados históricos o promover fallos persistentes a
  resueltos.
- Criterio de cierre: `439/439` claves únicas, cero faltantes, cero sobrantes,
  resultados terminales completos y ausencia de consumidores bloqueados por
  productor.

## Ejecución válida y corrida descartada

Un primer intento produjo
`_nscbuilder3/history/20261010-165326-d24dbeeb`, pero se canceló limpiamente
cuando el árbol cambió durante la ejecución. Esa evidencia conserva
`43 FAIL` y `396 CANCELLED` sobre las `439` claves y no se usa para decidir la
compuerta.

Después de congelar el árbol y aprobar las validaciones dirigidas, Builder 3
cargó la corrida autoridad y ejecutó la acción visible `Reejecutar fallos`.
La corrida válida registra:

- `origin`: `historical-failures:20261010-142113-a514d35c`;
- `revision`: `ca1ef7f04f6bd924405ea40f0fc37b4efb19c5fc`;
- `variant_count`: `439`;
- `case_count`: `439`;
- huella SHA-256 del orden proyectado:
  `cfb7f2083c0d19d6f9aa96065bf627a72f081a1d0bc0cab6ecdeece9a65f982e`;
- huella SHA-256 del conjunto ordenado:
  `ca07aaa1d364f153ea8b488580fa398c181efd828104096d57527bd31ff19bc1`;
- claves faltantes: `0`;
- claves sobrantes: `0`;
- claves duplicadas: `0`.

NSC, NSLink, LLVM, CMake, Ninja, Python, el archivador, los temporales y la
evidencia se resolvieron bajo `A:`. No se ejecutó ninguna de las otras `5,256`
variantes de la corrida integral.

## Resultado terminal

La corrida terminó sin cancelación con estado global `FAILED`, porque aún
existen resultados `FAIL`. La contabilidad completa es:

| Estado final | Cantidad |
|---|---:|
| `PASS` | 212 |
| `KNOWN_FAILURE` | 6 |
| `FAIL` | 221 |
| `BLOCKED` | 0 |
| `INFRASTRUCTURE_ERROR` | 0 |
| **Total** | **439** |

## Transiciones contra la autoridad

| Estado original | Estado final | Cantidad |
|---|---|---:|
| `FAIL` | `FAIL` | 221 |
| `FAIL` | `PASS` | 53 |
| `INFRASTRUCTURE_ERROR` | `PASS` | 4 |
| `BLOCKED` | `PASS` | 155 |
| `BLOCKED` | `KNOWN_FAILURE` | 6 |
| **Total** |  | **439** |

No existe transición desde `BLOCKED` o `INFRASTRUCTURE_ERROR` hacia `FAIL`.
Los `221` fallos finales ya eran `FAIL` en la autoridad; por tanto, esta
compuerta no introdujo fallos nuevos dentro de la selección correctiva.

## Cierre causal de Tanda 1

Los cuatro productores self-host que antes terminaban en
`INFRASTRUCTURE_ERROR` ahora pasan:

- `TSH-00188|Debug|O0`;
- `TSH-00399|Debug|O0`;
- `TSH-00849|Debug|O0`;
- `TSH-00981|Debug|O0`.

Sus `161` consumidores dejaron de estar bloqueados: `155` terminaron `PASS` y
seis conservaron la clasificación explícita `KNOWN_FAILURE`:

- `TSH-00213|Debug|O0`;
- `TSH-00214|Debug|O0`;
- `TSH-00442|Debug|O0`;
- `TSH-00903|Debug|O0`;
- `TSH-01011|Debug|O0`;
- `TSH-01023|Debug|O0`.

Los cuatro casos BuildSystem dirigidos de Tanda 1 terminaron `PASS`:
`T12B-00025`, `T12B-00026`, `T12B-00029` y `T12B-00030`. También pasó
`T12B-00453`, que era un `FAIL` de la autoridad.

Resultado del gate causal: quedan `0 BLOCKED`, `0 INFRASTRUCTURE_ERROR` y
ningún bloqueo productor observable. Esto cierra la Tanda 1; no cierra las
tandas correctivas restantes.

## Dos grupos tempranos de Tanda 2 ya incorporados

La revisión congelada contiene dos correcciones de fixture pertenecientes al
inicio de Tanda 2. Esta corrida confirma sus `48` transiciones `FAIL -> PASS`:

- `8` casos de macros `TF-FRONT-409..416`, contrato `CF-FRONT-052`;
- `40` casos de `for` con bloque obligatorio en `CF-FRONT-056..063`,
  inventariados en
  `TANDA-VALIDACION-165-FIXTURE-FRONTEND-FOR-CON-BLOQUE.md`.

Estas `48` mejoras, sumadas a los cinco `PASS` de BuildSystem, explican las
`53` transiciones `FAIL -> PASS`. No implican que Tanda 2 esté cerrada: aún
persisten `66 FAIL` de Host.Frontend ajenos a esos dos grupos.

## Distribución de los 221 fallos persistentes

| Componente | `FAIL` |
|---|---:|
| Host.Compilation | 11 |
| Host.Dominance | 12 |
| Host.Driver | 18 |
| Host.Frontend | 66 |
| Host.HIR | 5 |
| Host.Interop | 16 |
| Host.LanguageService | 14 |
| Host.MIR | 18 |
| Host.Parser | 8 |
| Host.Passes | 14 |
| Host.SSA | 5 |
| Host.Sema | 33 |
| SelfHost.Basic | 1 |
| **Total** | **221** |

La tabla es una distribución observada, no una reclasificación causal. Las
tandas posteriores conservan la obligación de corregir y reejecutar sus
grupos en el orden fijado por la Validación 161.

## Evidencia conservada

- Metadatos y recibos:
  `_nscbuilder3/history/20261010-170051-1caf1c2f/metadata.json`.
- Resultados exactos:
  `_nscbuilder3/history/20261010-170051-1caf1c2f/results.json`.
- Eventos progresivos:
  `_nscbuilder3/history/20261010-170051-1caf1c2f/events.jsonl`.
- Log completo:
  `_nscbuilder3/history/20261010-170051-1caf1c2f/full.log`.
- Resumen generado:
  `_nscbuilder3/history/20261010-170051-1caf1c2f/summary.md`.
- Evidencia por variante:
  `_nscbuilder3/history/20261010-170051-1caf1c2f/cases`.

## Estado de cierre

- Tanda 1: cerrada.
- Dos grupos tempranos de Tanda 2: incorporados y confirmados en esta corrida.
- Selección correctiva pendiente: `221 FAIL` y `6 KNOWN_FAILURE` visibles; los
  `KNOWN_FAILURE` no forman parte de la siguiente reejecución de fallos.
- Corrida integral de `5,695`: no ejecutada; continúa reservada para el cierre
  de todas las tandas.
- Código o pruebas modificados por esta validación: ninguno.

