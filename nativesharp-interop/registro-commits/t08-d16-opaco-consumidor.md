# T08-D16: declaración opaca propia del consumidor, conciliada con NSMeta

Fecha: 2026-11-09. Casillas: T08-D16 y T08-013-02c1
([decisión anterior](t08-incompletos-c.md#decisión-pendiente-t08-013-02c1)).

## Problema

Un productor precompilado publica `Opaque`. El consumidor no tiene el encabezado del productor y declara el record
en el suyo (`typedef struct Opaque Opaque;` o `struct Opaque;`). Se rechazaba:

- con typedef, «C/C++ type alias conflicts with a native type 'Opaque'»;
- con solo la etiqueta, «cannot initialize local 'o' of type 'type:C::Opaque*' from 'type:Pub::Opaque*'».

Con el mismo encabezado sí se conciliaba.

## Causa raíz

- `compareCRecordSourceEvidence` (`CppCRecordMetadataCodec.cpp`), para dos records incompletos con el mismo
  contrato (etiqueta, clase, ámbito, destino y USR), exigía además la misma procedencia: archivo, hash y offset.
- `collectVisibleCRecordReconciliations` (`CppSemanticRecord.cpp`) comparaba un record incompleto del consumidor
  con uno completo del productor como dos contratos distintos.
- `ProjectNameLookup::setCppImportedTypes` exigía la misma completitud y el mismo layout en un tipo conciliado.

## Decisión aplicada

Se aplica la opción 1: conciliar por etiqueta y USR. Es la regla de C: un tipo incompleto es compatible con el
record de la misma etiqueta en otra unidad de traducción.

- Dos records incompletos con el mismo contrato son el mismo tipo, sin importar el encabezado que los declara.
- La declaración adelantada del consumidor nombra el record que publicó el productor, también si es completo. En
  ese caso el consumidor ve los miembros, porque el tipo es el publicado.
- Sigue siendo necesario que el candidato visible sea único. Si dos productores publican el mismo tipo opaco, el
  consumidor que lo declara recibe el error de conflicto.
- Un consumidor que **define** el record que un productor publicó como opaco sigue rechazado. Para aceptarlo, el
  tipo del productor tendría que adoptar el layout del consumidor, y la conciliación va en el sentido contrario.
  La casilla trata la declaración opaca del consumidor.

## Cambio

- `CppCRecordMetadataCodec.cpp`, `compareCRecordSourceEvidence`: dos records incompletos con el mismo contrato
  coinciden sin comparar la procedencia.
- `CppSemanticRecord.cpp`, `collectVisibleCRecordReconciliations`: un record incompleto del consumidor coincide con
  el record completo del productor que tiene la misma etiqueta, clase, ámbito, destino y USR.
- `ProjectNameLookup.cpp`, `setCppImportedTypes`: un record C incompleto puede conciliarse con uno completo. Como
  no tiene layout propio, no se comparan layouts.

NSMeta no cambia.

## Pruebas

**Nueva:** `pruebas-tanda-08-incompletos-c/validar-opaco-consumidor.py`. Aprobada en O0–O3, con un control Clang
por caso y los rechazos en O0.

- **Casos positivos.** El consumidor tiene su propio encabezado y su propia biblioteca C:
  - typedef frente a typedef;
  - etiqueta frente a etiqueta;
  - etiqueta en el productor y typedef en el consumidor;
  - productor con el record completo y consumidor opaco, que además escribe `o->v` y mide `sizeof`.
- **Operaciones:**
  - un `Opaque*` del productor pasa a C del consumidor;
  - un `Opaque*` de C del consumidor pasa al productor;
  - una plantilla del productor con `Opaque*`, instanciada por el consumidor.
- **Rechazos:**
  - `union Opaque` frente a `struct Opaque`;
  - una etiqueta distinta (`Other*` desde `Opaque*`);
  - dos productores con el mismo tipo opaco;
  - un consumidor que define el record.

**Actualizada.** Fijaba el rechazo anterior; lo autoriza el punto 6 del usuario.

| Prueba | Antes | Ahora |
|---|---|---|
| `incompleto/validar-opaco.cmd`: `OpaqueOther` (copia del encabezado opaco en otro archivo) y el encabezado opaco modificado después del productor | rechazo «type alias conflicts with a native type» | se concilian, se compilan y se ejecutan |

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d16.cmd`. Primera ejecución: un fallo, la prueba de la tabla, que
se actualizó y se volvió a ejecutar con resultado 0. Contenido:

- la suite nueva y `incompletos-c/validar-incompletos.py`;
- `incompletos-diag-c`;
- `typedef-c/validar-reconciliacion`, `validar-record-dos-encabezados` y `validar-typedef-productores`;
- `union-c/validar-miembro-sin-etiqueta` y `globales-c/validar-enum-record`;
- los validadores `incompleto` (normal y opaco), `meta-c` (normal, productor y sombras), `anonimos-c` y `record-c`;
- la batería `identidad-c\validar.cmd`;
- `nsc_cpp_session_tests`, recompilado;
- el conjunto de D02.
