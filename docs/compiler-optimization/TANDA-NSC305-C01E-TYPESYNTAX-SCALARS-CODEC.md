# Tanda NSC305-C01e — `TypeSyntax` scalars codec

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: campos escalares/string de TypeSyntax).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeTypeSyntaxScalars`/`deserializeTypeSyntaxScalars` (identidad + flags/origin del tipo, sin punteros ni recursión). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `da2f0677`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeTypeSyntaxScalars`, `deserializeTypeSyntaxScalars`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Codec de los campos escalares/string de `TypeSyntax` (base, tokens, flags, convenciones, lifetimes,
  tuple names, origin tags). El grafo de argumentos/return/error/modificadores es la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
