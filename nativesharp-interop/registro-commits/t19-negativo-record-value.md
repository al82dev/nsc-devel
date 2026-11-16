# Tanda 19: negativo obsoleto de `nsc.cpp-interop.record-value-observation` (T19-037)

## Defecto

La prueba nueva de la interoperabilidad `nsc.cpp-interop.record-value-observation` termina con código 29. El
código 29 significa que un record se acepta como valor aunque no sea de disposición estándar o no sea POD.

## Causa

- La prueba es de la tanda 9 (`776dd119a` a `b91477251`, 2026-11-11 a 13). Exige que
  `cppRecordValueOperationsEligible` rechace un record si falta cualquiera de cinco rasgos:
  `triviallyCopyable`, `standardLayout`, `trivialDefaultConstructor`, `trivialDestructor`, `plainOldData`.
- `2751b8696` (T12-001, 2026-11-15) hizo valor a un record trivialmente copiable con construcción por defecto y
  destrucción triviales también si no es de disposición estándar: campos en una base y en el derivado, o
  acceso mixto. Es lo que hace Clang, y los fixtures `pruebas-tanda-12-*` lo comparan con Clang en estática y
  DLL, O0–O3.
- Desde entonces, dos de los cinco rechazos de la prueba (`standardLayout` y `plainOldData`) describen una regla
  que la tanda 12 reemplazó.

## Cambio

Se aplica la política de negativos obsoletos de esta interoperabilidad: el caso pasa a una comprobación
positiva y se conservan los rechazos genuinos. En `tests/unit/CppRecordValueObservationTests.cpp`:

- siguen siendo rechazos (código 29): `triviallyCopyable`, `trivialDefaultConstructor` y `trivialDestructor`;
- `standardLayout` y `plainOldData` en `false` deben seguir siendo elegibles (código nuevo 46), como en T12-001.

No cambia el compilador ni ninguna otra comprobación de la prueba.

## Evidencia

`ctest -R cpp-interop.record-value`: `record-value-traits`, `record-value-operations` y
`record-value-observation` pasan (`logs/aplicar-rv.log`). La comparación con Clang de esos records está en los
fixtures de la tanda 12, que pasan en la validación del bloque.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
