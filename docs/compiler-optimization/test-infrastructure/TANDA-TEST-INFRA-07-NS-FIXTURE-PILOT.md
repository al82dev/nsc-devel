# Tanda 7 — Piloto de fixture NativeSharp externo

## Alcance

Esta tanda externaliza una entrada NativeSharp mínima que antes sólo existía como literal dentro de
`tests/unit/Item394StageBoundaryVerifierTests.cpp`. No modifica el compilador, el contrato de la prueba
original ni su ejecutable legado.

## Decisión de separación

El caso original contiene dos responsabilidades distintas:

1. aceptar y procesar la fuente `int answer() { return 42; }`;
2. verificar mediante API C++ interna el orden de seis fronteras del pipeline.

El primer contrato puede ejercitarse mediante el `nsc.exe` construido. El segundo debe permanecer como unidad
C++ porque observa estado interno que no forma parte de la interfaz pública del compilador. Convertir todo el
archivo en un fixture externo habría eliminado cobertura.

## Archivos nuevos

- `tests/cases/frontend/nsc.item394.stage-boundary-source/Main.ns`: entrada NativeSharp externalizada.
- `tests/cases/frontend/nsc.item394.stage-boundary-source/case.json`: comando, optimización O0 y expectativa.

## Archivos modificados

- `docs/compiler-optimization/test-infrastructure/test-migration-state.json`: registra la convivencia y la
  procedencia del literal.
- `docs/compiler-optimization/test-infrastructure/TRACKING.md`: marca la tanda como terminada.

## Contrato ejecutado

El orquestador invoca el binario `nsc.exe` seleccionado explícitamente con:

```text
--dump-sema <Main.ns> -O0
```

La expectativa reproduce exactamente la parte externalizable del contrato antiguo: el frontend acepta la
fuente y devuelve código cero sin diagnóstico de error. No se exige texto incidental de la salida semántica.

## Compatibilidad y retiro

El literal antiguo permanece temporalmente en el ejecutable original. El estado `DUAL_PARITY` impide retirarlo
hasta que la fase de corte demuestre paridad y cambie la unidad C++ para consumir la fuente externa sin perder
sus aserciones internas.

## Validación dirigida

- `nsc.exe` construido: `A:\nsc-devel\build-clang\bin\nsc.exe`.
- SHA-256 del compilador: `2877624cefe980b2033023186c5be296c998ec8b77e4e9a6f3e01c6760647e7f`.
- Caso nuevo: `1/1` aprobado, código de salida `0`, sin diagnóstico y sin timeout.
- Duración del proceso nuevo: `0.023138` segundos.
- Caso legado `nsc.item394.stage-boundary-verifiers`: `1/1` aprobado en `0.18` segundos.
- Pruebas unitarias del harness: `15/15` aprobadas.
- Sandboxes residuales después de la corrida: `0`.
- Evidencia mecánica: `_test-harness/results/tanda07-item394/summary.json` y `report.log`.

No se ejecuta baseline ni la batería completa.
