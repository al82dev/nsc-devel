# Tanda NSC305-C01h — `AstNode` scalars codec

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: campos escalares/string de AstNode).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeAstNodeScalars`/`deserializeAstNodeScalars` (identidad + flags del nodo, sin punteros/hijos/tipos). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `a0ecf578`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeAstNodeScalars`, `deserializeAstNodeScalars`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Codec de los campos escalares/string de `AstNode` (kind, rango, textos, token, modifiers,
  genericParameters, lifetime, flags, enums, valores numéricos). El grafo hijos/typeSyntax es la
  siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
