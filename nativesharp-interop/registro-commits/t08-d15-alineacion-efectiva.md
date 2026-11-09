# T08-D15: alineación efectiva de Clang en los accesos a miembros C

Fecha: 2026-11-09. Casillas: T08-D15, T08-012-02 y T08-012-02a
([decisión anterior](t08-layout-c.md#decisión-pendiente-t08-012-02a)).

## Problema

Clang emite cada acceso a un miembro de un record C con `alignmentAtOffset(alineación del record, offset)`. NSC
emitía `min(alineación natural, gcd(alineación del record, offset))` y nunca pasaba de la natural. Ejemplos:

| Acceso | Clang | NSC antes |
|---|---|---|
| `gs.b`, `int` en el offset 8 de un struct alineado a 8 | 8 | 4 |
| `ga.x`, `_Alignas(16) int` | 16 | 4 |
| `gn.inner.b`, record anidado | 8 | 4 |
| `p->b`, por puntero al record | 8 | 4 |
| `p[i].b`, elemento por puntero | 8 | 1 |

El resultado era seguro, pero distinto de Clang.

## Causa raíz

- `CppSemanticRecord.cpp` limitaba la alineación efectiva de cada campo a la natural de su tipo.
- `ProjectObjectLayoutModel.cpp` rechazaba un campo C con alineación mayor que la natural («invalid C++ instance
  field layout»).
- En `p[i]` mediante un puntero, `fieldOrArrayAddressAlignment` (`PackedArrayAlignment.h`) no conocía la alineación
  del puntero base y caía a 1.

## Decisión aplicada

Se adopta la alineación de Clang solo donde está garantizada, igual que en C:

- en un objeto del record (global, local, campo de una clase NativeSharp, `p->m` y `p[i].m`), la alineación del
  record está garantizada;
- un puntero a un miembro (`int* q = &gs.b`) solo garantiza la alineación natural de su tipo, como en Clang a través
  de un puntero.

NSMeta no cambia de formato ni de contenido: la alineación del record y los offsets de los campos ya viajan en él.
La alineación efectiva se calcula con esos datos y coincide con la de Clang. El consumidor precompilado obtiene el
mismo valor.

## Cambio

- `CppSemanticRecord.cpp`: la alineación efectiva de un campo, ordinario o flexible, es
  `gcd(alineación del record, offset en bytes)`, sin el tope natural.
- `ProjectObjectLayoutModel.cpp`: se quita el rechazo de un campo C alineado por encima de su tipo natural. Siguen
  las demás comprobaciones físicas.
- `ProjectOopMirNode.h`: nueva bandera MIR `cppRecordElementAlignmentGuaranteed`.
- `MirNodeMachine.cpp`, `lowerRawElementAddress`: activa la bandera si el elemento es un struct o union C importado
  cuya alineación es la del elemento.
- `MirCache.cpp`: la bandera viaja en la caché MIR en un byte propio, porque los 32 bits de banderas están ocupados.
- `PackedArrayAlignment.h`, `fieldOrArrayAddressAlignment`: si la base es un puntero y la bandera está activa,
  `p[i]` tiene la alineación del record.

## Pruebas

**Nueva:** `pruebas-tanda-08-layout-c/validar-alineacion-efectiva.py`. Aprobada en O0–O3.

- **IR:** la alineación de la lectura de cada uno de los 11 accesos es la de Clang. Los valores de referencia se
  midieron con la sonda `A:\nsc-interop-build\claude\sondas\align1` (IR de Clang de los mismos accesos).
  - `gs.b` 8, `gs.c` 4, `ga.x` 16, `ga.y` 4, `gp.v` 2 (`pack(2)`), `gn.inner.b` 8;
  - `p->b` 8, `p->x` 16, `p[i].b` 8, copia local `s.b` 8;
  - `*q` con `q = &gs.b`: 4, la natural.
- **Ejecución:** directo y desde una plantilla precompilada consumida sin el encabezado, con:
  - lectura de los mismos accesos y comparación con los valores de C;
  - una clase NativeSharp con un campo `A` (`_Alignas(16)`), escrito y leído;
  - escritura `items[1].b` mediante el puntero devuelto por C, comprobada desde C.

**Actualizadas.**

| Prueba | Antes | Ahora |
|---|---|---|
| `layout-c/validar-layout.py`, comparación de IR (T08-012-02) | NSC debía emitir `min(alineación de Clang, natural)`; 16 accesos por encima de la natural (`_Alignas`, records alineados a 8) quedaban como defecto | NSC debe emitir la alineación de Clang en los 56 accesos ordinarios; los campos de bits siguen como ventanas |
| `layout/validar.cmd`, casos `PackedConstRecordAddress` y `AlignedConstRecordAddress` (`int* p = &value->pairs[1].b` sobre un record con miembros `const`) | «cannot take the address of readonly storage» | «would discard pointee constness»; el caso empaquetado además rompe el contrato de alineación del puntero |

El segundo cambio no lo causa D15, sino T08-D06: desde entonces el `&` de almacenamiento C `const` produce un
`T const*`. La regresión de D06 no incluía `layout/validar.cmd`, y la de D15 lo detectó. Lo autoriza el punto 6 del
usuario.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d15.cmd`. Primera ejecución: los dos fallos de la tabla anterior. Se
actualizaron y se volvieron a ejecutar con resultado 0. Contenido:

- la suite nueva, `layout-c/validar-layout.py` y `layout-comparacion-c/validar-comparacion.py`;
- `arreglos-c`, `union-c` (las dos suites), `record-c/validar-miembros`, `globales-c/validar-atomicas-record` y
  `typedef-c/validar-record-dos-encabezados`;
- los validadores `layout-clang`, `layout`, `record-c`, `anonimos-c`, `union`, `valores`, `bitfields-c` (normal,
  union, `volatile` y metadata), `volatile-campos-c` y `flexible-c` (normal y `decay`);
- `nsc_cpp_session_tests`, recompilado;
- el conjunto de D02.
