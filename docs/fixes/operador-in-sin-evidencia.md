# Barrera de escape de operadores `in` sin evidencia, por contenido (NSC-NEW-010)

## Problema

`nsc.compiler.operator-in-legacy-escape` usa un productor archivado con el build `14717501b74375ce`.
Sus operadores publican un campo `owned` del operando, y la prueba exigía que el consumidor rechazara
los temporales y aceptara los locales.

El lector actual decodifica por contenido. Los registros de tipo del archivo tienen una forma anterior
que no se distingue de la estructura vigente sin la etiqueta, así que la importación falla con
"truncated or invalid metadata type record". Como decidimos no usar la etiqueta para elegir una
lectura, la prueba no podía comprobar la barrera con ese archivo. Tampoco se puede regenerar: el
compilador actual rechaza las implementaciones inseguras.

## Decisión

- La barrera se comprueba con contenido actual en una suite nueva, `nsc.compiler.operator-in-missing-evidence`
  (`tests/unit/OperatorInMissingEvidenceTests.cpp`):
  - Un productor seguro (`operator +(in Direct a, in Direct b)` que solo lee) publica la evidencia
    `kMetadataOperatorInOperandsNoEscape`.
  - Control positivo: con esa evidencia, los temporales `new Direct(1) + new Direct(2)` se aceptan.
  - Negativo: el mismo modelo sin la evidencia, sin tocar nada más, se escribe y se verifica como
    contenido válido. Con él, los temporales se rechazan con "no viable" y los locales direccionables se
    siguen aceptando.
  - Contradictorio: la evidencia en un constructor se rechaza con "operator in-operand non-escape
    contract has an invalid signature". La metadata original del productor verifica.
- `operator_in_legacy_escape.py` conserva el fixture y sus huellas SHA-256. Ahora exige que el archivo
  nunca se importe:
  - cada consumidor se rechaza con un diagnóstico estructural ("cannot import precompiled metadata" con
    "invalid metadata" o "truncated");
  - ningún rechazo se basa en la magia o la versión.
  Así se registra la ambigüedad sin resolverla por etiqueta. El README del fixture lo indica.

## Validación

- `nsc.compiler.operator-in-missing-evidence`: 11/11 comprobaciones, más su guardia de binario vigente.
- `nsc.compiler.operator-in-legacy-escape`: pasa.

## Límites

- El demostrador de no-escape es conservador. Un operador que copia `a.item.Value` a un local `int`
  antes de usarlo no recibe la evidencia, y sus temporales se rechazan aunque son seguros. No se fija
  ese comportamiento en la prueba, para no consolidar la imprecisión.
- `tests/integration/operator_in_provenance_vistas.py` (no registrada en CMake) también usa el fixture
  archivado en `check_legacy_barrier` y no se modificó.
