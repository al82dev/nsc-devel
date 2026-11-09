# T08-D14: miembro C con nombre cuyo tipo es un struct o union sin etiqueta

Fecha: 2026-11-09. Casillas: T08-D14 y T08-011-02c1 ([decisión anterior](t08-union-c.md#decisión-pendiente-t08-011-02c1)).

## Problema

Un miembro con nombre cuyo tipo es un struct o union sin etiqueta, como
`union U3 { struct { short lo; short hi; } pair; int whole; }`, no se importaba:

- en el camino directo faltaba el miembro: «has no compatible member named 'pair'»;
- en el precompilado fallaba la generación de la biblioteca: «C record field dependency is unavailable or
  incompatible».

## Causa raíz

La sesión de Clang no inventariaba records sin nombre ni typedef. Solo capturaba los portadores anónimos
(`struct { ... };` sin nombre de miembro), cuyos miembros se promueven. El tipo del campo quedaba con una identidad
de contexto anónimo y no se podía proyectar.

## Cambio

Se aplica la opción 1 de la decisión: una identidad sintetizada e importada. Reutiliza el mecanismo de los
portadores anónimos, que ya viaja en el catálogo C de NSMeta.

- `CppRecordDeclaration.h`, `CppAnonymousCRecord`: dos campos nuevos.
  - `memberName`: el nombre del miembro cuyo tipo es este agregado; vacío en un portador anónimo.
  - `siblingMemberIndices`: los otros miembros del mismo padre declarados con ese mismo tipo
    (`struct { ... } a, b;`).
- `ClangSession.cpp`, `collectAnonymousCRecords`: además de los portadores, captura el tipo sin nombre de un miembro
  con nombre, una vez por tipo y en cualquier profundidad. El tipo puede ser el del miembro o el elemento de sus
  arreglos y punteros (`struct { ... } pairs[2], *current;`).
- `CppSemanticRecord.cpp`:
  - `materializeCppAnonymousCRecords` crea para cada uno el record anidado `Owner::__nsc_c_member_<miembro>`
    (`isAnonymousCInternal`). El miembro conserva su nombre y no es portador, así que sus miembros no se promueven.
    El tipo del miembro, o su elemento más interno, y el de los miembros hermanos pasan a nombrar ese record. Las
    cadenas de `CppType` se copian antes de cambiarlas porque comparten sus elementos.
  - `sameCapturedMembers` compara también `memberName` y `siblingMemberIndices`.

El catálogo C de NSMeta no cambia de formato. El record sintetizado viaja como cualquier record C anidado, y el
consumidor precompilado lo reconstruye.

## Pruebas

**Nueva:** `pruebas-tanda-08-union-c/validar-miembro-sin-etiqueta.py`. Aprobada en O0–O3, con rechazos en O0.

- **Tipos cubiertos:**
  - `U3.pair`: struct sin etiqueta dentro de una union;
  - `S.point` y `S.other`: dos miembros que comparten el tipo;
  - `S.val`: union sin etiqueta;
  - `S.deep.inner`: struct sin etiqueta dentro de otro;
  - `S.pairs[2]` y `S.current`: arreglo y puntero de un tipo sin etiqueta.
- **Operaciones:**
  - escritura y lectura en todas las profundidades;
  - copia entre miembros del mismo tipo (`s.other = s.point`);
  - `var p = s.point`;
  - paso por valor y por puntero a C, con comprobación en C;
  - `sizeof` frente a Clang.
- Cada caso tiene un control Clang y se ejecuta directo y desde una plantilla precompilada consumida sin el
  encabezado.
- **Rechazos:**
  - los miembros de un miembro con nombre no se promueven: `u.lo` da «has no compatible member named 'lo'»;
  - no se pueden asignar miembros de tipos sin nombre distintos: `s.point = s.deep`.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d14.cmd`, resultado 0:

- la suite nueva;
- `union-c/validar-uniones`, `record-c/validar-miembros`, `typedef-c/validar-formas` y
  `validar-record-dos-encabezados`, y `globales-c/validar-enum-record`;
- los validadores `anonimos-c`, `union`, `bitfields-c/validar-union`, `record-c`, `flexible-c` y `layout-clang`;
- la batería `identidad-c\validar.cmd`, que incluye `anonimos-anidados` y los USR publicados;
- `nsc_cpp_session_tests`;
- el conjunto de D02.
