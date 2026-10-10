# Tanda 07-16 — SelfHost.Types — materialización e integración central

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente los 7 contratos y 47 casos canónicos de
  `SelfHost.Types`, último componente de la Ola 7 por el orden de
  `inventory/implementation-waves.csv`.
- **Naturaleza:** infraestructura nueva de pruebas y documentación; no modifica el compilador, Core ni el
  producto self-host.
- **Comportamiento preservado:** `SemanticTypeKind`, `OwnershipKind`, `SemanticType` y `TypeTable` conservan su
  implementación, API, errores, ownership, layout y algoritmos vigentes.
- **Fuente canónica:** `selfhost/nsc/Types/TypeSystem.ns`, auditado en Fase 1 como `PF-000620`.
- **Rango:** `TSH-00981..TSH-01027`; contratos `CSH-TYP-001..CSH-TYP-007`.
- **Archivos autorizados en la materialización:** proyecto, módulos y catálogos nuevos `SelfHostTypes*`,
  fragmento temporal `selfhost-types.jsonl`, gate aislado `run_selfhost_types_gate.py` y este Markdown.
- **Enmienda de integración central autorizada:** registrar el gate en `tests/compiler-suite/CMakeLists.txt`,
  fusionar exactamente las 36 filas ejecutables en `manifest/cases.jsonl` y retirar el fragmento temporal
  después de confirmar la fusión sin duplicados.
- **Archivos protegidos:** producto, pruebas históricas, registro C++ compartido, `TRACKING.md`, `CURSOR.json`,
  otros componentes, `C:`, remotos y los cambios concurrentes.
- **Acciones prohibidas:** build, pruebas, ejecución de gates, validación, baseline, despliegue, recursión,
  artefactos viejos, `SKIP`, `XFAIL`, oráculos vacíos y éxito sintético para fallos o casos no representables.
- **Criterio de este corte:** los 47 IDs quedan materializados exactamente una vez entre ejecutables,
  infraestructura pendiente y contradicciones; sólo los 36 ejecutables entran al manifiesto central y toda
  validación permanece pendiente.

## Denominador canónico

La Fase 1 fija 1 archivo, 7 contratos y 47 casos para `SelfHost.Types`:

| Dimensión | Conteo |
|---|---:|
| Casos canónicos | 47 |
| Perfil `PR` / `NIGHTLY` | 46 / 1 |
| Capa `unit` / `component` | 41 / 6 |
| `Required` ejecutables | 34 |
| `KnownProductDefect` ejecutables | 2 |
| `InfrastructurePending` no ejecutable | 1 |
| `PlanContradiction` no ejecutable | 10 |
| Omitidos / desconocidos | 0 / 0 |

La partición cerrada es:

```text
47 = 34 Required + 2 KnownProductDefect + 1 InfrastructurePending + 10 PlanContradiction
36 ejecutables = 34 Required + 2 KnownProductDefect
```

El único caso `NIGHTLY`, `TSH-01014`, es precisamente el caso pendiente de infraestructura. Los 36 casos
integrados en el manifiesto central conservan perfil `PR`.

## Contratos y materialización

| Contrato | Rango | Ejecutables | Pendiente | Contradicciones | Oracle real |
|---|---|---:|---:|---:|---|
| `CSH-TYP-001` | `TSH-00981..00987` | 5 | 0 | 2 | 22 kinds, 3 ownerships, identidad y display |
| `CSH-TYP-002` | `TSH-00988..00993` | 5 | 0 | 1 | todos los campos y transiciones de `SemanticType` |
| `CSH-TYP-003` | `TSH-00994..01000` | 5 | 0 | 2 | IDs, conteo, duplicados y orden exacto |
| `CSH-TYP-004` | `TSH-01001..01007` | 5 | 0 | 2 | 16 builtins con ID, nombre, kind, size y alignment |
| `CSH-TYP-005` | `TSH-01008..01014` | 6 | 1 | 0 | interning, descriptores, conteo y estabilidad |
| `CSH-TYP-006` | `TSH-01015..01020` | 4 | 0 | 2 | kind, display, elemento, count, layout y reutilización |
| `CSH-TYP-007` | `TSH-01021..01027` | 6 | 0 | 1 | firma estructural y tabla completa de predicados |

Cada caso ejecutable despacha por `case_id` en un proceso NativeSharp. El proyecto incluye directamente la
fuente real `TypeSystem.ns` y consume `NativeSharp.Core` mediante `ProjectReference`; no hay runner C++ que
duplique el algoritmo del producto.

## Correcciones explícitas del plan genérico

Cinco objetivos genéricos se ajustan al contrato vigente sin cambiar el producto ni ocultar el ajuste:

- `TSH-00990`: la ausencia de relaciones opcionales se observa mediante los sentinelas `-1` del constructor;
- `TSH-01000`: el orden de parámetros es semántico; repetición idéntica es estable y una inversión debe ser
  observable, no canonicalizada;
- `TSH-01007`: la tabla builtin no recibe entradas permutables; se compara la inicialización fija de dos tablas;
- `TSH-01010`: un nombre nominal todavía no producido se interna una vez como descriptor opaco `Named`;
- `TSH-01027`: una firma idéntica reutiliza ID y una inversión de parámetros produce una identidad distinta.

Los cinco casos llevan el tag `plan-rewrite` en el manifiesto central. La corrección cumple la regla de Fase 2 que
prohíbe conservar expectativas de capacidades inexistentes dentro de la suite activa.

