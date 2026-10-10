# Tanda 08-02 — SelfHost.Contracts — materialización aislada

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente `C12B-SHC-001..048` y
  `T12B-01519..01806`.
- **Naturaleza:** infraestructura nueva de pruebas; cero cambios funcionales al compilador o al bootstrap.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostContracts*`, catálogo, gate estructural,
  integración central mínima y este Markdown.
- **Archivos protegidos:** producto, tracking, cursor, batería histórica, `C:`, remotos y los seis archivos
  protegidos de la Fase 1.
- **Acciones prohibidas:** build, pruebas, gates, baseline, recursión, despliegue, PASS sintético y consumo de
  artefactos viejos.
- **Criterio de este corte:** 288/288 IDs materializados con contrato, fuente, capa y perfil canónicos;
  infraestructura ausente explícita; cero filas ejecutables y validación pendiente.

## Denominador

| Dimensión | Conteo |
|---|---:|
| Contratos | 48 |
| Casos totales | 288 |
| Perfil operativo NIGHTLY | 288 |
| Capa self-host | 288 |
| InfrastructurePending | 288 |
| Ejecutables ordinarios | 0 |
| KnownProductDefect | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Cobertura contractual materializada

- `C12B-SHC-001..012`, `T12B-01519..01590`: toolchain, memoria, backend, determinismo y primera generación.
- `C12B-SHC-013..024`, `T12B-01591..01662`: frontend, equivalencia, AOT, interoperabilidad, IR y texto nativo.
- `C12B-SHC-025..036`, `T12B-01663..01734`: object model, performance, promoción, robustez y runtime.
- `C12B-SHC-037..048`, `T12B-01735..01806`: semántica y contratos source hasta código especializado.

Cada contrato conserva sus seis clases canónicas: `valid-minimal`, `valid-representative`,
`invalid-precondition`, `invalid-structure`, `boundary` y `determinism`. El catálogo preserva la ruta
productora y el oráculo `campos tipados, cardinalidades, paths y expectativas declaradas`.

## Infraestructura pendiente

Los 288 casos dependen de `SelfHost.Bootstrap.Infrastructure`, fixtures self-host y productos SH41. La suite
nueva todavía no posee una frontera ejecutable que cargue los contratos tipados, resuelva sus referencias,
produzca los artefactos declarados y compare cardinalidades, paths y expectativas sin usar caches o productos
viejos. Ejecutar una copia del contrato dentro del arnés probaría un sustituto.

Por ello los 288 casos se registran como `InfrastructurePending`; su cuerpo defensivo falla si se invoca
directamente. El fragmento ejecutable conserva cero filas y no existe evidencia para afirmar defectos del
producto.

## Contradicción conservada

El plan publica 240 casos `PR` y 48 `NIGHTLY`, mientras
`inventory/suite-case-allocation.csv` asigna los 288 a `NIGHTLY`. La elección operativa es el inventario de
asignación, autoridad de ejecución de la Fase 1. La discrepancia permanece abierta en
`SelfHostContractsPlanContradictions.jsonl`; no se normalizó silenciosamente.

## Artefactos

- `SelfHostContractsTestSupport.h`: IDs T12B, clases, metadata y disposición pendiente.
- Cuatro módulos `SelfHostContracts*Module.cpp`: 72 casos y doce contratos cada uno.
- `SelfHostContractsCatalog.jsonl`: 48 productores y sus 288 IDs.
- `manifest/cases.jsonl`: cero filas ejecutables por dependencia pendiente; el fragmento vacío fue retirado.
- `run_selfhost_contracts_gate.py`: gate estructural futuro del denominador y la contradicción.
- `SelfHostContractsPlanContradictions.jsonl`: discrepancia de perfil abierta.

Todos los recorridos son iterativos. Complejidad de registro: tiempo `O(288)` y memoria propia constante aparte
de las entradas almacenadas.

## Estado

Los cuatro registros y módulos están conectados en `TestRegistry.h`, `main.cpp` y CMake. El gate estructural
quedó registrado, el fragmento vacío fue retirado y el manifiesto central conserva cero filas del componente.
Tracking y cursor se actualizarán al cerrar los dos componentes de la Ola 8. No se ejecutaron builds, pruebas,
gates ni runners. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 288/288 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.

