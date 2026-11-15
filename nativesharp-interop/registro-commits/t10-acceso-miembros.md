# T10-007: acceso público, protegido y privado de los miembros estáticos C++

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md), `A.S.hidden(2)` sobre un método estático
privado y `A.S.prot(2)` sobre uno protegido fallaban con `has no compatible member named 'hidden'`. El
miembro existe en C++ y es inaccesible, no inexistente.

Causa en el código: `projectCppSemanticStaticMethod` (`CppSemanticFunction.cpp`) y
`projectCppSemanticStaticField` (`CppSemanticRecord.cpp`) descartaban todo miembro no público. El
modelo de miembros (`ProjectMemberModel.cpp`) creaba cada miembro C++ importado como público. Clang ya
informaba el acceso de cada miembro (`CppAccess`).

## Cambio

- `CppImportedStaticMethod.h` y `CppImportedStaticField.h`: `accessibility` guarda el acceso C++ como
  `ClassMemberAccessibility`.
- `CppSemanticFunction.h/.cpp` y `CppSemanticRecord.h/.cpp`: los métodos y campos estáticos públicos,
  protegidos y privados se proyectan con su acceso.
- `ProjectMemberModel.cpp`: el miembro importado conserva ese acceso. NativeSharp lo comprueba en cada
  uso como el de cualquier miembro, con su diagnóstico de miembro inaccesible.

NativeSharp deja fuera las sobrecargas inaccesibles antes de resolver, como C#. Una llamada para la que
C++ elegiría una sobrecarga privada se rechaza igual, con el diagnóstico de sobrecarga no viable.

Al probar la plantilla precompilada apareció un defecto en `CppCRecordMetadataCodec.cpp`: un catálogo
cuyas únicas dependencias eran métodos estáticos o de instancia C++ se rechazaba como vacío
(`empty C dependency catalogue`). Esos métodos cuentan ahora como dependencias, como ya hacía quien
elige la codificación (`CppSemanticRecord.cpp`).

No se modificó Clang ni NSLink. El contrato NSMeta no cambia: solo llegan al catálogo los miembros que
se usan, que siempre son accesibles.

## Evidencia

Fixture nuevo `pruebas-tanda-10-acceso/validar.py`, con y sin `--dll`. Encabezado:

- `struct Vault` con un método y un campo privados, un método y un campo protegidos, y un método y un
  campo públicos;
- `struct Mixed` con `pick(int)` público y `pick(double)` privado.

`Vault::open` usa los miembros no públicos dentro de la biblioteca. De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado 40), que
  escribe el campo público y llama a `open` y a `pick(4)`;
- exige las declaraciones llamadas en el IR de NSC con los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado, que solo llama a los métodos públicos
  (resultado 32).

Controles negativos:

- el método privado, el campo privado, el método protegido y la escritura del campo protegido se
  rechazan con `member '<nombre>' is not accessible`;
- `T10.Mixed.pick(1.5)`, para el que C++ elegiría la sobrecarga privada, se rechaza con
  `no viable overload of 'pick'`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-acceso-rsfi4cce` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-acceso-j4lrzrfs` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Los records y enums anidados privados o protegidos siguen sin importarse
  ([t10-tipos-anidados.md](t10-tipos-anidados.md)), así que su uso da un nombre desconocido.
- El acceso protegido no tiene todavía clases derivadas NativeSharp que lo usen (tanda 12).
- Los métodos de instancia y su acceso quedan para la tanda 11.
- Un record declarado con `class` no entra todavía en el catálogo precompilado
  (`record is not a consistent C declaration`); el fixture usa `struct` con secciones `private` y
  `protected`.
