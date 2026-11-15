# T10-002: inline namespaces, alias de namespace y declaraciones using

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md), una función solo era visible por su
nombre calificado impreso por Clang. Fallaban, con `unknown identifier`:

- `A.v1.f` para una función de `inline namespace v1`: el nombre impreso omite el inline namespace;
- `AB.f` con `namespace AB = A::B`;
- `B.f` con `namespace B { using A::f; }`.

Causa en el código:

- `ClangSession.cpp` (`collectFreeFunctions`) guardaba un solo nombre calificado
  (`getQualifiedNameAsString`).
- La selección por demanda (`selectCppSemanticDemands`, `CppSemanticFunctionGroups.cpp`) solo
  buscaba por ese nombre.
- El índice de funciones de namespace (`ProjectNamespaceFunctionModel.cpp`) registraba cada función
  importada bajo un único namespace.

## Cambio

- `CppFunctionDeclaration.h`: `alternateQualifiedNames` guarda otros nombres calificados que
  denotan la misma función.
- `ClangSession.cpp` los calcula de tres fuentes:
  - la ruta completa con inline namespaces (`fullNamespacePath`), cuando difiere del nombre impreso;
  - cada alias de namespace, dentro de un namespace o global, cuyo destino contiene la función
    directamente o a través de namespaces anidados;
  - cada declaración using en un namespace que introduce la función (`UsingShadowDecl`), una por
    sobrecarga.

  Los namespaces anónimos, los records y las funciones no aportan nombres.
- `CppSemanticFunctionGroups.cpp`: la demanda de un nombre alternativo selecciona la misma
  declaración, también desde un catálogo precompilado.
- `CppSemanticFunction.cpp` y `NamespaceFunctionSymbol.h` convierten cada alternativo en un
  namespace NativeSharp (`cppAlternateNamespaces`). `ProjectNamespaceFunctionModel.cpp` indexa el
  mismo ordinal bajo cada uno: el símbolo, la declaración LLVM y la identidad no se duplican.
- `CppCRecordMetadataCodec.cpp`: el catálogo guarda los nombres alternativos tras los parámetros,
  dentro del contrato NSMeta vigente.

No se modificó Clang ni NSLink.

## Evidencia

Fixture nuevo `pruebas-tanda-10-namespaces/validar.py`, con y sin `--dll`. Encabezado:

- `T10::v2::ver`, en un inline namespace;
- `T10::Deep::Inner::depth`, con el alias interno `T10::DI = Deep::Inner` y el alias global
  `T10A = T10::Deep`;
- `T10::Lib::helper(int)` y `helper(double)`, introducidas en `T10::Api` con `using Lib::helper`.

El consumidor llama a `T10.ver`, `T10.v2.ver`, `T10.DI.depth`, `T10A.Inner.depth`,
`T10.Api.helper` (las dos sobrecargas) y `T10.Lib.helper`. De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp;
- exige las 4 declaraciones del IR de NSC con los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado.

Controles negativos:

- una función de un namespace anónimo sigue invisible (`unknown identifier 'hidden'`);
- una declaración using dentro de una clase no da nombres de namespace
  (`no compatible member named 'put'`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-namespaces-h2k0757e` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-namespaces-jfoa43s5` |

La sonda de auditoría pasa ahora en `ns-inline-explicito`, `ns-alias`, `ns-alias-interno`,
`ns-using-decl` y `ns-using-solo` (`A:/temp/nsc-t10-sonda-x4uvmjl2`).

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Solo funciones libres. Las variables globales y los tipos de un namespace con alias siguen
  visibles solo por su nombre impreso.
- No se consideran las directivas `using namespace`.
