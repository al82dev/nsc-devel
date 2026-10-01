# Tanda baseline 01 — Eliminación de trabajo redundante

## Objetivo

Reducir costo de CPU y escritura en disco durante el baseline sin omitir casos, cambiar la
matriz de cobertura ni alterar el comportamiento normal del compilador fuera de `nscbuilder`.

## Problemas encontrados

1. SH41 consultaba la tabla completa de procesos de Windows cada milisegundo durante cada
   generación de `nsc2` y `nsc3`. En la corrida del 1 de octubre de 2026 esa sola prueba tardó
   1,142.49 segundos.
2. Las pruebas negativas ejecutadas por el builder ya conservan salida y evidencia en el
   historial, pero cada fallo esperado también generaba `error.log`, `error1.log`, etc. La
   verificación SH42 llegó a producir 1,140 archivos redundantes.

## Cambios

### Auditoría SH41

- La frecuencia de auditoría pasa de 1,000 Hz a 100 Hz.
- Se conserva la inspección de descendientes del proceso productor.
- Se añade una inspección final después de terminar el productor para cubrir procesos vivos en
  el límite de salida.
- No se elimina ni se marca como aprobada ninguna emisión de `nsc2`/`nsc3`.

### Logs automáticos durante el builder

- `nsc.exe` reconoce `NSC_NO_ERROR_LOG=1` como supresión del log automático consecutivo.
- `nscbuilder` establece esa variable únicamente para sus procesos hijos.
- `--log`, `--log-on-error` y la evidencia propia de las pruebas siguen disponibles.
- Fuera de `nscbuilder`, el valor predeterminado continúa generando `error.log` en un fallo.

## Archivos modificados

- `selfhost/bootstrap/verify_self_hosted_generation.py`
- `src/nsc/main.cpp`
- `nscbuilder.py`
- `tests/selfhost/test_bootstrap_infrastructure.py`

## Archivo nuevo

- Este documento. Se crea para conservar alcance, justificación, archivos y validación de la
  tanda en el mismo commit que el código.

## Mejora esperada

- Hasta 10 veces menos consultas de la tabla de procesos en el tramo auditado de SH41.
- Cero `error*.log` automáticos redundantes en las pruebas lanzadas por `nscbuilder`.
- Menos CPU, menos operaciones de directorio y menos limpieza posterior, sin reducir la matriz.

## Validación específica

- Dos pruebas unitarias dirigidas de infraestructura: aprobadas.
- Construcción exclusiva del objetivo `nsc` con Clang/LLD: aprobada.
- Contrato de host estático de `nsc.exe`: aprobado.
- Prueba funcional aislada:
  - con `NSC_NO_ERROR_LOG=1`: fallo esperado y cero logs automáticos;
  - sin la variable: fallo esperado y un `error.log`.
- Temporales de la prueba funcional: eliminados.
- Baseline completo: no ejecutado por instrucción del propietario.

