# Tanda 07-01 — SelfHost.Basic — materialización e integración

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente los cuatro contratos y 28 casos canónicos de
  `SelfHost.Basic` definidos por la Fase 1.
- **Naturaleza:** infraestructura nueva de pruebas y documentación; no modifica el compilador ni sus
  bibliotecas básicas.
- **Comportamiento preservado:** `FileId`, `SourcePosition`, `SourceRange`, `CompilerResult`,
  `GenerationArena`, `ArenaHandle` y `StableHash` conservan exactamente su implementación vigente.
- **Archivos autorizados:** proyecto y módulos nuevos `SelfHostBasic*`, fragmento y manifiesto consolidado,
  gate aislado, integración CMake mínima, tracking, cursor, catálogo de contradicciones y este Markdown.
- **Archivos protegidos:** producto, batería histórica, registro C++ del host, `C:`, remotos y los seis cambios
  preexistentes de Fase 1.
- **Acciones prohibidas:** build, pruebas, gates, validación, baseline, despliegue, recursión, fallback con
  artefactos viejos y éxito sintético para casos no representables.
- **Criterio de este corte:** 28 IDs trazados una sola vez; 27 casos ejecutables materializados y una
  contradicción del plan preservada explícitamente; cero omisiones y validación dinámica pendiente.

## Denominador canónico

| Dimensión | Conteo |
|---|---:|
| Contratos | 4 |
| Casos totales | 28 |
| Casos `PR` / `NIGHTLY` | 26 / 2 |
| Casos unitarios | 28 |
| Ejecutables ordinarios | 27 |
| Contradicciones del plan | 1 |
| `InfrastructurePending` | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Contratos y oráculos

| Contrato | Casos | Superficie | Oráculo observable | Disposición |
|---|---|---|---|---|
| `CSH-BAS-001` | `TSH-00001..00008` | identidades, posiciones, rangos y resultados | campos, validez, igualdad, longitud y exit code exactos | `Required` |
| `CSH-BAS-002` | `TSH-00009..00015` | arena generacional y handles | dirección, generación, tamaño, bytes usados y rechazo de handles inválidos | `Required` |
| `CSH-BAS-003` | `TSH-00016..00022` | hash FNV-1a de texto | vectores exactos, repetibilidad y sensibilidad al orden | 6 `Required` + 1 `PlanContradiction` |
| `CSH-BAS-004` | `TSH-00023..00028` | lifetime de valores y vistas de arena | transición de generación y frontera de vigencia | `Required` |

Los casos ejercitan las fuentes reales de `selfhost/nsc/Basic`; no copian sus algoritmos dentro de un runner
C++. El proyecto NativeSharp consolidado despacha un único `case_id` por proceso y devuelve cero únicamente
cuando el oráculo completo de ese caso se cumple.

## Contradicción preservada

`TSH-00018` pide comprobar que `StableHash.Text(string)` rechace en una frontera propietaria un prerrequisito
ausente sin publicar resultado parcial. La superficie vigente sólo acepta un `string` no nulo y devuelve un
`ulong`; no expone resultado discriminado, diagnóstico ni otro canal de rechazo observable.

Provocar una desreferencia nula o aceptar un crash como éxito falsearía el contrato. El caso permanece en
`SelfHostBasicPlanContradictions.jsonl` con disposición `PlanContradiction` y no aparece en el fragmento
ejecutable. Su resolución correcta requiere reescribir el caso canónico contra una conducta observable actual;
no requiere ni autoriza cambiar el producto.

## Artefactos materializados

- `SelfHostBasic.nsproj`: proyecto consolidado con las tres fuentes reales de `SelfHost.Basic`.
- `SelfHostBasicMain.ns`: selector iterativo de un caso por proceso.
- `SelfHostBasicIdentifiersModule.ns`: ocho casos de `CSH-BAS-001`.
- `SelfHostBasicArenaModule.ns`: siete casos de `CSH-BAS-002`.
- `SelfHostBasicStableHashModule.ns`: seis casos ejecutables de `CSH-BAS-003`.
- `SelfHostBasicLifetimeModule.ns`: seis casos de `CSH-BAS-004`.
- `SelfHostBasicPlanContradictions.jsonl`: clasificación explícita de `TSH-00018`.
- `manifest/cases.jsonl`: 27 filas ejecutables fusionadas, sin `TSH-00018`.
- `run_selfhost_basic_gate.py`: construcción única del fixture, selección por perfil y ejecución aislada por ID.

El gate exige rutas explícitas de NSC y NSLink, compila el fixture una sola vez por combinación de modo y
optimización, ejecuta cada caso seleccionado por separado y conserva stdout, stderr, exit code y duración. No
consulta ni modifica la batería histórica. Todos los recorridos son iterativos y no existe recursión.

## Estado de integración y validación

La integración central fusiona las 27 filas ejecutables en el manifiesto consolidado, registra el controlador
en CMake sólo cuando existen rutas explícitas de NSC y NSLink, y actualiza tracking y cursor. Una inspección
declarativa confirmó 27 IDs únicos de componente, ausencia de `TSH-00018` y cero duplicados globales; después
de esa confirmación se retiró el fragmento temporal. `TestRegistry.h` y `main.cpp` no cambian porque los casos
son programas NativeSharp ejecutados por proceso, no módulos C++ del host.

No se ejecutaron builds, pruebas, gates, runners ni validación dinámica. La conclusión exacta es:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 28/28; 27 EJECUTABLES Y 1 CONTRADICCIÓN DEL PLAN; VALIDACIÓN DINÁMICA PENDIENTE`.
