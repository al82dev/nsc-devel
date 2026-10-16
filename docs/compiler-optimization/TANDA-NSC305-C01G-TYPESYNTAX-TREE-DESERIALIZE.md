# Tanda NSC305-C01g — `deserializeTypeSyntaxTree` (árbol de tipos → arena)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: deserialización del árbol de tipos).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `deserializeTypeSyntaxTree` (inverso de `serializeTypeSyntaxTree`), reconstruyendo el árbol en la arena. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `0a8fc56a`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `deserializeTypeSyntaxTree`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `deserializeTypeSyntaxTree` lee scalars + argumentos + return/error recursivamente y reconstruye el
  árbol en la arena (tuplas conservan nombre por elemento). FileId del rango queda para el caller.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
