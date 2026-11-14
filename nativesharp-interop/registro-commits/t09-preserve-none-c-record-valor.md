# T09-003: record C por valor con preserve_none

## Defecto y cambio preparado

En Windows x64, una función C `preserve_none` que recibe y devuelve por valor el mismo record nombrado de 36 bytes se ejecutó con Clang 23.1.0, pero NSC la rechazó (`A:/temp/nsc-t09-preserve-none-c-record-27f5u_k2`). La ruta C excluía todos los records al solicitar el plan ABI de la declaración. El cambio solicita el plan de Clang de la declaración exacta y lo conserva cuando el validador existente comprueba que el argumento y retorno corresponden al mismo record, con identidad, USR, tamaño, alineación, símbolo y forma física compatibles. No cambia NSMeta ni modifica Clang o NSLink.

## Evidencia dirigida

`pruebas-tanda-09-preserve-none-c-record/validar-matriz.py` compara Clang y NSC con biblioteca estática y DLL, O0–O3. Comprueba todos los componentes del resultado, que el valor original observado conserve sus campos, y el retorno indirecto `sret` en el IR. Una plantilla precompilada también pasa con y sin encabezado. Evidencia: `A:/temp/nsc-t09-preserve-c-record-matriz-x4bhgp_1` y `A:/temp/nsc-t09-preserve-c-record-matriz-bs5om5zr`.

La prueba de identidad y cinco CTest ABI pasaron. La regresión C++ de record de 40 bytes pasó directa y por puntero (`A:/temp/nsc-t09-preserve-cpp-record24-99x9emxx`) y en 16 consumidores precompilados (`A:/temp/nsc-t09-preserve-cpp-record24-precompilado-wd3jp25g`). Una firma con dos records C distintos se aceptó en Clang y se rechazó en NSC (`A:/temp/t09-c-record-negativo-heterogeneo`).

## Pendiente y límites

La prueba existente `pruebas-tanda-09-preserve-none-c-plan/validar.py` todavía espera que el record de 36 bytes se rechace y falla por ese motivo (`A:/temp/nsc-t09-preserve-none-c-plan-vr7crvfo`). Se mantiene intacta mientras se consulta la autorización para conservar su control negativo con una firma distinta aún no admitida. No se presenta el bloque como validado hasta resolver y repetir esa prueba.

La evidencia cubre un record nombrado concreto de 36 bytes, un parámetro y retorno de la misma identidad, y llamada directa. No acredita records de distintas identidades, todas las disposiciones, llamadas por puntero ni variádicas. T09-003 y la tanda 9 siguen abiertos.

## Resolución del control anterior (2026-11-14)

Con autorización del usuario, `pruebas-tanda-09-preserve-none-c-plan/validar.py`
conserva la misma comprobación de rechazo
(`preserve_none does not yet support this signature`). Su entrada pasa del record
anónimo de 36 bytes, ya admitido como ida y vuelta, a
`T09Out t09_pn_mixed_records(T09In)`: dos records C nombrados distintos de 36 bytes en
parámetro y retorno. La prueba compila antes con Clang una implementación de esa
función para comprobar que la firma es válida. Así se confirma que el rechazo de NSC es
un límite real y no un error de la muestra.

Corridas con el compilador que incluye el cambio
(`3.0.0+build.20261114T174038Z.feefdef44ff158d5`, árbol sin cambios respecto de la
construcción probada):

- `pruebas-tanda-09-preserve-none-c-plan/validar.py --precompilado`, O0–O3, directo y
  plantilla con y sin encabezado, más el rechazo nuevo:
  - estática: `A:/temp/nsc-t09-preserve-none-c-plan-06c9yb1q`;
  - DLL: `A:/temp/nsc-t09-preserve-none-c-plan-tya3_kdy`.
- `pruebas-tanda-09-preserve-none-c-record/validar-matriz.py --precompilado`:
  - estática: `A:/temp/nsc-t09-preserve-c-record-matriz-za_etjwn`;
  - DLL: `A:/temp/nsc-t09-preserve-c-record-matriz-41_je_z6`.
- Regresión de 12 validadores C `preserve_*` (6 fixtures, comparación directa y 16
  consumidores precompilados cada uno), por ejemplo
  `A:/temp/nsc-t09-preserve-bool-3fhyek6b` y
  `A:/temp/nsc-t09-preserve-indirecta-mixta-precompilado-0apmdlsc`.

Siguen sin acreditar: records C distintos en una misma firma, records C mezclados con
escalares, punteros a función C con records por valor y variádicas.
