# Tanda NSC305-C01k — Structs anidados deserialize

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: deserialización de structs anidados).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `deserializeGenericParameter`/`deserializeGenericConstraint` (inversos de los serialize C01j). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `0d11f42b`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `deserializeGenericParameter`, `deserializeGenericConstraint`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `deserializeGenericParameter`/`deserializeGenericConstraint` reconstruyen el parámetro y la cláusula
  `where` (con las TypeSyntax referenciadas) en la arena. El codec C01 queda completo a nivel de
  structs; la integración WARM es el siguiente paso.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
