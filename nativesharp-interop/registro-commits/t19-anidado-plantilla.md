# Tanda 19: records anidados en especializaciones de plantilla de clase (T19-036)

## Defecto

La [verificación de traslados](t19-traslados-verificacion.md) mostró que un record anidado en una plantilla de
clase no se puede usar desde NativeSharp:

- `T.Box<int>.Item it` no se analiza como tipo (`expected ';'`);
- con `var`, `T.Box<int>.make` falla con `has no compatible member named 'make'`;
- `seen(Box<int>::Item)` da `C/C++ parameter 0 is not yet mapped`.

## Causas

Las tres están en `ClangSession.cpp`:

1. Clang instancia la clase miembro de una especialización solo cuando se usa completa.
   `instantiateNamedSpecializations` completaba las especializaciones que nombra el encabezado, pero no sus
   clases miembro. `Box<int>::Item` quedaba declarado e incompleto.
2. La identidad de un record anidado en una especialización usaba el nombre de la plantilla (`Box`) como record
   contenedor y marcaba `specializationContext`. `projectCppRecordIdentity` y `projectCppRecordTypeIdentity` (en
   `CppSemanticRecord.cpp`) descartan esos records, así que `Item` no se importaba.
3. `findComparableRecords`, que busca un record por su nombre calificado, no recorría el interior de las
   especializaciones. La observación del valor C++ (`recordValueOperations`) no encontraba `T::Box<int>::Item`:
   el tipo quedaba importado pero no elegible como valor trivial
   (`experimental C++ local requires direct automatic construction and cleanup`).

## Cambio

- `ClangSession.cpp`:
  - `instantiateNamedSpecializations` instancia las clases miembro públicas de cada especialización que completa,
    y completa una clase miembro nombrada, como lo haría una unidad C++ que la usa. Las instanciaciones que Clang
    rechaza siguen con los diagnósticos suprimidos y no se importan;
  - `cppEnclosingRecordName`: una especialización en un namespace encierra a sus records con su nombre
    NativeSharp (`Box__t<hash>`, T13-004). Así la identidad de `Item` es exacta. `specializationContext` queda solo
    para una especialización sin nombre NativeSharp. El hash ODR de Clang se sigue omitiendo dentro de cualquier
    especialización;
  - `findComparableRecords` busca también dentro de las especializaciones con nombre.
- `CppCRecordMetadataCodec.cpp`, `qualifiedMemberOfOwner`. El codec exige que el nombre calificado de un método
  estático C++ empiece con el de su propietario. Cuando el propietario es una especialización, en NativeSharp se
  llama `Box__t<hash>` y Clang la escribe `T::Box<int>`. Ahora el codec reconoce esa escritura si reproduce el hash.
  Antes, un método estático de una especialización no se podía publicar en un catálogo precompilado.

## Evidencia

Casos `anidado-plantilla` (tipo explícito) y `anidado-plantilla-var` (`var`) de `pruebas-tanda-19-traslados`:

- `Item` devuelto por el método estático inline `Box<int>::make`;
- un campo de `Item` modificado desde NativeSharp;
- `Item` pasado por valor a `seen`, de la biblioteca.

| Prueba | Resultado |
|---|---|
| Estática, O0–O3 | 444, igual que Clang |
| DLL, O0–O3 | 444, igual que Clang |
| Forma de la declaración en el IR de NSC | igual que en el IR de Clang |

Registros: `logs/fx-an4.log` y `logs/fx-an4d.log`. El fixture completo pasa: 8 casos en estática y 7 en DLL.

Regresión dirigida (`lista-an4.txt`, tandas 10 a 18, porque el cambio toca la identidad de todo record anidado en
una especialización, incluida la STL de la tanda 18):

- 61 de 61 en estática (`logs/regresion-an4`);
- 43 de 43 en DLL (`logs/regresion-dll-an4d`);
- `lote-ct2`: `ninja -k 0` sin errores; las nueve CTests de interoperabilidad y `nsc_cpp_c_identity_tests` pasan;
- `ctest -R "snapshot|precompiled|record|cpp-interop"`: 21 de 22, falla `nsc.cpp-interop.session` sin sus fixtures
  (T19-037).

## Sigue pendiente

- El mismo uso desde una plantilla NativeSharp precompilada. La superficie pública de la plantilla conserva
  `T.Box<int>` y el consumidor no le aplica la reescritura de usos de plantillas C++ (`applyCppTemplateUses` omite
  las unidades precompiladas). El productor falla al verificar su propia superficie (`expected ';'`) y el consumidor
  dice que `T.Box` no es una plantilla genérica visible. Es T19-016: especializaciones C++ en la API pública de un
  módulo precompilado.
- Las clases miembro protegidas o privadas de una especialización no se instancian ni se importan.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
