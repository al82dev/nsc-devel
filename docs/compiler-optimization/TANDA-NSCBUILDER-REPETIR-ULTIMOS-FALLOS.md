# Tanda NSC Builder — repetir los últimos fallos

## Objetivo

Permitir que **Salida y resultados** vuelva a ejecutar únicamente los CTest fallidos de la
corrida CTest más reciente, sin lanzar otra matriz completa ni iniciar un baseline. Una repetición
que ya pasó desactiva la acción y no resucita fallos obsoletos de corridas anteriores.

## Cambios

- Se agregó el botón `Repetir últimos fallos (N)` a la barra de **Salida y resultados**.
- Se recuperan nombres exactos desde `metadata.json` y desde el log CTest. Esto conserva los
  fallos que ya habían aparecido aunque la corrida haya sido cancelada antes de terminar.
- La repetición hereda el `nsc.exe`, el árbol CTest, NSLink y el número de trabajos de la corrida
  original.
- El plan derivado desactiva explícitamente construcción, instalación, sincronización de
  bibliotecas, fixtures, triage, diagnóstico LLD y certificación de baseline.
- Se usa el filtro exacto de CTest ya existente; no se ejecutan pruebas aprobadas.
- El botón muestra el número de pruebas que repetirá y se deshabilita mientras haya otra
  operación activa.

## Archivos modificados

- `nscbuilder.py`: extracción de fallos, reconstrucción segura del plan histórico e integración
  del botón en la interfaz.
- `tests/unit/test_nscbuilder_pipeline.py`: cobertura del log cancelado y del aislamiento del plan
  dirigido.

## Archivos nuevos

- Este documento, para dejar evidencia de alcance, decisiones y validación de la tanda.

## Mejora obtenida

Una corrida larga ya no obliga a buscar y seleccionar manualmente cada nombre ni a repetir las
pruebas que pasaron. También se evita convertir por accidente esta acción de diagnóstico en una
nueva certificación costosa.

## Validación dirigida

- Tres pruebas unitarias específicas de `test_nscbuilder_pipeline.py`: recuperación desde una
  cancelación, aislamiento del plan dirigido y descarte de fallos obsoletos después de una pasada
  correcta.
- Comprobación contra la corrida cancelada `20261004-062631-aa322818`: deben recuperarse sus 25
  nombres CTest fallidos sin ejecutar el baseline.
