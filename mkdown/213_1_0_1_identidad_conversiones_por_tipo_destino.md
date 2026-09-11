# Identidad de conversiones por tipo destino / Conversion identity by target type

## Resumen / Summary

NativeSharp rechazaba o colisionaba operadores de conversión que compartían tipo fuente pero producían destinos distintos. El modelo aplicaba a `op_Implicit` y `op_Explicit` la regla de métodos ordinarios —el retorno no participa en la sobrecarga— aunque una conversión se selecciona precisamente por el par fuente/destino.

NativeSharp rejected or collided conversion operators that shared a source type but produced distinct targets. The model applied the ordinary-method rule—return type does not participate in overload identity—to `op_Implicit` and `op_Explicit`, although a conversion is selected by its source/target pair.

## Archivos modificados / Modified files

- `src/Frontend/ProjectMemberModel.cpp`
  - Incorpora el tipo destino a `ClassMemberSymbol::signatureKey` sólo para conversiones de usuario.
  - Includes the target type in `ClassMemberSymbol::signatureKey` only for user conversions.
- `src/Frontend/ProjectOopHirModel.cpp`
  - Incorpora el tipo destino al sufijo del símbolo LLVM/COFF de la conversión.
  - Includes the target type in the conversion's LLVM/COFF symbol suffix.
- `tests/unit/Stage38PropertiesIndexersOperatorsTests.cpp`
  - Añade un contrato con dos conversiones implícitas desde la misma clase hacia `int` y `long`.
  - Adds a contract with two implicit conversions from the same class to `int` and `long`.

## Implementación / Implementation

### Firma lógica / Logical signature

Después de codificar nombre y parámetros, una conversión agrega `-><tipo-destino>` a su `signatureKey`. Los métodos ordinarios conservan exactamente su identidad previa y continúan rechazando sobrecargas que difieren sólo por retorno.

After encoding name and parameters, a conversion appends `-><target-type>` to its `signatureKey`. Ordinary methods preserve their previous identity and continue rejecting overloads that differ only by return type.

### Identidad física / Physical identity

`typeSuffix` agrega `__to__<destino-codificado>` para conversiones. Esto evita que dos miembros lógicamente válidos terminen definiendo la misma función LLVM y el mismo símbolo COFF. NSMETA conserva esa identidad física exacta, por lo que productores y consumidores precompilados resuelven el mismo nombre.

`typeSuffix` appends `__to__<encoded-target>` for conversions. This prevents two logically valid members from defining the same LLVM function and COFF symbol. NSMETA stores that exact physical identity, so precompiled producers and consumers resolve the same name.

## Motivo / Rationale

El tipo destino no se añadió globalmente a firmas de métodos: hacerlo cambiaría toda la regla de sobrecarga y el ABI sin necesidad. La excepción se limita a miembros marcados `isUserConversion`, cuya semántica ya compara explícitamente fuente y destino en `ProjectConversionClassifier`.

The target type was not added globally to method signatures, which would unnecessarily change overload rules and ABI. The exception is limited to `isUserConversion` members, whose selection already compares source and target explicitly in `ProjectConversionClassifier`.

Esta corrección permite que una fachada publique varias conversiones hacia tipos externos (`Mat4`, `Quat`, `Vec2`, `Vec3`, `Vec4`) sin colisión, y deja que NSMETA procese posteriormente su clausura nominal.

This correction lets a facade publish several conversions to external types (`Mat4`, `Quat`, `Vec2`, `Vec3`, `Vec4`) without collision and allows NSMETA to process their nominal closure afterward.

## Validación / Validation

No se ejecutaron pruebas ni baseline durante esta corrección. La prueba directa de Stage 38 y la fachada de Stage 48.2 quedan para la validación acumulativa final.

No tests or baseline were run during this correction. The direct Stage 38 case and Stage 48.2 facade remain for final cumulative validation.

## Commit de referencia / Reference commit

### Título / Title

```text
Distingue conversiones por su tipo destino / Distinguish conversions by target type
```

### Descripción / Description

```text
ES:
- incluye fuente y destino en la identidad lógica de conversiones
- evita colisiones LLVM/COFF entre conversiones con el mismo origen
- conserva sin cambios la sobrecarga de métodos ordinarios

EN:
- include source and target in logical conversion identity
- prevent LLVM/COFF collisions between conversions sharing a source
- preserve ordinary method overload rules unchanged
```
