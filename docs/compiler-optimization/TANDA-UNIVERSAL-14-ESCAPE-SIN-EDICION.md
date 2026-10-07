# Tanda universal 14: escape seguro sin edición ni `unsafe`

## Objetivo

Convertir la prueba de resúmenes de escape al contrato acumulativo del lenguaje:
la seguridad depende del destino demostrado del puntero, no de una edición declarada
ni de un bloque `unsafe`.

## Cambios

- Los proyectos del fixture ya no escriben `<Language>2</Language>` ni
  `<Language>3</Language>`.
- Las operaciones de puntero usan directamente la sintaxis nativa del lenguaje.
- El caso no-escape continúa aceptando la dirección de un local.
- Los escapes globales cross-project y en el mismo proyecto deben rechazarse con el
  diagnóstico causal publicado por el resumen.

## Archivo

- `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp`

El nombre histórico de la prueba CTest no se cambia; sólo se vuelve universal su
contrato observable.

## Validación dirigida

`nsc.ns3.t06.escape-summaries`: **PASSED**, 8/8 comprobaciones en O0. Quedaron
verificados el caso no-escape, el retorno, la retención global, la propagación
transitiva, la captura, NSMETA y los rechazos globales local y cross-project.

No se ejecutó baseline, certificación, promoción ni matriz O1/O2/O3.
