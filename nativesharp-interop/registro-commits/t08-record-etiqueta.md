# T08-010-10: struct C declarado solo con etiqueta

Fecha: 2026-11-08. Casilla: T08-010-10.

## Resultado

Corregido en el compilador.

## Síntoma

Una plantilla publicada que nombraba un struct declarado solo con etiqueta (`struct Holder { int v; };`, sin
typedef) compilaba en el productor y en el camino directo. En cambio, el consumidor precompilado sin encabezado
informaba «unknown type name 'Holder' in module 'Pub'». Lo mismo ocurría si existía un typedef aparte con otro
nombre y la plantilla usaba la etiqueta.

## Causa

`provideCppCRecordMetadataCatalog` siembra el catálogo C de la metadata con estos tipos:

- los que aparecen en firmas publicadas;
- los alias (typedef) nombrados por las plantillas;
- los de funciones y globales C usadas por las plantillas.

Un record nombrado solo por su etiqueta dentro del cuerpo de una plantilla no entra por ninguno de esos caminos.
Además, los cuerpos de plantilla no se ligan en el productor, así que no hay un binding que lo identifique.

## Corrección

Para cada plantilla publicada, el productor recorre los nombres de tipo escritos
(`foreignReferenceCandidates`). Siembra los structs, unions y enums C cuya etiqueta coincide con uno de esos
nombres y cuyo proyecto es visible desde el de la plantilla. La clausura existente añade los records de sus
campos.

Una etiqueta que ninguna plantilla nombra sigue fuera de la metadata.

## Prueba

Nueva: `pruebas-tanda-08-record-c/validar-etiqueta.py`. Aprobada en O0–O3, con el rechazo en O0.

Se ejecuta en el camino directo y en una plantilla precompilada consumida sin encabezado. C comprueba los
valores escritos por NativeSharp. Cubre:

- un struct solo con etiqueta;
- una union y un enum solo con etiqueta;
- un struct cuyo campo es otro struct solo con etiqueta, para probar la clausura;
- una etiqueta con un typedef aparte de otro nombre.

**Rechazo:** el consumidor sin encabezado no puede nombrar una etiqueta que ninguna plantilla usa:
«unknown type name 'Unused' in module 'N'».

Con el compilador anterior, la sonda de origen fallaba en el consumidor con
«unknown type name 'Holder' in module 'Pub'».

Regresión: la misma batería de `t08-record-miembros.md`, más esta prueba, con el compilador final.
