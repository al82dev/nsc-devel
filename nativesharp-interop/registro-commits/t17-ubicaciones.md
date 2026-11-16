# Tanda 17, cambio 1: ubicaciones C++ en los diagnósticos

## Defecto

En la [auditoría inicial](../TANDA-17-AUDITORIA-INICIAL.md), una función C/C++ que no se puede importar se
rechaza en el uso NativeSharp sin decir dónde la declara el encabezado, y las notas de Clang de una
instanciación inválida llegan sin su ubicación.

Causa en el código: `CallAnalyzer` arma el mensaje con `deferred.declaration.qualifiedName` y el motivo, y no
usa `deferred.declaration.location`, que la sesión ya llena con `getPresumedLoc`. `ClangSession` copia a
`result.diagnostics` solo el `message` de cada `CppSessionDiagnostic`, aunque `DiagnosticCollector` guarda su
`location`.

## Diseño

- `describeCppSourcePoint` da `archivo:línea:columna` de un punto C/C++, con la ruta en UTF-8, y
  `describeCppDiagnostic` antepone esa ubicación al mensaje de Clang cuando la hay.
- Las notas de una instanciación o deducción de plantilla fallida usan `describeCppDiagnostic`.
- El rechazo de una importación (llamada simple y calificada) agrega la nota `'<nombre>' is declared at
  <archivo>:<línea>:<columna>`. La ubicación del uso NativeSharp no cambia (T17-002).

## Cambio

- `CppSessionDiagnostic.h`: `describeCppSourcePoint`, `describeCppDiagnostic`.
- `ClangSession.cpp`: las notas de plantilla con su ubicación.
- `CallAnalyzer.cpp`: `cppDeclarationNotes` en los dos rechazos de importación.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture `pruebas-tanda-17-diagnosticos` (registro de [diagnósticos](t17-diagnosticos.md)). Sonda con el
compilador del cambio (`A:/temp/nsc-t17-sonda-v5v6rqwa`):

```
Main.ns:4:12: error: unknown identifier 'A' ...: C/C++ function 'A::take' cannot be imported: ...
note: 'A::take' is declared at .../importacion-fallida/api.h:2:40
Main.ns:4:34: error: Clang cannot instantiate the C++ template 'A::twice' with these arguments
note: .../plantilla/api.h:2:88: invalid operands to binary expression ('A::S' and 'int')
note: .../plantilla/api.h:3:1: in instantiation of function template specialization 'A::twice<A::S>' requested here
```

## Límites

- La ubicación es texto en la nota, sin la línea del encabezado ni subrayado.
- La nota «requested here» apunta al final del encabezado, donde NSC pide la instanciación, no al uso
  NativeSharp, que ya señala el error.
