# Tanda Universal 56: distinguir el retorno genérico en caja

## SCOPE LOCK

| Dimensión | Alcance autorizado |
|---|---|
| Objetivo | Conservar SH41/181 y desempaquetar únicamente el resultado realmente encajonado de `List<bool>.this` usado por SH42/182. |
| Evidencia | El intento de Tanda 55 hizo que `TokenStream.KindAt` cargara `i32` desde otro `i32`; LLVM lo rechazó antes de generar nsc2. |
| Archivo modificable | `selfhost/nsc/CodeGen/LlvmBackend.ns`. |
| Símbolo modificable | Rama de resultado de accesor en `EmitScalarExpression`. |
| Protegido | Pruebas, fixtures, infraestructura, clasificadores globales y cualquier consumidor ajeno a indexadores genéricos. |
| Terminación | SH41/181 pasa; `DumpSymbol` carga el `bool`; nsc0/nsc2 coinciden; 182 pasa. |

## Evidencia diferencial

La ejecución dirigida de SH41 sobre `a7e7085d9` falló con:

```text
Load operand must be a pointer.
%generic.unbox1 = load i32, i32 %generic.unbox
NativeSharp.Compiler.Lexer.TokenStream.KindAt
```

Esto prueba que el tipo semántico declarado no basta para decidir si debe existir una carga. Dos indexadores genéricos cerrados tienen representaciones distintas en el punto de llamada:

- `List<bool>.get_Item` en `DumpSymbol` devuelve un puntero a la caja; el desensamblado anterior mostró `cmp rax, 0` sin carga;
- el indexador de `List<TokenKind>` en `TokenStream.KindAt` ya devuelve `i32`; agregar otra carga produce IR inválido.

## Corrección mínima

La reconstrucción del tipo escalar cerrado se activa sólo cuando `LLVMTypeOf(result)` es exactamente el tipo puntero opaco del contexto. Si el accesor ya devolvió un escalar, se conserva el `SemanticFact` original y `ConvertScalar` mantiene la conducta previa.

La decisión se basa en la representación LLVM concreta del valor recién emitido, no en nombres, ediciones ni reglas globales. No se modifica ningún otro sitio de conversión.

## Complejidad y efectos

- Tiempo: `O(1)` para comparar los handles de tipo; el cierre textual `O(g)` sólo ocurre para un retorno puntero.
- Memoria adicional: `O(1)` fuera de las cadenas ya documentadas en Tanda 55.
- `TokenStream.KindAt` y demás retornos ya escalares conservan su IR.
- `List<bool>.this` obtiene la única carga necesaria.

## Validación dirigida

Después del commit: SH41/181 O0, comparación de `--dump-symbols` y sólo CTest 182. Sin baseline ni batería completa.
