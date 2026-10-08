# Tanda universal 40: CFG válido para ternarios self-host

## Problema confirmado

SH41 produjo LLVM inválido al compilar `SemanticModel.NodeSymbol`:

```text
Basic Block ... NodeSymbol does not have terminator
label %select.else
```

El backend self-host emitía el branch terminal de los dos brazos del ternario y después llamaba a
`ConvertScalar` para ambos valores con el builder aún situado en el bloque `else`. Cuando una
conversión requería instrucciones, intentaba insertarlas después del terminador y corrompía el CFG.

## Cambio

Para cada brazo del ternario:

1. se emite su expresión;
2. se convierte al tipo común mientras el builder sigue en ese brazo;
3. se captura el bloque resultante;
4. se emite el branch a `select.end`;
5. el PHI usa el valor y bloque ya terminados.

El tipo semántico y el tipo LLVM común se resuelven antes de crear los brazos. No se agrega un caso
especial para `NodeSymbol`; se corrige el lowering general de `ConditionalExpression`.

## Archivos

- Modificado: `selfhost/nsc/CodeGen/LlvmBackend.ns`.
- Nuevo: este documento.

## Validación requerida

1. Generar el compilador self-host de primera generación.
2. Hacer que compile el proyecto del compilador en O0.
3. Verificar el módulo LLVM de `SemanticModel.NodeSymbol`.
4. Ejecutar SH41 y después la cadena derivada en la corrida integral final.

