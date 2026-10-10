# Tanda 07-13 — SelfHost.Source — materialización e integración central

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente los 8 contratos y 61 casos canónicos de
  `SelfHost.Source`, componente inmediatamente posterior a `SelfHost.Sema` en la Ola 7.
- **Naturaleza:** infraestructura nueva de pruebas y documentación; no es una corrección ni una modificación
  del compilador, Core, Core.IO o del producto self-host.
- **Comportamiento preservado:** `ComptimeExpression`, `ComptimeProcessor`, `DiagnosticPath` y `SourceBuffer`
  conservan API, errores, ownership, formatos, algoritmos y conducta observable.
- **Revisión conocida:** inventario de Fase 1 auditado sobre `65835716712f0d04537aa92591f766fb65ff641b`.
- **Rango evaluado:** no aplica un rango de commits; la autoridad es el plan y las matrices de Fase 1 vigentes.
- **Fuentes canónicas:** `selfhost/nsc/Source/ComptimeProcessor.ns` (`PF-000616`) y
  `selfhost/nsc/Source/SourceBuffer.ns` (`PF-000617`).
- **Rango:** `TSH-00849..TSH-00909`; contratos `CSH-SRC-001..CSH-SRC-008`.
- **Archivos autorizados en la materialización:** proyecto, módulos y catálogo nuevos `SelfHostSource*`, fragmento
  temporal `selfhost-source.jsonl`, gate aislado `run_selfhost_source_gate.py` y este Markdown.
- **Enmienda de integración central autorizada:** registrar el gate en `tests/compiler-suite/CMakeLists.txt`,
  fusionar exactamente 48 filas en `manifest/cases.jsonl` y retirar el fragmento tras confirmar cero duplicados.
- **Archivos protegidos:** producto, Core, Core.IO, pruebas históricas, registro C++ compartido, `TRACKING.md`,
  `CURSOR.json`, otros componentes, `C:`, remotos y cambios concurrentes.
- **Pruebas que deben permanecer idénticas:** las 684 históricas y toda prueba existente; no se editó ninguna.
- **Acciones prohibidas:** build, pruebas, ejecución de gates, validación, baseline, despliegue, recursión nueva,
  artefactos viejos, `SKIP`, `XFAIL`, oráculos vacíos y éxito sintético.
- **Criterio de terminación de este corte:** 61/61 IDs clasificados exactamente entre ejecutables,
  `InfrastructurePending` y `PlanContradiction`; sólo los 48 ejecutables entran al manifiesto central y el KPD
  conserva su prueba roja.

## Selección canónica y denominador

`inventory/implementation-waves.csv` ordena la cola relevante de la Ola 7 como:

```text
SelfHost.Sema -> SelfHost.Source -> SelfHost.Symbols -> SelfHost.Syntax -> SelfHost.Types
```

Por tanto, el componente solicitado es `SelfHost.Source`, no uno de sus sucesores ni un componente ya trabajado.
La Fase 1 fija 2 archivos, 8 contratos y 61 casos:

| Dimensión | Conteo |
|---|---:|
| Casos canónicos | 61 |
| Perfil `PR` / `NIGHTLY` original | 51 / 10 |
| Capa `unit` / `component` original | 36 / 25 |
| `Required` ejecutables | 47 |
| `KnownProductDefect` ejecutable | 1 |
| `InfrastructurePending` no ejecutables | 2 |
| `PlanContradiction` no ejecutables | 11 |
| Omitidos / desconocidos | 0 / 0 |

La partición cerrada es:

```text
61 = 47 Required + 1 KnownProductDefect + 2 InfrastructurePending + 11 PlanContradiction
48 ejecutables = 42 PR + 6 NIGHTLY = 28 unit + 20 component
```

## Contratos y materialización

| Contrato | Rango | Ejecutables | Pendientes | Contradicciones | Oracle público real |
|---|---|---:|---:|---:|---|
| `CSH-SRC-001` | `TSH-00849..00855` | 6 | 0 | 1 | éxito, valor y consumo total de `Evaluate` |
| `CSH-SRC-002` | `TSH-00856..00863` | 7 | 0 | 1 | validez y valor de primarios/unarios por `Evaluate` |
| `CSH-SRC-003` | `TSH-00864..00871` | 7 | 0 | 1 | precedencia completa, fallo y consumo total |
| `CSH-SRC-004` | `TSH-00872..00878` | 5 | 0 | 2 | bytes exactos y longitud de `Transform` |
| `CSH-SRC-005` | `TSH-00879..00886` | 6 | 1 | 1 | reemplazo/padding exactos o entrada intacta |
| `CSH-SRC-006` | `TSH-00887..00893` | 5 | 0 | 2 | vectores absolutos exactos, independientes del cwd del invocador |
| `CSH-SRC-007` | `TSH-00894..00900` | 5 | 0 | 2 | raw/text, longitudes, líneas y mapeo de offsets |
| `CSH-SRC-008` | `TSH-00901..00909` | 7 | 1 | 1 | identidad, bytes, hash, líneas y ausencia de publicación fallida |

Cada ejecutable despacha un único `case_id` en un proceso NativeSharp y recibe la ruta absoluta de su sandbox.
El proyecto compila directamente las dos fuentes reales de Source, `Identifiers.ns` y `StableHash.ns`, y referencia
los proyectos reales `NativeSharp.Core` y `NativeSharp.Core.IO`. No hay runner C++ ni copia de los algoritmos.

## Reescrituras explícitas del plan genérico

Seis filas genéricas sí poseen una observación pública concreta y se materializan contra ella con tag
`plan-rewrite`:

