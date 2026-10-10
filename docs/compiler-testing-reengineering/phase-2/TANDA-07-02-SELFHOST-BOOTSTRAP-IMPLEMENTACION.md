# Tanda 07.02 — materialización aislada de SelfHost.Bootstrap

## 1. Alcance y fuente canónica

Esta tanda materializa exclusivamente `SelfHost.Bootstrap`, componente inmediatamente posterior a
`SelfHost.Basic` en `inventory/implementation-waves.csv`. Se respetaron el SCOPE LOCK de Fase 2,
el inventario, la matriz de asignación, los contratos y el plan de pruebas de Fase 1.

Denominador canónico:

| Magnitud | Total |
|---|---:|
| Archivos de producto inventariados | 1 |
| Contratos | 3 |
| Casos | 23 |
| Capa `component` | 16 |
| Capa `integration` | 7 |
| Perfil `PR` | 18 |
| Perfil `NIGHTLY` | 5 |

Los casos son `TSH-00029..TSH-00051`. La partición contractual exacta es 8 para
`CSH-BOO-001`, 8 para `CSH-BOO-002` y 7 para `CSH-BOO-003`.

## 2. Materialización

Se añadieron dos módulos consolidados y un soporte común:

- `SelfHostBootstrapProfileModule.cpp`: `TSH-00029..TSH-00044`.
- `SelfHostBootstrapBuildModule.cpp`: `TSH-00045..TSH-00051`.
- `SelfHostBootstrapTestSupport.h`: identidad, trazabilidad y disposición no ejecutable.

Cada ID aparece una vez con contrato, capa, perfil, oracle y requisito de infraestructura. Los dos
registros están conectados a `TestRegistry.h` y `main.cpp`, y sus fuentes al target consolidado de
`CMakeLists.txt`.

El fragmento temporal `manifest/fragments/selfhost-bootstrap.jsonl` se inspeccionó antes de
retirarlo: contenía cero filas ejecutables y sólo dos comentarios. No se agregaron filas al
manifiesto central porque los 23 casos continúan pendientes de infraestructura.

## 3. Trazabilidad contrato-oracle

| Contrato | Casos | Oracle canónico | Clasificación |
|---|---|---|---|
| `CSH-BOO-001` | `TSH-00029..00036` | matriz aceptar/rechazar y primer constructo rechazado | `InfrastructurePending` |
| `CSH-BOO-002` | `TSH-00037..00044` | modelo normalizado o categoría exacta de rechazo | `InfrastructurePending` |
| `CSH-BOO-003` | `TSH-00045..00051` | secuencia de etapas, salidas, diagnósticos y código | `InfrastructurePending` |

## 4. Clasificación honesta

Resultado de la inspección estática:

- `InfrastructurePending`: 23.
- `KnownProductDefect`: 0.
- Ejecutables: 0.
- PASS afirmados: 0.

`GenerationBuilder.VerifySource` y `GenerationBuilder.VerifyManifest` son métodos privados. El
ejecutor `nsc_suite_controller+nsc` vigente sólo implementa cuatro operaciones de identidad de
`Host.Basic`; no carga el grafo self-host, no llama esos verificadores ni invoca `Build` con
productor, manifiesto, salidas, traza y captura diagnóstica aislados. Copiar los algoritmos en la
suite probaría un sustituto y no el producto.

`GenerationBuilder.Build` escribe actualmente `source_derived_codegen=false`, estado `blocked`,
razón `compiler-codegen-incomplete`, y retorna 78 después de validar entradas suficientes. Esta
limitación se conserva como evidencia, pero no se cuenta como `KnownProductDefect` de un caso: sin
el ejecutor requerido no existe todavía una observación reproducible del oracle de cada ID. Tampoco
se interpreta como PASS.

## 5. Gate aislado

`run_selfhost_bootstrap_gate.py` queda preparado para comprobar, sin ejecutar el producto:

1. el denominador canónico de 23 casos y 3 contratos;
2. la secuencia exacta `TSH-00029..TSH-00051`;
3. la distribución 8/8/7, 16/7 y 18/5;
4. la disposición `InfrastructurePending` de los módulos;
5. la ausencia del fragmento vacío retirado y de promoción al manifiesto ejecutable;
6. la ausencia de una clasificación `KnownProductDefect` sin oracle ejecutable.

El gate está registrado en CMake como `nsc.compiler-suite.selfhost-bootstrap-structure`. Es
deliberadamente estructural: no solicita un binario, no ejecuta el producto y no exige PASS.

## 6. Exclusiones y validación

No se tocaron producto, batería histórica, coverage, bibliotecas externas, repositorios externos,
instalaciones, `C:` ni remotos. No se ejecutaron build, pruebas, gates ni validaciones. Por tanto, el
cierre es de materialización estática; la compilación y toda validación dinámica quedan pendientes.
