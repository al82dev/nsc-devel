# Congelamiento 3.0 D-10 — `nsc` deja de escribir `error*.log` en el directorio de trabajo

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `837325b27`.

## Propósito y problema

Hallazgo D-10 de la auditoría del congelamiento: cuando `nsc` falla escribe `error.log`, `error1.log`, ... en el
directorio de trabajo (`publishAutomaticErrorLog`, `src/nsc/main.cpp`). Así se llenaron `C:\python` (6,309
archivos) y `C:\Windows\System32\WindowsPowerShell\v1.0` (42). Decisión de Alberto Huerta Luna (DEC-01,
2026-10-26): no se borra nada de `C:` y se corrige D-10 para que `nsc` no genere esos archivos en el directorio de
trabajo; él limpia los externos por separado.

## SCOPE LOCK

- **Objetivo.** Ningún fallo de `nsc` escribe `error*.log` en el directorio de trabajo salvo que se pida.
- **Naturaleza.** Corrección de conducta por orden explícita.
- **Debe quedar idéntico.** `--log`, `--log-on-error`, `--log-format`, `--time-trace`, `--save-failure-bundle`,
  el formato del log, la asignación con claims y el código de salida.
- **Modificable.** `src/nsc/main.cpp`, `src/nsc/Internal/CompilerRunConfiguration.h`, `src/Driver/DriverHelp.cpp`,
  `docs/compiler-testing-reengineering/components/HOST-NSC.md`.
- **Prohibido.** Borrar archivos de `C:`; cambiar otras opciones.
- **Terminación.** Un fallo sin opciones no crea archivos; con `--error-log` crea `error.log`.

## Cambio

- `CompilerRunConfiguration::automaticErrorLog` empieza en `false`.
- `configureCompilerRun` reconoce `--error-log`, que pide el log automático; `--no-error-log` lo apaga (el último
  gana) y `NSC_NO_ERROR_LOG=1` lo sigue apagando siempre. Las dos opciones se quitan de los argumentos que recibe
  el driver, igual que antes `--no-error-log`.
- Ayuda en inglés y español, y sección 10 de `HOST-NSC.md`.
- `publishAutomaticErrorLog` no cambia: la capacidad sigue existiendo, ahora es opcional.

`docs/compiler-testing-reengineering/inventory/contracts.csv` (CF-NSC-001) no cambia: describe
`automaticErrorLogEnabled`, que sigue leyendo `NSC_NO_ERROR_LOG` igual.

## Consumidores

`nsc-builder/nsc_builder/deploy.py` pasa `--no-error-log` (sigue aceptado). Ninguna prueba del repositorio espera
el log automático (búsqueda de `error.log`, `error1.log`, `NSC_NO_ERROR_LOG` y `publishAutomaticErrorLog` en
`tests` y `nsc-builder`).

## Validación

nsc Release recompilado (exit 0). Sonda en `A:\temp\nsc-dec\d10` con una fuente inexistente:

| Invocación | Exit | Archivos creados en el directorio |
|---|---|---|
| `nsc --no-banner missing.ns` | 1 | ninguno |
| `nsc --no-banner --error-log missing.ns` | 1 | `error.log` |
| `nsc --no-banner --error-log --no-error-log missing.ns` | 1 | ninguno |

`nsc --help` muestra el texto nuevo.

## No verificado

- Batería completa (para el cierre final).
