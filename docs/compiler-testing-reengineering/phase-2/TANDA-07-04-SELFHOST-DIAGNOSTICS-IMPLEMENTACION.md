# Tanda 07.04 — implementación real de SelfHost.Diagnostics

## 1. Alcance canónico

El sucesor inmediato de `SelfHost.CodeGen` en `inventory/implementation-waves.csv` es
`SelfHost.Diagnostics`.

| Magnitud | Total |
|---|---:|
| Archivos de producto inventariados | 1 |
| Contratos | 4 |
| Casos | 28 |
| IDs | `TSH-00188..TSH-00215` |
| Capa `unit` | 28 |
| Perfil `PR` | 28 |

Todos los casos están asignados al ejecutor `nsc_suite_controller+nsc` y a la familia
`ns/selfhost/selfhost-diagnostics`.

## 2. Materialización reemplazada

- `SelfHostDiagnosticsRecordsModule.ns`: `TSH-00188..TSH-00201` para registro y catálogo reales.
- `SelfHostDiagnosticsRenderingModule.ns`: `TSH-00202..TSH-00215` para salida humana y JSON reales.
- `SelfHostDiagnosticsMain.ns`: despacho de un caso exacto por ID.
- `SelfHostDiagnostics.nsproj`: fixture externa que compila el producto `Diagnostics.ns` y sus productores
  mínimos.

Cada contrato aporta siete casos. La ruta C++ que sólo marcaba pendientes fue retirada por clasificación
incorrecta: sus dos registros, llamadas y fuentes dejaron de pertenecer al host consolidado.

## 3. Contratos y oracles

| Contrato | IDs | Oracle |
|---|---|---|
| `CSH-DIA-001` | `TSH-00188..00194` | valores exactos y lifetime de copia |
| `CSH-DIA-002` | `TSH-00195..00201` | código, severidad, argumento y exit exactos |
| `CSH-DIA-003` | `TSH-00202..00208` | campos semánticos normalizados, sin locale incidental |
| `CSH-DIA-004` | `TSH-00209..00215` | parsear JSON y comparar campos y escaping exactos |

## 4. Clasificación honesta

- `Required`: 26.
- `InfrastructurePending`: 0.
- `KnownProductDefect`: 2 (`TSH-00213`, `TSH-00214`).
- Ejecutables: 28.
- Filas centrales: 28.
- PASS afirmados: 0.

La infraestructura existente de `SelfHost.Basic` proporciona la ruta real: recibe `nsc.exe` y
`nslink.exe`, compila un `.nsproj` externo y ejecuta un proceso aislado por caso. No existe por ello un
bloqueo de infraestructura para Diagnostics.

Los dos defectos conservan oráculo correcto y nunca se informan como PASS:

- `TSH-00213`: `JsonEscape` no escapa tabulador ni todos los caracteres de control; el resultado no es JSON
  válido.
- `TSH-00214`: `Json` fija la severidad a `error` y pierde la identidad `Warning`.

## 5. Fragmento y gate

Las 28 filas fueron fusionadas en `manifest/cases.jsonl` y el fragmento temporal fue retirado después de
confirmar 28 IDs únicos, 26 Required y 2 KPD. El gate `run_selfhost_diagnostics_gate.py`, registrado como
`nsc.compiler-suite.selfhost-diagnostics-controller`, compila la fixture real y evalúa cada oráculo.
Un KPD que sigue fallando se registra como `KNOWN_PRODUCT_DEFECT` y mantiene el gate rojo; si el producto
empieza a cumplir el oráculo, se registra `UNEXPECTED_PASS` hasta reclasificarlo.

## 6. Exclusiones y validación

No se tocaron producto, históricos, coverage, bibliotecas externas, C:, remotos, `TRACKING.md` ni
`CURSOR.json`. No se ejecutaron build, pruebas, gates o validación. El estado exacto es:

`IMPLEMENTACIÓN E INTEGRACIÓN ESTÁTICAS COMPLETAS; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.
