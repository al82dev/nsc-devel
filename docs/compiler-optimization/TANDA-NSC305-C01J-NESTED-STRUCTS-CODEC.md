# Tanda NSC305-C01j — Structs anidados (GenericParameter/Constraint) serialize

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización relocatable: structs anidados de la declaración genérica).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeGenericParameter`/`serializeGenericConstraint` (parámetro genérico y cláusula `where` con TypeSyntax referenciadas como árbol). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `ebf038d0`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeGenericParameter`, `serializeGenericConstraint`. |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `serializeGenericParameter` serializa nombre/kind/constantType/range; `serializeGenericConstraint`
  serializa el nombre, las banderas de capability, las TypeSyntax de igualdad/proyección/constraints y
  el argumento sustituido. La deserialización de estos structs es la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
