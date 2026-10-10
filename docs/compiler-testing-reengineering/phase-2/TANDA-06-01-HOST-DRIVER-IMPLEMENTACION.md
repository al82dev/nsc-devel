# Tanda 06-01 — Host.Driver — implementación

## Alcance

- Componente canónico: `Host.Driver`.
- Contratos: `CF-DRIVER-001..077`.
- Casos materializados: `TF-DRIVER-001..616`.
- Distribución canónica: 240 unitarios, 376 de integración; 539 `PR`, 77 `NIGHTLY`.
- Fuentes de autoridad: `HOST-DRIVER-TEST-PLAN.md`, `test-case-design.csv`,
  `suite-case-allocation.csv` y el producto vigente bajo `include/nativesharp/Driver` y `src/Driver`.

## Implementación materializada

| Superficie | Contratos | Casos | Estado |
|---|---:|---:|---|
| Oráculo público ejecutable | 33 | 264 | materializado, no validado |
| Infraestructura especial o seam privado | 44 | 352 | `InfrastructurePending` visible |
| Total | 77 | 616 | materializado, no integrado |

Los 264 casos ejecutables se publican en
`tests/compiler-suite/manifest/cases.jsonl`. El fragmento temporal fue eliminado después de confirmar la fusión
sin duplicados y el gate quedó registrado en CMake.

## Contratos ejecutables

- `CF-DRIVER-001..020`: documentos de compatibilidad, scheduling, trabajos, perfiles, telemetría, workers,
  consola, crash boundary y deprecación.
- `CF-DRIVER-024`: despacho superior de ayuda y comando inválido.
- `CF-DRIVER-044..054`: análisis/migración y contratos documentales de baseline, compatibilidad, numericidad,
  toolchain, freeze y promoción.
- `CF-DRIVER-077`: temas `explain`, salida texto/JSON y delegación del contrato de optimización.

## Infraestructura pendiente conservada

- `CF-DRIVER-021..023`: bundle diagnóstico con redacción/fault injection y extracción privada de opciones.
- `CF-DRIVER-025..043`: pipeline completo, LLVM, NSLink, archivos, metadata, ABI/API y publicación.
- `CF-DRIVER-055..062`: instalación autenticada, reproducibilidad, actualización, remoción y rollback.
- `CF-DRIVER-063..064`: parser JSON y modelo sidecar privados de `ProjectCommands.cpp`.
- `CF-DRIVER-065..076`: templates, restauración, paquetes, proceso Windows, clean y publicación transaccional.

Cada caso pendiente permanece registrado en los módulos con `TestDisposition::InfrastructurePending` y una causa
específica. No se convierte en `PASS`, `SKIP` ni resultado vacío. Sólo los casos ejecutables forman parte del
fragmento ordinario.

## Artefactos

- `DriverTestSupport.h`.
- `DriverFoundation001To096Module.cpp`.
- `DriverRuntime097To192Module.cpp`.
- `DriverPipeline193To344Module.cpp`.
- `DriverMigration345To432Module.cpp`.
- `DriverInstallation433To496Module.cpp`.
- `DriverProject497To616Module.cpp`.
- 264 filas ejecutables en `manifest/cases.jsonl`.
- `controller/integration/run_host_driver_gate.py`.

## Validación

No ejecutada por orden de la Fase 2. Esta tanda no compiló ni ejecutó pruebas o gates. Los seis registros y módulos,
las fuentes de producto estrictamente requeridas, las 264 filas ejecutables, el gate y el estado persistente quedaron
integrados sin modificar el producto. Estado exacto:
`IMPLEMENTACIÓN E INTEGRACIÓN MATERIALIZADAS; VALIDACIÓN PENDIENTE`.
