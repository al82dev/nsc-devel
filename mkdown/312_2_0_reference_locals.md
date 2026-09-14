# 312 — Variables locales `ref` / Reference locals

## Resumen / Summary

Esta entrega incorpora alias locales mutables con sintaxis explícita:

```nativesharp
int value = 10;
ref int alias = ref value;
alias += 2; // value == 12
```

This delivery adds mutable local aliases with explicit declaration and initialization syntax.
The alias keeps the logical element type while referring to the original storage.

## Contrato de lenguaje / Language contract

- La forma es `ref T nombre = ref ubicación;`; ambos `ref` son obligatorios.
- `T` debe escribirse expresamente y coincidir exactamente con el tipo de la ubicación.
- La fuente debe ser mutable y direccionable: local, parámetro `ref`, field, elemento de array o
  dereference válido.
- Literales, temporales, properties, ubicaciones readonly y conversiones implícitas se rechazan.
- El alias no posee el objeto ni el storage, no extiende su duración y no admite `const`, `raii`,
  `owned` ni `borrowed`.
- Un alias puede inicializar otro alias y puede reenviarse como argumento `ref` sin perder la
  identidad de la ubicación.
- La captura por lambda se rechaza hasta que los puntos 315–316 puedan demostrar el lifetime.

Both ends spell `ref`, the element type is invariant, and no hidden temporary is permitted. Reads
and writes go through the same source address. Reference locals are non-owning and cannot yet be
captured by a delegate.

## Diseño del compilador / Compiler design

El parser conserva `ref` como palabra contextual: sólo lo interpreta como declaración cuando le
sigue una forma de tipo. El AST marca el declarador como `local:ref`; el modelo de cuerpos conserva
`MethodLocalSymbol::isReference` separadamente de `typeKey`, que sigue siendo el tipo lógico `T`.

El análisis del inicializador reutiliza `ExpressionResult::addressable` y `assignable`, por lo que
no inventa una segunda definición de lvalue. Además exige identidad de tipo en vez de clasificación
de conversión. La captura por lambda recibe un diagnóstico de escape antes de materializar el
entorno.

HIR y MIR transportan `referenceLocal=true` en la declaración. El backend reserva un slot `ptr`,
guarda allí la dirección inicial y reutiliza `LocalValue` más `PointerLoad`:

1. en contexto de dirección carga el `ptr` y lo reenvía;
2. en contexto de valor carga el `ptr` y luego carga el `T` apuntado.

No se agregó operación runtime, ABI ni helper LLVM. This matches the already-certified physical
shape of `ref` parameters and keeps reference rules in the semantic layer.

## Cobertura / Coverage

`tests/unit/Item312ReferenceLocalsTests.cpp` valida en O0:

- alias de local, parámetro `ref`, field de struct, elemento de storage crudo y field de clase;
- lectura, asignación, incremento, asignación compuesta, encadenamiento y reenvío;
- dumps del modelo semántico, HIR y MIR;
- LLVM verificado, enlace y ejecución real;
- rechazo de literal, readonly, tipo distinto, segundo `ref` omitido, `ref var` y captura lambda.

La prueba enfocada pasó 1/1. No se ejecutó baseline completo para este punto; corresponde una sola
vez al cierre de la tanda 6.

## Archivos principales / Main files

- `src/Parser/Parser.cpp`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/ProjectOopHirModel.cpp`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- documentos normativos, mapas de conformidad, estado, changelog y roadmap 2.0.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Implementa variables locales ref / Implement reference locals`

**Descripción / Description**

`Agrega alias locales mutables de tipo exacto, conserva su identidad de dirección desde AST hasta
LLVM y certifica lecturas, escrituras, reenvío y diagnósticos en O0. / Add exact-type mutable local
aliases, preserve address identity from AST through LLVM, and certify reads, writes, forwarding,
and diagnostics at O0.`
