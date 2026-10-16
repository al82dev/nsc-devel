# Tanda NSC305-C01f — `serializeTypeSyntaxTree` (grafo recursivo de tipos)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: árbol de tipos con argumentos y return/error).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeTypeSyntaxTree` (scalars + argumentos ordenados + return/error recursivos). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `818833c9`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeTypeSyntaxTree`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `serializeTypeSyntaxTree` serializa el árbol de tipos (scalars + argumentos + return/error). Los
  modificadores (arrayCount) y constExpression son la capa AstNode.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