- `TSH-00852`: token numérico malformado observado como `Evaluate == false`;
- `TSH-00858`: expresión vacía como ausencia concreta de primario;
- `TSH-00866`: entrada sólo whitespace como ausencia concreta de operando de precedencia;
- `TSH-00882`: `#run` inválido permanece byte por byte sin publicación falsa;
- `TSH-00890`: `Path.GetFullPath("")` produce el fallo documentado `""`;
- `TSH-00906`: las variantes nombradas reales son `Load` y `LoadFrontend`.

No se inventaron cursores públicos, diagnósticos, enums, modos ni rechazos que el producto no expone.

## Defecto de producto conservado como prueba roja

### `TSH-00903` — carga de archivo ausente

El contrato reconstructivo declara que un fallo de lectura no publica un `SourceBuffer` aceptado. Sin embargo,
`File.ReadAllText` colapsa archivo ausente y archivo vacío al mismo string `""`, y `SourceBuffer.Load` construye
siempre un buffer con ese contenido. El fixture exige `null` para la ruta ausente y por ello conserva el oracle
correcto; no espera el fallo ni lo transforma en `XFAIL`.

El gate clasifica un incumplimiento como `KNOWN_PRODUCT_DEFECT` y devuelve fallo. Si el producto empieza a cumplir
el oracle, informa `UNEXPECTED_PASS` y también devuelve fallo hasta reclasificar la evidencia.

## Infraestructura pendiente

- `TSH-00885`: fallo determinista después de la primera mutación del `StringBuilder` de `Transform`.
- `TSH-00908`: fallo determinista después de leer el archivo y antes de publicar el `SourceBuffer`.

Ambos requieren una seam de fallo sólo para pruebas. Provocar agotamiento real, crash, lectura parcial no
controlada, modificar el producto o copiar el algoritmo produciría evidencia falsa. Quedan en
`SelfHostSourceCaseClassifications.jsonl` con resolución explícita y fuera del flujo ejecutable.

## Contradicciones del plan

| IDs | Motivo |
|---|---|
| `TSH-00851` | `ComptimeExpression` no tiene un productor opcional que pueda omitirse |
| `TSH-00863` | no existe límite documentado de profundidad; fabricarlo ejercería recursión sin umbral contractual |
| `TSH-00870` | `Evaluate` no publica modelo mutable ni transición owned parcial |
| `TSH-00874`, `TSH-00875` | los helpers privados de marcador no poseen prerrequisito ni relación malformable |
| `TSH-00881` | `Transform` sólo recibe el string requerido y no posee productor separable |
| `TSH-00889`, `TSH-00892` | `DiagnosticPath` no posee prerrequisito opcional, modo, enum ni variante ABI |
| `TSH-00896`, `TSH-00897` | toda entrada textual de normalización tiene conducta; no hay canal de rechazo de relaciones |
| `TSH-00904` | el plan no identifica una relación de carga malformada y añadir validación de `FileId` inventaría contrato |

Estas 11 filas no entran al flujo ejecutable. El catálogo conserva contrato, causa y resolución, sin tratarlas
como features futuras ni ampliar el producto.

## Artefactos exclusivos

- `SelfHostSource.nsproj`: fixture consolidado contra las fuentes reales.
- `SelfHostSourceMain.ns`: dispatcher de un caso por proceso con sandbox explícito.
- `SelfHostSourceTestSupport.ns`: comparadores públicos sin duplicar algoritmos.
- ocho módulos `SelfHostSource*Module.ns`: uno por contrato, sin recursión añadida.
- `SelfHostSourceCaseClassifications.jsonl`: 11 contradicciones, 2 pendientes y 1 KPD.
- `manifest/cases.jsonl`: 48 filas ejecutables fusionadas sin duplicados; el fragmento temporal se retiró.
- `run_selfhost_source_gate.py`: filtra `SelfHost.Source` desde el manifiesto central, comprueba trazabilidad y
  partición, prepara archivos deterministas, hace un build único y ejecuta en directorio fresco por caso.
- `tests/compiler-suite/CMakeLists.txt`: una inscripción del controller condicionada a NSC y NSLink explícitos.

No se modificaron el registro C++ compartido, `TRACKING.md` ni `CURSOR.json`. La edición de los archivos centrales
se limitó a las filas y al registro de `SelfHost.Source`, sin reordenar ni reformatear trabajo concurrente.

## Algoritmos, complejidad y aislamiento

- Los generadores de escala del fixture son bucles iterativos: `O(n)` tiempo y `O(n)` memoria para construir
  expresiones o fuentes; no agregan recursión.
- Los oráculos comparan valores o bytes públicos; no reimplementan parsing, transformación, normalización,
  hashing ni mapeo de líneas.
- El gate compila una vez por modo/optimización, crea archivos con bytes CR/LF explícitos y ejecuta cada caso en
  un cwd nuevo.
- El gate valida mecánicamente el denominador, perfiles, capas, clasificaciones y tags KPD antes de compilar.
- No existe fallback a binarios, manifiestos, resultados o archivos de otra corrida.

## Estado exacto

No se ejecutaron builds, pruebas, gates, runners ni validaciones. Tampoco se afirma compilabilidad o aprobación.
El estado es:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 61/61 CLASIFICADOS; 48 EJECUTABLES CENTRALES,
2 INFRASTRUCTUREPENDING Y 11 PLANCONTRADICTION FUERA DEL FLUJO; 1 KPD CONSERVADO COMO PRUEBA ROJA;
VALIDACIÓN PENDIENTE`.
