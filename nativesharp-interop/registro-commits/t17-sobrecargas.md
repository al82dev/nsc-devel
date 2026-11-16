# Tanda 17, cambio 2: las alternativas de una sobrecarga

## Defecto

En la [auditoría inicial](../TANDA-17-AUDITORIA-INICIAL.md), una llamada que ninguna sobrecarga acepta dice
`no viable overload of namespace function 'A.f' accepts the argument types` (o `accepts N argument(s)`, o
`ambiguous namespace function call`), sin las alternativas.

Causa en el código: `reportNoViableNamespaceFunctionCall` y `selectNamespaceFunctionCandidate` tienen los
candidatos evaluados (`evaluatedCandidates`, con la conversión de cada argumento y el ordinal de la función) y
solo informan el nombre escrito. El símbolo de una función C/C++ (`NamespaceFunctionSymbol`) no conserva la
declaración C++ ni su ubicación.

## Diseño

- `NamespaceFunctionSymbol` guarda, solo para diagnósticos, la declaración C++ como texto
  (`int A::f(double)`) y su ubicación (`cppDeclarationSpelling`, `cppLocation`), que llena
  `CppSemanticFunction` desde la declaración de Clang. No forman parte de la identidad ni de NSMeta: una
  función restaurada de un catálogo precompilado no las tiene y su nota usa la firma NativeSharp.
- Cada error de resolución lleva una nota por candidato (hasta ocho): su declaración y por qué no acepta la
  llamada (cuántos argumentos toma, o el primer argumento cuyo tipo no se convierte, con los tipos como los
  escribe el programa), y dónde lo declara el encabezado. En la ambigüedad, las notas son los candidatos
  empatados.

## Cambio

- `NamespaceFunctionSymbol.h`: `cppDeclarationSpelling`, `cppLocation`.
- `CppSemanticFunction.cpp`: los llena.
- `CallAnalyzer.cpp`: `namespaceFunctionCandidateNotes` y `readableTypeKey` en los tres errores.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture `pruebas-tanda-17-diagnosticos` (registro de [diagnósticos](t17-diagnosticos.md)). Sonda con el
compilador del cambio (`A:/temp/nsc-t17-sonda-y4droiwm`):

```
Main.ns:4:12: error: no viable overload of namespace function 'A.f' accepts the argument types
note: candidate: int A::f(int): argument 1 of type 'string literal' does not convert to 'int', declared at .../api.h:2:19
note: candidate: int A::f(double): argument 1 of type 'string literal' does not convert to 'double', declared at .../api.h:2:42
note: candidate: int A::f(int *): argument 1 of type 'string literal' does not convert to 'int*', declared at .../api.h:2:68
```

## Límites

- Las llamadas a métodos de clases C++ conservan su mensaje sin alternativas.
- La razón es la primera conversión que falla; no se explican los rangos de conversión de una ambigüedad.
