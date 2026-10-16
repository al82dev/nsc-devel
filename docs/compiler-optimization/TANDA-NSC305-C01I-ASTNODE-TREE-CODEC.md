# Tanda NSC305-C01i — `AstNode` tree codec

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: árbol de AstNode con typeSyntax y hijos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeAstNodeTree`/`deserializeAstNodeTree` (scalars + typeSyntax/throws/typeSyntaxes + first/second/third + children). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `706e1b06`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeAstNodeTree`, `deserializeAstNodeTree`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `serializeAstNodeTree`/`deserializeAstNodeTree` recorren el árbol de nodos (typeSyntax, throwsType,
  typeSyntaxes, first/second/third, children). Los structs anidados genéricos/lifetime/constraints son
  la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