## Defectos de producto conservados como pruebas rojas

### `TSH-01011` — malformed fixed-array count

El contrato de `Intern` declara que texto compuesto malformado sigue el descriptor desconocido documentado.
La implementación vigente llama `Number.TryParseInt64` sin comprobar su resultado y publica
`int[not-a-count]` como `Array` de conteo cero. El fixture exige `SemanticTypeKind.Named` y preservación exacta
del display. Su oracle es el contrato correcto; no espera el fallo y no se transforma en `XFAIL`.

### `TSH-01023` — invalid numeric predicate ID

El contrato declara `false` para un tipo inválido. `IsInteger` e `IsNumeric` llaman `Get(id)` antes de rechazar
el ID, por lo que `-1` y `Count()` no poseen hoy un retorno falso controlado. El fixture exige que ambos
predicados devuelvan `false` sin terminar el proceso. También permanece rojo hasta corregir el producto en una
tarea autorizada distinta.

El gate marca ambos como `KNOWN_PRODUCT_DEFECT` si no satisfacen el oracle y devuelve fallo. Si el producto ya
los satisface, informa `UNEXPECTED_PASS` y también exige reclasificar la evidencia. Nunca invierte el oracle.

## Infraestructura pendiente

`TSH-01014` exige inyectar un fallo después de una mutación recursiva parcial de `TypeTable`. No existe una seam
autorizada de fallo de allocation o de mutación. Provocar un crash, modificar el producto o copiar el algoritmo
en el fixture produciría evidencia falsa.

El caso queda catalogado como `InfrastructurePending`, fuera del flujo ejecutable, con resolución exacta:
una seam sólo de pruebas que permita fallar de forma determinista después de la primera transición sin cambiar
la API ni el comportamiento público. Este pendiente impide declarar cerrado el componente, pero no reduce el
denominador.

## Contradicciones del plan

Los diez IDs siguientes no describen una operación observable de la API congelada:

| IDs | Motivo |
|---|---|
| `TSH-00983`, `TSH-00984` | los enums cerrados no tienen prerequisito productor ni parser de valores malformados |
| `TSH-00991` | `SemanticType` es un carrier; el productor semántico posee la validación de relaciones |
| `TSH-00996` | ausencia de parámetros es el estado válido vacío ya cubierto, no un prerequisito rechazable |
| `TSH-00997` | un índice inválido es error del llamador y no existe rechazo recuperable observable |
| `TSH-01003`, `TSH-01004` | el constructor builtin no recibe entrada y `AddBuiltin` es privado y fijo |
| `TSH-01017`, `TSH-01018` | `StringLiteral` no tiene prerequisito ni canal de error declarado |
| `TSH-01024` | `Function` consume IDs canónicos y no expone un resultado de rechazo controlado |

`SelfHostTypesCaseClassifications.jsonl` conserva para cada ID contrato, razón y resolución requerida. Ninguno
aparece en el manifiesto central; tampoco se trata un crash o acceso fuera de rango como éxito.

## Artefactos exclusivos

- `SelfHostTypes.nsproj`: fixture consolidado contra `TypeSystem.ns` real.
- `SelfHostTypesMain.ns`: selector de un caso por proceso.
- `SelfHostTypesTestSupport.ns`: inventario builtin, kinds, ownership y predicados compartidos.
- siete módulos `SelfHostTypes*Module.ns`: uno por contrato, sin recursión.
- `SelfHostTypesCaseClassifications.jsonl`: 10 contradicciones, 1 pendiente y 2 KPD.
- `manifest/cases.jsonl`: 36 filas ejecutables fusionadas sin duplicados; el fragmento temporal se retiró.
- `run_selfhost_types_gate.py`: filtra `SelfHost.Types` desde el manifiesto central, verifica trazabilidad y
  partición, hace un build único y ejecuta cada caso seleccionado en un directorio fresco.
- `tests/compiler-suite/CMakeLists.txt`: una inscripción del controller condicionada a NSC y NSLink explícitos.

No se modificaron el registro C++ compartido, `TRACKING.md` ni `CURSOR.json`. La edición de los dos archivos
centrales se limitó a las filas y al registro de `SelfHost.Types`, sin reordenar ni reformatear trabajo concurrente.

## Algoritmos, complejidad y aislamiento

- Los recorridos de kinds y builtins son tablas explícitas o bucles iterativos acotados.
- `TablesEqual` y `NumericTruthTable` son `O(B)` tiempo y `O(1)` espacio, con `B = 16` builtins.
- El dispatcher es lineal en siete módulos y cada `Run` es una cadena acotada de IDs.
- No existe recursión en fixture ni gate.
- El gate compila una vez por modo/optimización y ejecuta cada caso seleccionado con un cwd fresco.
- No existe fallback a binarios, manifiestos o resultados anteriores.

## Estado exacto

No se ejecutaron builds, pruebas, gates, runners ni validaciones. Tampoco se hizo una afirmación de
compilabilidad o aprobación. El estado exacto es:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 47/47 CLASIFICADOS; 36 EJECUTABLES CENTRALES,
1 INFRASTRUCTUREPENDING Y 10 CONTRADICCIONES FUERA DEL FLUJO; 2 KPD CONSERVADOS COMO PRUEBAS ROJAS;
VALIDACIÓN PENDIENTE`.
