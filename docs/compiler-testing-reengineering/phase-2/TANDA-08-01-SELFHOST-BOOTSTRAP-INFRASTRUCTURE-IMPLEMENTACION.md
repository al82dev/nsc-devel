# Tanda 08-01 — SelfHost.Bootstrap.Infrastructure — implementación

## SCOPE LOCK operativo

- **Objetivo:** materializar e integrar los 73 contratos `C12B-BSI-001..073` y los 438 casos
  `T12B-01081..01518`.
- **Naturaleza:** infraestructura de pruebas de Fase 2; producto y verificadores existentes sin cambios.
- **Archivos autorizados:** catálogo, soporte aislado, seis módulos, gate estructural, integración central mínima,
  tracking, cursor y este Markdown.
- **Protegidos:** producto, históricos, coverage, bibliotecas externas, `C:`, remotos y los seis archivos
  protegidos de la Fase 1.
- **Prohibido:** ejecutar verificadores de producto, build, pruebas, gates, bootstrap, despliegue y generaciones
  o cachés viejos como fallback.
- **Criterio:** 438/438 IDs materializados con metadata canónica, disposición visible, integración estructural,
  cero filas ordinarias y validación pendiente.

## Denominador exacto

| Magnitud | Total |
|---|---:|
| Archivos inventariados | 74 |
| Archivos EXCLUIDA | 1 (`selfhost/bootstrap/__init__.py`) |
| Productores INDIRECTA | 73 |
| Contratos | 73 |
| Casos | 438 |
| Productores de datos normativos | 16 |
| Verificadores Python | 57 |
| Perfil operativo NIGHTLY | 438 |
| Capa self-host | 438 |
| InfrastructurePending | 438 |
| Ejecutables ordinarios / KPD | 0 / 0 |

## Materialización

- `SelfHostBootstrapInfrastructureCatalog.jsonl`: 73 productores, rutas, categorías, lenguaje, propósito,
  oráculo y seis IDs por contrato.
- `selfhost_bootstrap_infrastructure_support.py`: lector tipado, iterativo y defensivo del catálogo.
- `SelfHostBootstrapInfrastructureTestSupport.h`: registro iterativo de casos y clases de entrada.
- Seis módulos `SelfHostBootstrapInfrastructure*Module.cpp`: 438 casos sin omisiones.
- `run_selfhost_bootstrap_infrastructure_gate.py`: gate estructural futuro del denominador.
- `SelfHostBootstrapInfrastructurePlanContradictions.jsonl`: contradicción de perfiles abierta.

## Disposición y dependencia

Los 16 datos normativos no son pruebas autónomas: sus oráculos exigen correspondencia exacta con productores y
consumidores. Los 57 verificadores requieren procesos reales y observación de reportes, hashes, etapas, tiempos
y códigos de salida. Los 73 contratos declaran como dependencias `nsc0`, `nsc1`, `nsc2`, NSLink, LLVM,
Python y contratos self-host.

La suite nueva todavía no posee el adaptador que gobierna esa clausura sin ejecutar el bootstrap ni consumir
generaciones anteriores. Reimplementar parsers o verificadores dentro del arnés probaría un sustituto. Por ello
los 438 casos quedan `InfrastructurePending` con cuerpo defensivo rojo si se invocan directamente, y el
manifiesto central conserva cero filas.

## Contradicción conservada

El plan publica 365 casos `PR` y 73 `NIGHTLY`; `suite-case-allocation.csv` asigna los 438 a `NIGHTLY`.
La elección operativa es el inventario de asignación, autoridad ejecutable de Fase 1. La discrepancia permanece
abierta y no se normalizó silenciosamente.

## Integración

Los seis registros y módulos están conectados en `TestRegistry.h`, `main.cpp` y CMake. El gate estructural
está registrado, no existe fragmento ejecutable y no hay filas del componente en el manifiesto central. Todos
los recorridos son iterativos; el registro cuesta tiempo `O(438)` y memoria propia constante aparte de las
entradas almacenadas.

No se ejecutaron builds, pruebas, gates ni verificadores. Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 438/438 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.

