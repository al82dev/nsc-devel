# T10-005 y T10-002: records y enums C++ anidados en records

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md) fallaban tres formas de tipo anidado:

- `A.S.T.f(1)`, un método estático de `struct S { struct T { static int f(int); }; }`;
- `A.S.T t;`, una variable del record anidado (`expected ';', found 't'`);
- `A.S.E.Y` y `A.S.take(A.S.E.Y)`, un enum de un record y un método estático que lo recibe.

Causas en el código:

- `prepareCppRecordObservation` (`CppSemanticRecord.cpp`) aplazaba todo record con tipos
  contenedores, así que un record anidado nunca llegaba a la búsqueda de nombres.
- El parser solo reconoce como tipo un nombre que conoce. `cppParserProjectInventories`
  (`CppParserProjectInventory.h`) le daba el nombre de identidad, que separa los tipos contenedores con
  `+` (`A.S+T`), y no la escritura `A.S.T` del código.
- `collectEnums` (`ClangSession.cpp`) no entraba en los records C++, y la identidad de un enum
  (`projectCppEnumDeclarationIdentity`) trataba cada calificador como un namespace.
- Un método estático se proyectaba sin la búsqueda de nombres, así que no podía mapear un parámetro
  enum. El modelo de miembros (`ProjectMemberModel.cpp`) solo admitía `type:` detrás de un puntero.

## Cambio

Records anidados:

- `CppSemanticRecord.cpp`: un record C++ anidado deja de aplazarse si todos sus records contenedores,
  hasta el namespace, son grupos importados y cada nivel es público. Pasa a ser un tipo anidado
  NativeSharp, como una clase anidada en código NativeSharp. Un record privado o protegido, o
  dentro de uno, sigue oculto.
- `CppParserProjectInventory.h`: el parser recibe el nombre con puntos entre los tipos contenedores.

Enums anidados:

- `CppEnumDeclaration.h` y `CppType.h` (`CppEnumTypeIdentity`): `containingRecords` guarda los records
  C++ que encierran el enum.
- `ClangSession.cpp`:
  - `collectEnums` entra en las definiciones de record C++ con nombre, sin plantilla, públicas a
    cualquier profundidad, y toma sus enums públicos con nombre;
  - `enclosingCppRecords` calcula los contenedores de la declaración y del tipo.

  En C, un enum declarado en un struct pertenece al archivo y no tiene contenedores.
- `CppSemanticEnumType.h` (`splitCppEnumQualifiers`): los calificadores finales que nombra
  `containingRecords` son tipos contenedores; el resto, namespaces. Lo usan la identidad de la
  declaración, la del tipo y la de una firma (`CppSemanticInventory.cpp`).
- `CppSemanticInventory.cpp`:
  - los enumeradores de un enum anidado se nombran por el tipo (`A.S.E.Y`), también si no es scoped;
  - no son constantes del namespace;
  - un enum anidado solo es tipo si cada record que lo encierra también lo es.
- `CppCRecordMetadataCodec.cpp`: la identidad del tipo enum y el catálogo de enums guardan
  `containingRecords`, dentro del contrato NSMeta vigente.

Métodos estáticos con enums:

- `CppSemanticFunction.cpp`: `projectCppSemanticStaticMethod` recibe la búsqueda de nombres y mapea un
  enum importado como en una función libre.
- `ProjectMemberModel.cpp`: un método estático puede tomar y devolver un enum importado por valor. Al
  construir los miembros se exige que el tipo sea un enum; un record por valor sigue sin ABI estático.

Catálogo precompilado:

- `CppSemanticRecord.cpp`: el receptor de un método estático de un record anidado se escribe con
  puntos al sembrar el catálogo, y cada tipo anidado del catálogo lleva sus records contenedores.
- `CppSemanticFunction.cpp` (`sameSharedCppTypeKey`) y `ProjectMemberModel.cpp`: un método estático
  visto en vivo y en el catálogo comparte símbolo. Sus claves de tipo pueden diferir solo en el módulo,
  si nombran el mismo tipo de Clang (mismo USR, origen y nombre calificado).

No se modificó Clang ni NSLink.

## Evidencia

Fixture nuevo `pruebas-tanda-10-anidados/validar.py`, con y sin `--dll`. Encabezado:

- `T10::Outer` con `Point`, `Box::Cell`, `enum Color`, `enum class Mode : short` e `Inner`;
- métodos estáticos `Outer::scale(Color, Mode)`, `Outer::at(Point*)`, `Outer::flip(Mode)` e
  `Outer::Inner::twice(int)`;
- funciones libres que toman o devuelven `Outer::Point`, `Outer::Box::Cell`, `Outer::Color` y
  `Outer::Mode` por valor;
- `useg(Gl::K)`, con un enum en un struct global.

De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado 75);
- exige las 10 declaraciones del IR de NSC con los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado, con el mismo cuerpo.

Controles negativos y sus contrapartes:

- el record privado `Hidden::Secret` sigue oculto (`no compatible member named 'Secret'`) y
  `T10.Hidden.open()` sí se llama;
- el enum protegido `Hidden::Mode` sigue oculto (`no compatible member named 'Mode'`), y el enum
  público `T10.Outer.Color.Red` sí se nombra;
- `T10.Outer.Red` se rechaza (`no compatible member named 'Red'`): el enumerador de un enum no scoped
  no se inyecta en el record.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-anidados-5y0ncaa9` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-anidados-_yvzd_7e` |

La sonda de auditoría pasa ahora en `estatico-anidado`, `anidado-tipo-variable`,
`anidado-enum-variable`, `enum-en-clase` y `enum-clase-scoped`; `anidado-privado` y `enum-privado`
siguen rechazados (`A:/temp/nsc-t10-sonda-eflpzf8q`, `A:/temp/nsc-t10-sonda-6dyglz12`).

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Un enumerador de un enum no scoped anidado no se nombra a través del record (`S::Y` en C++).
- Los records anidados en plantillas, anónimos o en especializaciones no se importan.
- Un método estático no toma ni devuelve un record por valor.
- Las llamadas variádicas siguen rechazando tipos anidados.
