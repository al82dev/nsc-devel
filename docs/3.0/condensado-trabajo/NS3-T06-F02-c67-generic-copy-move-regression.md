# NS3-T06-F02 — Corrección C67 de `move` genérico copiable

## Problema

La corrección que mantenía prestados los fields genéricos especializados como referencia también
retiró el contexto consumidor de `value = move(item)` cuando `T` se cerraba como un tipo copiable.
C67 fallaba tanto en O0 como en O2 antes de bajar código.

## Corrección

- Se separó el permiso de `move` de un field perteneciente a una especialización genérica de la
  propiedad real de su almacenamiento.
- Un parámetro genérico cerrado como tipo copiable conserva un `move` copy-shaped: no se marca
  moved-from ni se convierte en `owned`.
- Un parámetro realmente move-only u `owned` conserva el consumo, drop flags y diagnósticos
  existentes.
- Un field cerrado como referencia sigue siendo prestado; esta corrección no revierte el contrato
  de `a81f9974e`.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`: separación entre permiso genérico, consumo real y
  ownership del destino.
- `tests/cc1/c67_ns3_optional_coalescing/Main.ns`: fixture ya existente usada como evidencia.

## Evidencia puntual

`nsc_cc1_correctness_tests.exe cc1-c67-ns3-optional-coalescing`: **6/6 checks**.

No se ejecutó baseline, minibaseline ni suite general.
